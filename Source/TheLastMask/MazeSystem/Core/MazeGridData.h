// Copyright The Last Mask Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MazeTypes.h"
#include "MazeGridData.generated.h"

/*=============================================================================
    UNREAL ENGINE CONCEPTS EXPLAINED:
    
    UDataAsset:
        - A UObject that can be saved as a .uasset file in Content Browser
        - Think of it as a "saved config file" that lives alongside textures,
          materials, etc.
        - Created either manually (Right Click -> Miscellaneous -> Data Asset)
          or programmatically (which our Bake button does)
        - Persists between editor sessions (unlike transient UObjects)
        - Perfect for storing data that needs to survive level loads
    
    Why we need this:
        - The VISUAL maze = baked Static Mesh Actors (permanent geometry)
        - The LOGICAL maze = this Data Asset (grid data for pathfinding)
        - At runtime, we load this data to know which cells are walkable
        - The pathfinder reads from this, not from the mesh actors
    
    RF_Public | RF_Standalone:
        - Object flags used when saving assets
        - RF_Public: visible outside its package
        - RF_Standalone: can exist without being referenced
        - Together they mean "save this as a normal asset"
=============================================================================*/

/**
 * Stores the baked maze grid data for runtime pathfinding.
 * 
 * Created automatically by the "Bake Maze to Level" button.
 * Assign this to the MazeManager's MazeGridData slot.
 * 
 * This asset contains:
 *   - Grid dimensions and cell size
 *   - Generation parameters (for reference/re-baking)
 *   - Complete cell array (floor/wall data + positions)
 */
UCLASS(BlueprintType)
class THELASTMASK_API UMazeGridData : public UDataAsset
{
    GENERATED_BODY()

public:
    //=========================================================================
    // GRID DIMENSIONS
    //=========================================================================

    /** Width of the maze grid */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid")
    int32 SizeX = 0;

    /** Height of the maze grid */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid")
    int32 SizeY = 0;

    /** Size of each cell in world units (centimeters) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid")
    float CellSize = 200.0f;

    /** Height of walls in world units */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid")
    float WallHeight = 300.0f;

    //=========================================================================
    // GENERATION PARAMETERS (for reference / re-baking)
    //=========================================================================

    /** Seed used to generate this maze */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid|Generation")
    int32 Seed = 0;

    /** Algorithm used to generate this maze */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid|Generation")
    EMazeGenerationAlgorithm Algorithm = EMazeGenerationAlgorithm::RecursiveBacktracker;

    //=========================================================================
    // CELL DATA
    //=========================================================================

    /** 
     * Complete array of maze cells.
     * Stored row by row: Index = Y * SizeX + X
     * Each cell knows if it's floor or wall, and its world position.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze Grid")
    TArray<FMazeCell> Cells;

    //=========================================================================
    // UTILITY
    //=========================================================================

    /** Check if this data asset has valid data */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze Grid")
    bool IsValid() const
    {
        return SizeX > 0 && SizeY > 0 && Cells.Num() == SizeX * SizeY;
    }

    /** Get the number of floor (walkable) cells */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze Grid")
    int32 GetFloorCount() const
    {
        int32 Count = 0;
        for (const FMazeCell& Cell : Cells)
        {
            if (Cell.bIsFloor) Count++;
        }
        return Count;
    }

    /** Get the number of wall cells */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Maze Grid")
    int32 GetWallCount() const
    {
        return Cells.Num() - GetFloorCount();
    }
};