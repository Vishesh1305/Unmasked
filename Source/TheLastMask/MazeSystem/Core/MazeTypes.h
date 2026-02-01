// Copyright The Last Mask Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MazeTypes.generated.h"

/*=============================================================================
    UNREAL ENGINE CONCEPTS EXPLAINED (for your reference):
    
    UENUM(BlueprintType):
        - Makes this enum visible to Unreal's reflection system
        - BlueprintType allows it to be used in Blueprints
        - Without this, the enum is just regular C++ (invisible to editor/BP)
    
    UMETA(DisplayName="..."):
        - Controls how the enum value appears in dropdowns/editor
        - Without it, UE uses the raw C++ name
    
    USTRUCT(BlueprintType):
        - Makes this struct visible to reflection
        - Can be used as a variable type in Blueprints
        - MUST have GENERATED_BODY() inside
    
    UPROPERTY(...):
        - Exposes a member variable to the engine
        - EditAnywhere: editable in Details panel on instances AND in Class Defaults
        - BlueprintReadWrite: readable and writable in Blueprint graphs
        - Category="...": groups properties in the Details panel
        - meta=(ClampMin=..., ClampMax=...): editor-enforced value limits
        - meta=(ToolTip="..."): hover text in editor
=============================================================================*/

/**
 * Enum for selecting which maze generation algorithm to use.
 * Each algorithm produces mazes with different characteristics.
 */
UENUM(BlueprintType)
enum class EMazeGenerationAlgorithm : uint8
{
    /** 
     * Creates long, winding corridors with many dead-ends.
     * Best for horror: maximizes "lost" feeling.
     * Uses depth-first search with random neighbor selection.
     */
    RecursiveBacktracker UMETA(DisplayName = "Recursive Backtracker (Horror Recommended)"),
    
    /**
     * Grows maze outward from a random starting point.
     * Creates organic, radial patterns.
     * Good for mazes that feel "grown" rather than carved.
     */
    Prims UMETA(DisplayName = "Prim's Algorithm (Organic Growth)"),
    
    /**
     * Builds maze by randomly connecting regions.
     * Creates uniform, balanced mazes.
     * Neither too winding nor too open.
     */
    Kruskals UMETA(DisplayName = "Kruskal's Algorithm (Balanced)")
};

/**
 * Cardinal directions for maze connectivity.
 * Uses bit flags so a cell can have multiple open directions.
 * Example: A cell open to East and South = 1 | 4 = 5
 */
UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EMazeDirection : uint8
{
    None  = 0       UMETA(Hidden),
    East  = 1 << 0, // Binary: 0001
    North = 1 << 1, // Binary: 0010
    South = 1 << 2, // Binary: 0100
    West  = 1 << 3  // Binary: 1000
};
ENUM_CLASS_FLAGS(EMazeDirection); // Allows bitwise operations like |, &, ~

/**
 * Represents the current pathfinding target based on game state.
 * The maze "brain" switches between these targets dynamically.
 */
UENUM(BlueprintType)
enum class EMazePathTarget : uint8
{
    /** Default: guide player toward the exit */
    Exit UMETA(DisplayName = "Exit"),
    
    /** When exit is discovered but key not owned: guide to key */
    Key UMETA(DisplayName = "Key"),
    
    /** No valid target (error state or game complete) */
    None UMETA(DisplayName = "None")
};

/**
 * Configuration for maze generation.
 * Exposed to editor as a grouped set of parameters.
 */
USTRUCT(BlueprintType)
struct FMazeGenerationConfig
{
    GENERATED_BODY()

    /** Seed for random number generation. Same seed = same maze. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Generation",
        meta = (ToolTip = "Random seed. Same seed always produces the same maze layout."))
    int32 Seed = 12345;

    /** Width of the maze in cells (X axis) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Generation",
        meta = (ClampMin = "5", ClampMax = "101", UIMin = "5", UIMax = "51",
                ToolTip = "Maze width in cells. Odd numbers recommended for clean walls."))
    int32 SizeX = 21;

    /** Height of the maze in cells (Y axis) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Generation",
        meta = (ClampMin = "5", ClampMax = "101", UIMin = "5", UIMax = "51",
                ToolTip = "Maze height in cells. Odd numbers recommended for clean walls."))
    int32 SizeY = 21;

    /** Which algorithm to use for generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Generation",
        meta = (ToolTip = "Algorithm determines maze feel. Backtracker is best for horror."))
    EMazeGenerationAlgorithm Algorithm = EMazeGenerationAlgorithm::RecursiveBacktracker;

    /** Size of each cell in world units (Unreal units = centimeters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Generation",
        meta = (ClampMin = "50.0", ClampMax = "1000.0",
                ToolTip = "Size of each maze cell in centimeters. 200 = 2 meters."))
    float CellSize = 200.0f;

    /** Height of walls in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Generation",
        meta = (ClampMin = "100.0", ClampMax = "1000.0",
                ToolTip = "Height of maze walls in centimeters."))
    float WallHeight = 300.0f;
};

/**
 * Runtime state of the maze pathfinding "brain".
 * Tracks what the player has discovered/collected and mask availability.
 */
USTRUCT(BlueprintType)
struct FMazeGameState
{
    GENERATED_BODY()

    /** Has the player reached/discovered the exit location? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State")
    bool bExitDiscovered = false;

    /** Does the player currently possess the key? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State")
    bool bHasKey = false;

    /** What is the current pathfinding target? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State")
    EMazePathTarget CurrentTarget = EMazePathTarget::Exit;

    /** Is the path currently being visualized? (Mask 1 active) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State")
    bool bPathVisible = false;

    //=========================================================================
    // HOLLOW MASK (MASK 3) STATE
    //
    // The Hollow Mask gives x-ray vision to see the Key through walls.
    // It only unlocks if the player discovers the Exit BEFORE finding the Key.
    // If the player finds the Key first, Mask 3 is permanently locked.
    //
    // State transitions:
    //   Start               -> Locked (default)
    //   Find Exit first     -> UNLOCKED (bHollowMaskUnlocked = true)
    //   Find Key first      -> PERMANENTLY LOCKED (bHollowMaskPermanentlyLocked = true)
    //   Find Key after Exit -> Still unlocked, but no longer needed
    //=========================================================================

    /** Is the Hollow Mask (Mask 3 / x-ray) currently unlocked? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State|Hollow Mask")
    bool bHollowMaskUnlocked = false;

    /** 
     * Is the Hollow Mask permanently locked this run?
     * Set to true if player finds Key before discovering Exit.
     * Once true, cannot be reversed.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|State|Hollow Mask")
    bool bHollowMaskPermanentlyLocked = false;
};

/**
 * A single cell in the maze grid.
 * Stores whether this cell is walkable and its world position.
 */
USTRUCT(BlueprintType)
struct FMazeCell
{
    GENERATED_BODY()

    /** Grid coordinates (0-indexed) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze")
    FIntPoint GridPosition = FIntPoint::ZeroValue;

    /** World position (center of this cell) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze")
    FVector WorldPosition = FVector::ZeroVector;

    /** Is this cell walkable (floor) or blocked (wall)? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze")
    bool bIsFloor = false;

    /** Is this cell part of the current solution path? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze")
    bool bIsOnPath = false;

    /** Index into the instanced mesh array (-1 if not applicable) */
    int32 InstanceIndex = INDEX_NONE;

    FMazeCell() = default;

    FMazeCell(FIntPoint InGridPos, FVector InWorldPos, bool bInIsFloor)
        : GridPosition(InGridPos)
        , WorldPosition(InWorldPos)
        , bIsFloor(bInIsFloor)
        , bIsOnPath(false)
        , InstanceIndex(INDEX_NONE)
    {
    }
};

/*=============================================================================
    HELPER FUNCTIONS
    
    These are inline functions (defined in header) for direction math.
    "FORCEINLINE" is UE's cross-platform inline hint for performance.
=============================================================================*/

/** Get the opposite direction (for bidirectional connections) */
FORCEINLINE EMazeDirection GetOppositeDirection(EMazeDirection Direction)
{
    switch (Direction)
    {
        case EMazeDirection::East:  return EMazeDirection::West;
        case EMazeDirection::West:  return EMazeDirection::East;
        case EMazeDirection::North: return EMazeDirection::South;
        case EMazeDirection::South: return EMazeDirection::North;
        default: return EMazeDirection::None;
    }
}

/** Get X offset for a direction (-1, 0, or 1) */
FORCEINLINE int32 GetDirectionDeltaX(EMazeDirection Direction)
{
    switch (Direction)
    {
        case EMazeDirection::East:  return 1;
        case EMazeDirection::West:  return -1;
        default: return 0;
    }
}

/** Get Y offset for a direction (-1, 0, or 1) */
FORCEINLINE int32 GetDirectionDeltaY(EMazeDirection Direction)
{
    switch (Direction)
    {
        case EMazeDirection::North: return -1;
        case EMazeDirection::South: return 1;
        default: return 0;
    }
}