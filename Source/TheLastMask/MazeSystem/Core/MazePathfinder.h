// Copyright The Last Mask Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MazeTypes.h"
#include "MazePathfinder.generated.h"

/*=============================================================================
    UNREAL ENGINE CONCEPTS EXPLAINED:
    
    TQueue<T>:
        - UE's FIFO queue implementation
        - Used in BFS (Breadth-First Search) algorithm
        - Enqueue() adds to back, Dequeue() removes from front
    
    TMap<K, V>:
        - UE's hash map (like std::unordered_map)
        - TMap<FIntPoint, FIntPoint> maps grid positions
        - Used here to track "parent" cells for path reconstruction
    
    INDEX_NONE:
        - UE constant equal to -1
        - Convention for "invalid index" or "not found"
=============================================================================*/

/**
 * Result of a pathfinding operation.
 * Contains both success status and the actual path.
 */
USTRUCT(BlueprintType)
struct FMazePathResult
{
    GENERATED_BODY()

    /** Did we find a valid path? */
    UPROPERTY(BlueprintReadOnly, Category = "Maze|Pathfinding")
    bool bSuccess = false;

    /** 
     * The path as a series of grid coordinates.
     * First element is the start, last element is the end.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Maze|Pathfinding")
    TArray<FIntPoint> PathGridCoordinates;

    /** 
     * The path as world positions (center of each cell).
     * Use this for visualization.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Maze|Pathfinding")
    TArray<FVector> PathWorldPositions;

    /** Length of the path (number of cells) */
    UPROPERTY(BlueprintReadOnly, Category = "Maze|Pathfinding")
    int32 PathLength = 0;
};

/**
 * Handles pathfinding through the maze using BFS.
 * 
 * BFS (Breadth-First Search) guarantees the shortest path
 * in an unweighted grid, which is exactly what we need.
 */
UCLASS(BlueprintType)
class THELASTMASK_API UMazePathfinder : public UObject
{
    GENERATED_BODY()

public:
    UMazePathfinder();

    /**
     * Initialize the pathfinder with maze data.
     * Must be called before FindPath().
     * 
     * @param InCells - Array of maze cells from the generator
     * @param InMazeSize - Dimensions of the maze grid
     * @param InCellSize - World size of each cell in centimeters
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    void Initialize(const TArray<FMazeCell>& InCells, FIntPoint InMazeSize, float InCellSize);

    /**
     * Find path between two grid coordinates.
     * Uses BFS to guarantee shortest path.
     * 
     * @param Start - Starting grid position
     * @param End - Target grid position
     * @return Path result with success flag and path data
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    FMazePathResult FindPath(FIntPoint Start, FIntPoint End);

    /**
     * Find path from a world position to a grid coordinate.
     * Converts world position to nearest walkable grid cell.
     * 
     * @param WorldStart - Starting position in world space
     * @param GridEnd - Target grid position
     * @return Path result
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    FMazePathResult FindPathFromWorld(FVector WorldStart, FIntPoint GridEnd);

    /**
     * Convert a world position to the nearest grid coordinate.
     * 
     * @param WorldPosition - Position in world space
     * @return Grid coordinate (may be out of bounds - check IsValidCell)
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    FIntPoint WorldToGrid(FVector WorldPosition) const;

    /**
     * Convert a grid coordinate to world position (cell center).
     * 
     * @param GridPosition - Grid coordinate
     * @return World position at center of cell
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    FVector GridToWorld(FIntPoint GridPosition) const;

    /**
     * Check if a grid coordinate is valid and walkable.
     * 
     * @param GridPosition - Grid coordinate to check
     * @return True if within bounds and is a floor cell
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    bool IsValidCell(FIntPoint GridPosition) const;

    /**
     * Find the nearest walkable cell to a grid position.
     * Useful when a world position maps to a wall.
     * 
     * @param GridPosition - Starting position (might be a wall)
     * @return Nearest floor cell, or (-1,-1) if none found
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Pathfinding")
    FIntPoint FindNearestWalkableCell(FIntPoint GridPosition) const;

protected:
    /** Get the 1D index into CachedCells from 2D grid position */
    int32 GridToIndex(FIntPoint GridPos) const;

    /** Get neighbors of a cell that are walkable */
    TArray<FIntPoint> GetWalkableNeighbors(FIntPoint GridPos) const;

private:
    /** Cached reference to maze cells */
    TArray<FMazeCell> CachedCells;

    /** Maze dimensions */
    FIntPoint MazeSize;

    /** Size of each cell in world units */
    float CellSize;

    /** Is the pathfinder initialized with valid data? */
    bool bIsInitialized;
};
