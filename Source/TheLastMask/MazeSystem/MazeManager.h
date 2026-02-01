// Copyright The Last Mask Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/MazePathfinder.h"
#include "GameFramework/Actor.h"
#include "Core/MazeTypes.h"
#include "MazeManager.generated.h"

/*=============================================================================
    UNREAL ENGINE CONCEPTS EXPLAINED:
    
    AActor:
        - Base class for anything that can exist in a level
        - Has Transform (location, rotation, scale)
        - Has a lifecycle: Construction -> BeginPlay -> Tick -> EndPlay
        - Contains Components (UActorComponent)
    
    UCLASS(Blueprintable):
        - Blueprintable: can create Blueprint subclass of this C++ class
        - This lets artists make BP_MazeManager with custom meshes
    
    TObjectPtr<T>:
        - UE5's smart pointer for UObject references
        - Replaces raw UObject* for better safety and debugging
        - Automatically nulled if object is garbage collected
    
    UPROPERTY(EditInstanceOnly):
        - Editable on instances in level but NOT in Class Defaults
        - Good for "which specific actor is the exit" references
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE:
        - Creates an event that Blueprints can bind to
        - "Multicast" = multiple listeners allowed
        - "Dynamic" = works with UE's reflection (Blueprints)
        - Use: OnPathUpdated.Broadcast() to fire event
    
    CallInEditor:
        - Makes a UFUNCTION appear as a button in the Details panel
        - Only works in editor, not at runtime
        - Perfect for one-time tools like "Bake Maze"
    
    UDataAsset:
        - A UObject that can be saved as a .uasset file
        - Lives in Content Browser alongside textures, materials, etc.
        - We use it to store the maze grid for runtime pathfinding
        - The baked static meshes are the VISUAL maze
        - The Data Asset is the LOGICAL maze (for pathfinding)
=============================================================================*/

class UMazeGenerator;
class UMazePathfinder;
class UMazeGridData;
class UHierarchicalInstancedStaticMeshComponent;

// Delegate declarations for Blueprint-bindable events
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMazeGenerated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPathUpdated, const TArray<FVector>&, PathWorldPositions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExitDiscovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnKeyCollected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetChanged, EMazePathTarget, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHollowMaskUnlocked);

/**
 * Main maze manager actor — PRODUCTION VERSION.
 * 
 * This actor no longer generates or renders the maze in the editor.
 * The maze geometry is "baked" into the level as static mesh actors,
 * and pathfinding data is stored in a Data Asset.
 * 
 * WORKFLOW:
 *   PRE-PRODUCTION (done):
 *     1. Used HISM preview to iterate on maze layout
 *     2. Art team finalized the maze
 * 
 *   BAKING (one-time):
 *     1. Place this actor in the level
 *     2. Set FloorMesh, WallMesh, and GenerationConfig
 *     3. Click "Bake Maze to Level" button
 *     4. Static mesh actors appear (artists work on these)
 *     5. MazeGridData asset is created (pathfinding brain)
 *     6. Assign the Data Asset to the MazeGridData slot
 * 
 *   PRODUCTION (now):
 *     1. Artists place assets on baked geometry (zero lag)
 *     2. At runtime, pathfinder loads from Data Asset
 *     3. Path visualization uses lightweight HISM overlay (~50 cells)
 *     4. All game logic works exactly as before
 * 
 * Setup:
 *   1. Create a Blueprint subclass (BP_MazeManager)
 *   2. Assign MazeGridData (from baking)
 *   3. Assign FloorMesh (for path overlay)
 *   4. Place in level
 *   5. Set ExitActor and KeyActor references
 *   6. Wire up trigger volumes and input
 */
UCLASS(Blueprintable, ClassGroup = "Maze", meta = (BlueprintSpawnableComponent))
class THELASTMASK_API AMazeManager : public AActor
{
    GENERATED_BODY()

public:
    AMazeManager();

    //=========================================================================
    // BAKED DATA (Production Mode)
    //=========================================================================

    /**
     * The baked maze grid data.
     * Assign the Data Asset created by "Bake Maze to Level".
     * At runtime, pathfinding loads from this instead of generating.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Data",
        meta = (ToolTip = "Drag your MazeGridData asset here (created by Bake Maze button)"))
    TObjectPtr<UMazeGridData> MazeGridData;

    //=========================================================================
    // BAKE CONFIGURATION (Only used during baking, not at runtime)
    //=========================================================================

    /** Maze generation settings — only used when baking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Bake Settings",
        meta = (ShowOnlyInnerProperties))
    FMazeGenerationConfig GenerationConfig;

    /** Static mesh for floor cells (used during baking AND for path overlay at runtime) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Bake Settings",
        meta = (ToolTip = "Mesh for walkable floor cells"))
    TObjectPtr<UStaticMesh> FloorMesh;

    /** Static mesh for wall cells (used during baking only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Bake Settings",
        meta = (ToolTip = "Mesh for wall cells"))
    TObjectPtr<UStaticMesh> WallMesh;

    /** Default floor material (applied to baked floor actors) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Bake Settings",
        meta = (ToolTip = "Material for baked floor meshes"))
    TObjectPtr<UMaterialInterface> DefaultFloorMaterial;

    /** Default wall material (applied to baked wall actors) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Bake Settings",
        meta = (ToolTip = "Material for baked wall meshes"))
    TObjectPtr<UMaterialInterface> DefaultWallMaterial;

    //=========================================================================
    // VISUALIZATION (Runtime only)
    //=========================================================================

    /** 
     * Glowing material for path visualization (Mask 1 effect).
     * Applied to PathMeshComponent when path is shown.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visualization",
        meta = (ToolTip = "Glowing material for path visualization (Mask 1 / Path Mask effect)"))
    TObjectPtr<UMaterialInterface> PathGlowMaterial;

    //=========================================================================
    // ACTOR REFERENCES (Set in Editor per-instance)
    //=========================================================================

    /** 
     * Reference to the Exit actor in the level.
     * Set this in the Details panel to your BP_Exit instance.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Maze|Targets",
        meta = (ToolTip = "Drag your Exit actor from the level here"))
    TObjectPtr<AActor> ExitActor;

    /** 
     * Reference to the Key actor in the level.
     * Set this in the Details panel to your BP_Key instance.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Maze|Targets",
        meta = (ToolTip = "Drag your Key actor from the level here"))
    TObjectPtr<AActor> KeyActor;

    //=========================================================================
    // RUNTIME STATE (Read-only in Editor)
    //=========================================================================

    /** Current game state */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State")
    FMazeGameState GameState;

    //=========================================================================
    // EVENTS (Blueprint-bindable)
    //=========================================================================

    /** Fired when maze data is loaded and pathfinder is ready */
    UPROPERTY(BlueprintAssignable, Category = "Maze|Events")
    FOnMazeGenerated OnMazeReady;

    /** Fired when path is recalculated (provides world positions) */
    UPROPERTY(BlueprintAssignable, Category = "Maze|Events")
    FOnPathUpdated OnPathUpdated;

    /** Fired when player discovers the exit */
    UPROPERTY(BlueprintAssignable, Category = "Maze|Events")
    FOnExitDiscovered OnExitDiscovered;

    /** Fired when player collects the key */
    UPROPERTY(BlueprintAssignable, Category = "Maze|Events")
    FOnKeyCollected OnKeyCollected;

    /** Fired when pathfinding target changes */
    UPROPERTY(BlueprintAssignable, Category = "Maze|Events")
    FOnTargetChanged OnTargetChanged;

    /** 
     * Fired when the Hollow Mask (Mask 3) becomes available.
     * Art team: bind to this to enable the x-ray overlay shader.
     */
    UPROPERTY(BlueprintAssignable, Category = "Maze|Events")
    FOnHollowMaskUnlocked OnHollowMaskUnlocked;

    //=========================================================================
    // EDITOR TOOLS (Baking)
    //=========================================================================

    /**
     * BAKE MAZE TO LEVEL.
     * 
     * This is a one-time editor operation that:
     *   1. Generates the maze using current settings
     *   2. Spawns individual Static Mesh Actors for every cell
     *   3. Creates a MazeGridData asset in Content/Maze/
     *   4. Organizes actors under a folder in World Outliner
     * 
     * After baking:
     *   - Static meshes = permanent level geometry (zero lag)
     *   - Data Asset = pathfinding brain (assign to MazeGridData slot)
     *   - Artists can freely place assets on baked geometry
     * 
     * NOTE: This creates NEW actors. Click "Clear Baked Maze" before re-baking.
     */
    UFUNCTION(CallInEditor, Category = "Maze|Bake Tools",
        meta = (DisplayPriority = 0, 
                ToolTip = "Generate maze and spawn static mesh actors. One-time operation."))
    void BakeMazeToLevel();

    /**
     * Delete all previously baked maze actors from the level.
     * Use before re-baking to avoid duplicates.
     */
    UFUNCTION(CallInEditor, Category = "Maze|Bake Tools",
        meta = (DisplayPriority = 1,
                ToolTip = "Remove all baked maze actors (tagged 'BakedMaze')"))
    void ClearBakedMaze();

    //=========================================================================
    // PUBLIC API (Call from Blueprints or C++)
    //=========================================================================

    /**
     * Show the path from player's current position to current target.
     * This is the Mask 1 (Path Mask) ability.
     * 
     * @param PlayerWorldLocation - Current player position in world space
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Visualization",
        meta = (ToolTip = "Activate Mask 1: reveal path to current target"))
    void ShowPath(FVector PlayerWorldLocation);

    /**
     * Hide the path visualization.
     * Call when Mask 1 effect ends.
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Visualization",
        meta = (ToolTip = "Deactivate Mask 1: hide path"))
    void HidePath();

    /**
     * Toggle path visibility.
     * Convenience function for input binding.
     * 
     * @param PlayerWorldLocation - Current player position
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Visualization")
    void TogglePath(FVector PlayerWorldLocation);

    /**
     * Notify the maze that the player has discovered the exit.
     * Call this from a trigger volume near the exit.
     * 
     * This will:
     *   - Switch pathfinding target to Key (if player doesn't have it)
     *   - Unlock Hollow Mask (Mask 3) if Key hasn't been found yet
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|GameState",
        meta = (ToolTip = "Call when player reaches exit area (from trigger)"))
    void NotifyExitDiscovered();

    /**
     * Notify the maze that the player has collected the key.
     * Call this from your Key pickup logic.
     * 
     * This will:
     *   - Switch pathfinding target back to Exit
     *   - Permanently lock Hollow Mask if Exit wasn't discovered yet
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|GameState",
        meta = (ToolTip = "Call when player picks up the key"))
    void NotifyKeyCollected();

    /**
     * Check if the game should end (exit discovered AND has key).
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze|GameState")
    bool CanExitMaze() const;

    /**
     * Check if the Hollow Mask (Mask 3 / X-Ray) is available.
     * Art team: use this to check before activating the overlay shader.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze|GameState")
    bool IsHollowMaskAvailable() const;

    /**
     * Get the grid position of the exit.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze|Targets")
    FIntPoint GetExitGridPosition() const;

    /**
     * Get the grid position of the key.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze|Targets")
    FIntPoint GetKeyGridPosition() const;

    /**
     * Get the grid position for current target (exit or key based on state).
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze|Targets")
    FIntPoint GetCurrentTargetGridPosition() const;

protected:
    //=========================================================================
    // ACTOR LIFECYCLE
    //=========================================================================

    virtual void BeginPlay() override;

    // NOTE: OnConstruction is intentionally NOT overridden.
    // The baked maze is static geometry — no editor preview needed.
    // This eliminates ALL editor lag from clicking the actor.

    //=========================================================================
    // INTERNAL METHODS
    //=========================================================================

    /** 
     * Load maze data from the assigned Data Asset and initialize pathfinder.
     * Called in BeginPlay. No maze generation happens here.
     */
    void LoadMazeData();

    /** Update which target (Exit/Key) the pathfinder should guide toward */
    void UpdatePathfindingTarget();

    /** Update Hollow Mask availability based on game state */
    void UpdateHollowMaskState();

    /** Recalculate path from given position to current target */
    void RecalculatePath(FVector FromWorldPosition);

    /** Apply glow material to path cells */
    void ApplyPathVisualization();

    /** Remove glow from all cells */
    void ClearPathVisualization();

    /** Helper: Convert actor's world position to grid position */
    FIntPoint ActorToGridPosition(AActor* Actor) const;

private:
    //=========================================================================
    // INTERNAL OBJECTS
    //=========================================================================

    /** Pathfinding logic (created at runtime) */
    UPROPERTY()
    TObjectPtr<UMazePathfinder> Pathfinder;

    //=========================================================================
    // PATH VISUALIZATION COMPONENT
    // 
    // This is the ONLY HISM left. It renders ~50 glowing floor cells
    // when the path is active. 50 instances = no lag.
    // Compare to pre-bake: 500+ instances = massive lag.
    //=========================================================================

    /** 
     * Instanced mesh for path cells (overlaid on baked floor).
     * Uses glowing material when Mask 1 is active.
     * Only ~50 instances at a time (path length), not 500+ (entire maze).
     */
    UPROPERTY(VisibleAnywhere, Category = "Maze|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PathMeshComponent;

    //=========================================================================
    // CACHED DATA (loaded from Data Asset at runtime)
    //=========================================================================

    /** All cells from the Data Asset */
    TArray<FMazeCell> CachedCells;

    /** Maze dimensions (from Data Asset) */
    FIntPoint LoadedMazeSize = FIntPoint::ZeroValue;

    /** Cell size (from Data Asset) */
    float LoadedCellSize = 200.0f;

    /** Current computed path */
    FMazePathResult CurrentPath;

    /** Grid positions that are on the current path (for quick lookup) */
    TSet<FIntPoint> PathCellSet;

    //=========================================================================
    // BAKE TAG (for finding/deleting baked actors)
    //=========================================================================

    /** Tag applied to all baked maze actors so they can be found/deleted */
    static const FName BakedMazeTag;
};