// Copyright The Last Mask Team. All Rights Reserved.

#include "MazeManager.h"
#include "Core/MazeGenerator.h"
#include "Core/MazePathfinder.h"
#include "Core/MazeGridData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h" // Required for TActorIterator


#if WITH_EDITOR
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#endif

// Static tag name for identifying baked maze actors
const FName AMazeManager::BakedMazeTag = FName(TEXT("BakedMaze"));

AMazeManager::AMazeManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    /*=========================================================================
        PATH MESH COMPONENT
        
        This is the ONLY HISM component remaining.
        It renders the glowing path overlay when Mask 1 is active.
        
        At most ~50 instances (path length through a 21x21 maze).
        Compare to old system: 500+ instances (entire maze) = lag.
        
        This component has NO collision and is hidden by default.
        It only becomes visible when ShowPath() is called.
    =========================================================================*/
    PathMeshComponent = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PathMeshes"));
    PathMeshComponent->SetupAttachment(RootComponent);
    PathMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PathMeshComponent->SetVisibility(false);

    // Set reasonable defaults for bake config
    GenerationConfig.Seed = 12345;
    GenerationConfig.SizeX = 21;
    GenerationConfig.SizeY = 21;
    GenerationConfig.CellSize = 200.0f;
    GenerationConfig.WallHeight = 300.0f;
    GenerationConfig.Algorithm = EMazeGenerationAlgorithm::RecursiveBacktracker;
}

void AMazeManager::BeginPlay()
{
    Super::BeginPlay();

    /*=========================================================================
        RUNTIME INITIALIZATION
        
        At runtime we do NOT generate the maze.
        We load the pre-baked grid data from the Data Asset.
        The visual maze is already in the level as static mesh actors.
        We only need the grid data for pathfinding.
    =========================================================================*/

    // Create pathfinder
    Pathfinder = NewObject<UMazePathfinder>(this, TEXT("MazePathfinder"));

    // Load maze data from Data Asset
    LoadMazeData();

    // Set initial pathfinding target
    if (ExitActor)
    {
        UpdatePathfindingTarget();
    }
}

void AMazeManager::LoadMazeData()
{
    if (!MazeGridData)
    {
        UE_LOG(LogTemp, Error, TEXT("MazeManager: No MazeGridData assigned! "
            "Did you forget to bake the maze and assign the Data Asset?"));
        return;
    }

    if (!MazeGridData->IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MazeManager: MazeGridData is invalid! "
            "Size: %dx%d, Cells: %d"), 
            MazeGridData->SizeX, MazeGridData->SizeY, MazeGridData->Cells.Num());
        return;
    }

    // Load cell data
    CachedCells = MazeGridData->Cells;
    LoadedMazeSize = FIntPoint(MazeGridData->SizeX, MazeGridData->SizeY);
    LoadedCellSize = MazeGridData->CellSize;

    // Initialize pathfinder with loaded data
    if (Pathfinder)
    {
        Pathfinder->Initialize(CachedCells, LoadedMazeSize, LoadedCellSize);
    }

    // Setup path overlay mesh
    if (FloorMesh && PathMeshComponent)
    {
        PathMeshComponent->SetStaticMesh(FloorMesh);
        if (PathGlowMaterial)
        {
            PathMeshComponent->SetMaterial(0, PathGlowMaterial);
        }
    }

    // Reset game state
    GameState = FMazeGameState();

    // Fire ready event
    OnMazeReady.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("MazeManager: Loaded maze data - %dx%d, %d floors, %d walls"),
        LoadedMazeSize.X, LoadedMazeSize.Y,
        MazeGridData->GetFloorCount(), MazeGridData->GetWallCount());
}

//=============================================================================
// BAKE SYSTEM (Editor-Only)
//
// These functions only compile in editor builds.
// They convert the procedural maze into permanent level geometry.
//=============================================================================

void AMazeManager::BakeMazeToLevel()
{
#if WITH_EDITOR


    if (!FloorMesh || !WallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("BakeMaze: Assign FloorMesh and WallMesh before baking!"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("BakeMaze: Starting bake with seed %d, size %dx%d..."),
        GenerationConfig.Seed, GenerationConfig.SizeX, GenerationConfig.SizeY);

    // Step 1: Generate the maze
    UMazeGenerator* TempGenerator = NewObject<UMazeGenerator>(this);
    TArray<FMazeCell> Cells = TempGenerator->GenerateMaze(GenerationConfig);

    // Step 2: Calculate mesh scales (same logic as old RebuildMeshInstances)
    const FVector FloorMeshSize = FloorMesh->GetBoundingBox().GetSize();
    const FVector WallMeshSize = WallMesh->GetBoundingBox().GetSize();

    const float CellSize = GenerationConfig.CellSize;
    const float WallHeight = GenerationConfig.WallHeight;

    const FVector FloorScale(
        CellSize / FMath::Max(FloorMeshSize.X, 1.0f),
        CellSize / FMath::Max(FloorMeshSize.Y, 1.0f),
        1.0f
    );

    const FVector WallScale(
        CellSize / FMath::Max(WallMeshSize.X, 1.0f),
        CellSize / FMath::Max(WallMeshSize.Y, 1.0f),
        WallHeight / FMath::Max(WallMeshSize.Z, 1.0f)
    );

    const float WallCenterZ = WallHeight * 0.5f;
    const FVector ActorOrigin = GetActorLocation();

    int32 FloorCount = 0;
    int32 WallCount = 0;

    // Step 3: Spawn static mesh actors for each cell
    for (int32 i = 0; i < Cells.Num(); ++i)
    {
        const FMazeCell& Cell = Cells[i];
        
        // Calculate world position (relative to MazeManager + actor origin)
        FVector WorldPos = ActorOrigin + Cell.WorldPosition;

        FActorSpawnParameters SpawnParams;

        AStaticMeshActor* MeshActor = nullptr;

        if (Cell.bIsFloor)
        {
            SpawnParams.Name = *FString::Printf(TEXT("BakedFloor_%d_%d"), 
                Cell.GridPosition.X, Cell.GridPosition.Y);
            
            MeshActor = World->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                FVector(WorldPos.X, WorldPos.Y, 0.0f),
                FRotator::ZeroRotator,
                SpawnParams
            );

            if (MeshActor)
            {
                UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
                MeshComp->SetStaticMesh(FloorMesh);
                MeshComp->SetWorldScale3D(FloorScale);
                if (DefaultFloorMaterial)
                {
                    MeshComp->SetMaterial(0, DefaultFloorMaterial);
                }
                FloorCount++;
            }
        }
        else
        {
            SpawnParams.Name = *FString::Printf(TEXT("BakedWall_%d_%d"), 
                Cell.GridPosition.X, Cell.GridPosition.Y);
            
            MeshActor = World->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                FVector(WorldPos.X, WorldPos.Y, WallCenterZ),
                FRotator::ZeroRotator,
                SpawnParams
            );

            if (MeshActor)
            {
                UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
                MeshComp->SetStaticMesh(WallMesh);
                MeshComp->SetWorldScale3D(WallScale);
                if (DefaultWallMaterial)
                {
                    MeshComp->SetMaterial(0, DefaultWallMaterial);
                }
                WallCount++;
            }
        }

        // Tag and organize
        if (MeshActor)
        {
            MeshActor->Tags.Add(BakedMazeTag);
            MeshActor->SetFolderPath(TEXT("BakedMaze"));
        }
    }

    // Step 4: Spawn outer border walls
    int32 BorderCount = 0;
    const int32 SizeX = GenerationConfig.SizeX;
    const int32 SizeY = GenerationConfig.SizeY;

    // Lambda to spawn a single border wall
    auto SpawnBorderWall = [&](int32 GridX, int32 GridY)
    {
        const FVector LocalPos(
            GridX * CellSize + CellSize * 0.5f,
            GridY * CellSize + CellSize * 0.5f,
            WallCenterZ
        );
        const FVector WorldPos = ActorOrigin + LocalPos;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *FString::Printf(TEXT("BakedBorder_%d_%d"), GridX, GridY);

        AStaticMeshActor* BorderActor = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(),
            WorldPos,
            FRotator::ZeroRotator,
            SpawnParams
        );

        if (BorderActor)
        {
            UStaticMeshComponent* MeshComp = BorderActor->GetStaticMeshComponent();
            MeshComp->SetStaticMesh(WallMesh);
            MeshComp->SetWorldScale3D(WallScale);
            if (DefaultWallMaterial)
            {
                MeshComp->SetMaterial(0, DefaultWallMaterial);
            }
            BorderActor->Tags.Add(BakedMazeTag);
            BorderActor->SetFolderPath(TEXT("BakedMaze/Border"));
            BorderCount++;
        }
    };

    // Bottom row (Y = -1)
    for (int32 X = -1; X <= SizeX; ++X) SpawnBorderWall(X, -1);
    // Top row (Y = SizeY)
    for (int32 X = -1; X <= SizeX; ++X) SpawnBorderWall(X, SizeY);
    // Left column (X = -1), excluding corners
    for (int32 Y = 0; Y < SizeY; ++Y) SpawnBorderWall(-1, Y);
    // Right column (X = SizeX), excluding corners
    for (int32 Y = 0; Y < SizeY; ++Y) SpawnBorderWall(SizeX, Y);
    
    
    const FString PackagePath = TEXT("/Game/Maze/MazeGridData");
    
    UPackage* Package = CreatePackage(*PackagePath);
    Package->FullyLoad();

    UMazeGridData* NewGridData = NewObject<UMazeGridData>(
        Package, 
        UMazeGridData::StaticClass(), 
        TEXT("MazeGridData"), 
        RF_Public | RF_Standalone
    );

    // Populate the Data Asset
    NewGridData->SizeX = GenerationConfig.SizeX;
    NewGridData->SizeY = GenerationConfig.SizeY;
    NewGridData->CellSize = GenerationConfig.CellSize;
    NewGridData->WallHeight = GenerationConfig.WallHeight;
    NewGridData->Seed = GenerationConfig.Seed;
    NewGridData->Algorithm = GenerationConfig.Algorithm;
    NewGridData->Cells = Cells;

    // Mark dirty and save
    NewGridData->MarkPackageDirty();
    
    const FString FilePath = FPackageName::LongPackageNameToFilename(
        PackagePath, FPackageName::GetAssetPackageExtension());
    
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewGridData, *FilePath, SaveArgs);

    // Notify Asset Registry
    FAssetRegistryModule::AssetCreated(NewGridData);

    // Auto-assign to this actor
    MazeGridData = NewGridData;

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("BAKE COMPLETE!"));
    UE_LOG(LogTemp, Warning, TEXT("  Floors: %d"), FloorCount);
    UE_LOG(LogTemp, Warning, TEXT("  Walls:  %d"), WallCount);
    UE_LOG(LogTemp, Warning, TEXT("  Border: %d"), BorderCount);
    UE_LOG(LogTemp, Warning, TEXT("  Total:  %d actors"), FloorCount + WallCount + BorderCount);
    UE_LOG(LogTemp, Warning, TEXT("  Data Asset: %s"), *PackagePath);
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("The MazeGridData has been auto-assigned."));
    UE_LOG(LogTemp, Warning, TEXT("Save your level to keep the baked actors!"));

#else
    UE_LOG(LogTemp, Warning, TEXT("BakeMazeToLevel is editor-only and cannot run in shipping builds."));
#endif
}

void AMazeManager::ClearBakedMaze()
{
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }



    int32 DestroyedCount = 0;
    
    TArray<AActor*> ActorsToDestroy;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(BakedMazeTag))
        {
            ActorsToDestroy.Add(*It);
        }
    }

    for (AActor* Actor : ActorsToDestroy)
    {
        Actor->Destroy();
        DestroyedCount++;
    }

    UE_LOG(LogTemp, Log, TEXT("ClearBakedMaze: Destroyed %d baked maze actors"), DestroyedCount);

#else
    UE_LOG(LogTemp, Warning, TEXT("ClearBakedMaze is editor-only."));
#endif
}

//=============================================================================
// PATH VISUALIZATION (Mask 1 — Path Mask)
//=============================================================================

void AMazeManager::ShowPath(FVector PlayerWorldLocation)
{
    if (!Pathfinder)
    {
        UE_LOG(LogTemp, Warning, TEXT("MazeManager: Pathfinder not initialized"));
        return;
    }

    // Convert world position to local (relative to this actor)
    const FVector LocalPlayerPos = GetActorTransform().InverseTransformPosition(PlayerWorldLocation);

    // Recalculate path from player position to current target
    RecalculatePath(LocalPlayerPos);

    // Make path visible
    ApplyPathVisualization();
    GameState.bPathVisible = true;

    UE_LOG(LogTemp, Log, TEXT("MazeManager: Showing path to %s (%d cells)"),
        GameState.CurrentTarget == EMazePathTarget::Exit ? TEXT("Exit") : TEXT("Key"),
        CurrentPath.PathLength);
}

void AMazeManager::HidePath()
{
    ClearPathVisualization();
    GameState.bPathVisible = false;
}

void AMazeManager::TogglePath(FVector PlayerWorldLocation)
{
    if (GameState.bPathVisible)
    {
        HidePath();
    }
    else
    {
        ShowPath(PlayerWorldLocation);
    }
}

//=============================================================================
// GAME STATE MANAGEMENT
//=============================================================================

void AMazeManager::NotifyExitDiscovered()
{
    if (GameState.bExitDiscovered)
    {
        return; // Already discovered
    }

    GameState.bExitDiscovered = true;
    OnExitDiscovered.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("MazeManager: Exit discovered! HasKey=%s"),
        GameState.bHasKey ? TEXT("true") : TEXT("false"));

    // Update Hollow Mask state (may unlock Mask 3)
    UpdateHollowMaskState();

    // Update pathfinding target
    UpdatePathfindingTarget();
}

void AMazeManager::NotifyKeyCollected()
{
    if (GameState.bHasKey)
    {
        return; // Already have key
    }

    GameState.bHasKey = true;
    OnKeyCollected.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("MazeManager: Key collected! ExitDiscovered=%s"),
        GameState.bExitDiscovered ? TEXT("true") : TEXT("false"));

    // Update Hollow Mask state (may permanently lock Mask 3)
    UpdateHollowMaskState();

    // Update pathfinding target
    UpdatePathfindingTarget();
}

bool AMazeManager::CanExitMaze() const
{
    return GameState.bExitDiscovered && GameState.bHasKey;
}

bool AMazeManager::IsHollowMaskAvailable() const
{
    return GameState.bHollowMaskUnlocked && !GameState.bHollowMaskPermanentlyLocked;
}

void AMazeManager::UpdatePathfindingTarget()
{
    EMazePathTarget OldTarget = GameState.CurrentTarget;

    /*
        Target Priority Logic:
        
        1. Default: target Exit
        2. If Exit discovered AND Key NOT owned: target Key
        3. If Key owned: target Exit (even if Exit discovered)
        
        This ensures:
        - First use of Mask 1 guides to exit
        - After finding exit without key, guides to key
        - After getting key, guides back to exit
    */

    if (GameState.bHasKey)
    {
        GameState.CurrentTarget = EMazePathTarget::Exit;
    }
    else if (GameState.bExitDiscovered)
    {
        GameState.CurrentTarget = EMazePathTarget::Key;
    }
    else
    {
        GameState.CurrentTarget = EMazePathTarget::Exit;
    }

    // Fire event if target changed
    if (GameState.CurrentTarget != OldTarget)
    {
        OnTargetChanged.Broadcast(GameState.CurrentTarget);
        
        UE_LOG(LogTemp, Log, TEXT("MazeManager: Target changed from %s to %s"),
            OldTarget == EMazePathTarget::Exit ? TEXT("Exit") : TEXT("Key"),
            GameState.CurrentTarget == EMazePathTarget::Exit ? TEXT("Exit") : TEXT("Key"));
    }
}

void AMazeManager::UpdateHollowMaskState()
{
    /*=========================================================================
        HOLLOW MASK (MASK 3) STATE LOGIC
        
        The Hollow Mask (x-ray vision to see Key through walls) follows
        strict unlock/lock rules:
        
        UNLOCK CONDITION:
            Player discovers Exit BEFORE finding Key.
            -> "You've seen the exit, now you need to find the key"
            -> Mask 3 helps by showing Key through walls
        
        PERMANENT LOCK CONDITION:
            Player finds Key BEFORE discovering Exit.
            -> Player found the key on their own, no need for x-ray
            -> Mask 3 is never available this run
        
        STATE TRANSITIONS:
            Start       -> Locked (default)
            Find Exit   -> UNLOCKED (if no key yet)
            Find Key first -> PERMANENTLY LOCKED
            
        NOTE: The visual effect (overlay shader) is handled by the art team.
        This function only manages the boolean state.
        The art team should check IsHollowMaskAvailable() before activating
        their shader, OR bind to the OnHollowMaskUnlocked event.
    =========================================================================*/

    // Already permanently locked — nothing to do
    if (GameState.bHollowMaskPermanentlyLocked)
    {
        return;
    }

    // Player found Key BEFORE Exit -> permanently lock Mask 3
    if (GameState.bHasKey && !GameState.bExitDiscovered)
    {
        GameState.bHollowMaskPermanentlyLocked = true;
        UE_LOG(LogTemp, Log, TEXT("MazeManager: Hollow Mask PERMANENTLY LOCKED (key found before exit)"));
        return;
    }

    // Player found Exit BEFORE Key -> unlock Mask 3
    if (GameState.bExitDiscovered && !GameState.bHasKey && !GameState.bHollowMaskUnlocked)
    {
        GameState.bHollowMaskUnlocked = true;
        OnHollowMaskUnlocked.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("MazeManager: Hollow Mask UNLOCKED (exit found, need key)"));
    }
}

//=============================================================================
// PATHFINDING INTERNALS
//=============================================================================

void AMazeManager::RecalculatePath(FVector FromWorldPosition)
{
    if (!Pathfinder)
    {
        return;
    }

    // Get target grid position
    const FIntPoint TargetGrid = GetCurrentTargetGridPosition();
    
    if (TargetGrid == FIntPoint(-1, -1))
    {
        UE_LOG(LogTemp, Warning, TEXT("MazeManager: Invalid target grid position"));
        return;
    }

    // Find path
    CurrentPath = Pathfinder->FindPathFromWorld(FromWorldPosition, TargetGrid);

    // Update path cell set for visualization
    PathCellSet.Empty();
    for (const FIntPoint& GridPos : CurrentPath.PathGridCoordinates)
    {
        PathCellSet.Add(GridPos);
    }

    // Fire event
    if (CurrentPath.bSuccess)
    {
        OnPathUpdated.Broadcast(CurrentPath.PathWorldPositions);
    }
}

void AMazeManager::ApplyPathVisualization()
{
    if (!CurrentPath.bSuccess || CurrentPath.PathGridCoordinates.Num() == 0)
    {
        return;
    }

    if (!PathMeshComponent || !FloorMesh)
    {
        return;
    }

    // Clear old path instances
    PathMeshComponent->ClearInstances();

    // Ensure mesh and material are set
    PathMeshComponent->SetStaticMesh(FloorMesh);
    if (PathGlowMaterial)
    {
        PathMeshComponent->SetMaterial(0, PathGlowMaterial);
    }

    // Match the floor scale so the overlay aligns perfectly
    const FVector FloorMeshSize = FloorMesh->GetBoundingBox().GetSize();
    const float SafeFloorX = FMath::Max(FloorMeshSize.X, 1.0f);
    const float SafeFloorY = FMath::Max(FloorMeshSize.Y, 1.0f);

    const FVector PathScale(
        LoadedCellSize / SafeFloorX,
        LoadedCellSize / SafeFloorY,
        1.0f
    );

    // Add instances for each path cell
    const float PathZOffset = 1.0f; // Slight raise to prevent z-fighting

    for (const FIntPoint& GridPos : CurrentPath.PathGridCoordinates)
    {
        const int32 Index = GridPos.Y * LoadedMazeSize.X + GridPos.X;
        if (Index >= 0 && Index < CachedCells.Num() && CachedCells[Index].bIsFloor)
        {
            FVector LocalPos = CachedCells[Index].WorldPosition;
            LocalPos.Z = PathZOffset;

            const FTransform PathTransform(FRotator::ZeroRotator, LocalPos, PathScale);
            PathMeshComponent->AddInstance(PathTransform);
        }
    }

    // Make path visible
    PathMeshComponent->SetVisibility(true);
}

void AMazeManager::ClearPathVisualization()
{
    if (PathMeshComponent)
    {
        PathMeshComponent->SetVisibility(false);
        PathMeshComponent->ClearInstances();
    }
}

//=============================================================================
// TARGET POSITION HELPERS
//=============================================================================

FIntPoint AMazeManager::GetExitGridPosition() const
{
    return ActorToGridPosition(ExitActor);
}

FIntPoint AMazeManager::GetKeyGridPosition() const
{
    return ActorToGridPosition(KeyActor);
}

FIntPoint AMazeManager::GetCurrentTargetGridPosition() const
{
    switch (GameState.CurrentTarget)
    {
        case EMazePathTarget::Exit:
            return GetExitGridPosition();
        case EMazePathTarget::Key:
            return GetKeyGridPosition();
        default:
            return FIntPoint(-1, -1);
    }
}

FIntPoint AMazeManager::ActorToGridPosition(AActor* Actor) const
{
    if (!Actor || !Pathfinder)
    {
        return FIntPoint(-1, -1);
    }

    // Convert world position to local
    const FVector WorldPos = Actor->GetActorLocation();
    const FVector LocalPos = GetActorTransform().InverseTransformPosition(WorldPos);

    // Convert to grid
    FIntPoint GridPos = Pathfinder->WorldToGrid(LocalPos);

    // Find nearest walkable if it's on a wall
    if (!Pathfinder->IsValidCell(GridPos))
    {
        GridPos = Pathfinder->FindNearestWalkableCell(GridPos);
    }

    return GridPos;
}