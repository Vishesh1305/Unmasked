// Copyright The Last Mask Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MazeTypes.h"
#include "MazeGenerator.generated.h"

/*=============================================================================
    UNREAL ENGINE CONCEPTS EXPLAINED:
    
    UObject:
        - Base class for all objects in Unreal's reflection system
        - Gets automatic memory management (garbage collection)
        - Can have UPROPERTY members
        - Cannot be placed in a level (that's AActor)
        - Created with NewObject<T>() not "new T()"
    
    UCLASS():
        - Registers this class with Unreal's reflection system
        - Required for any class deriving from UObject
        - Without this, UE doesn't know the class exists
    
    TArray<TArray<uint8>>:
        - 2D array using Unreal's array type
        - TArray is UE's std::vector equivalent
        - Supports reflection, serialization, GC integration
        - uint8 = unsigned 8-bit integer (0-255)
    
    FRandomStream:
        - UE's seedable random number generator
        - Same seed = same sequence of random numbers
        - Critical for reproducible maze generation
=============================================================================*/

/**
 * Pure C++ maze generation logic.
 * This class generates a 2D grid representing the maze.
 * 
 * Grid format:
 *   - 0 = wall
 *   - 1 = floor (walkable)
 * 
 * The grid uses a "directions" intermediate format internally,
 * then converts to the final floor/wall grid.
 */
UCLASS(BlueprintType)
class THELASTMASK_API UMazeGenerator : public UObject
{
    GENERATED_BODY()

public:
    UMazeGenerator();

    /**
     * Generate a maze grid based on the given configuration.
     * 
     * @param Config - Generation parameters (size, seed, algorithm)
     * @return 2D array where 1=floor, 0=wall
     */
    UFUNCTION(BlueprintCallable, Category = "Maze|Generation")
    TArray<FMazeCell> GenerateMaze(const FMazeGenerationConfig& Config);

    /**
     * Get the raw grid data (for debugging/visualization).
     * Call after GenerateMaze().
     */
    const TArray<TArray<uint8>>& GetRawGrid() const { return CachedGrid; }

    /** Get the size used in last generation */
    FIntPoint GetMazeSize() const { return CachedSize; }

protected:
    //=========================================================================
    // ALGORITHM IMPLEMENTATIONS
    // Each returns a "directions grid" where each cell stores which 
    // directions are open (using EMazeDirection bit flags)
    //=========================================================================

    /** 
     * Recursive Backtracker (Depth-First Search)
     * Creates long winding passages - best for horror
     */
    TArray<TArray<uint8>> GenerateBacktracker(const FIntPoint& Size, FRandomStream& Random);

    /** 
     * Prim's Algorithm
     * Grows organically from a random starting point
     */
    TArray<TArray<uint8>> GeneratePrims(const FIntPoint& Size, FRandomStream& Random);

    /** 
     * Kruskal's Algorithm  
     * Randomly joins cells, creating balanced mazes
     */
    TArray<TArray<uint8>> GenerateKruskals(const FIntPoint& Size, FRandomStream& Random);

    //=========================================================================
    // HELPER FUNCTIONS
    //=========================================================================

    /** Create a 2D array filled with zeros */
    static TArray<TArray<uint8>> CreateZeroedGrid(const FIntPoint& Size);

    /** 
     * Convert directions grid to floor/wall grid
     * Directions grid is half the size (each cell represents a room)
     * Final grid includes walls between rooms
     */
    TArray<TArray<uint8>> DirectionsToFloorWallGrid(
        const TArray<TArray<uint8>>& DirectionsGrid, 
        const FIntPoint& FinalSize);

    /** Shuffle an array using the given random stream */
    template<typename T>
    static void ShuffleArray(TArray<T>& Array, FRandomStream& Random);

private:
    /** Cached grid from last generation */
    TArray<TArray<uint8>> CachedGrid;

    /** Cached size from last generation */
    FIntPoint CachedSize;

    //=========================================================================
    // BACKTRACKER HELPERS
    //=========================================================================
    
    void CarvePassagesFrom(
        int32 X, int32 Y, 
        TArray<TArray<uint8>>& Grid, 
        FRandomStream& Random);

    //=========================================================================
    // PRIM'S HELPERS
    //=========================================================================

    /** Cell states for Prim's algorithm */
    enum class EPrimCellState : uint8
    {
        Out = 0,      // Not yet in maze
        Frontier = 64, // Adjacent to maze, candidate for addition
        In = 128      // Part of the maze
    };

    TArray<TPair<int32, int32>> PrimFrontier;

    void PrimExpandFrontierFrom(int32 X, int32 Y, TArray<TArray<uint8>>& Grid);
    void PrimAddToFrontier(int32 X, int32 Y, TArray<TArray<uint8>>& Grid);
    TArray<TPair<int32, int32>> PrimGetInNeighbors(int32 X, int32 Y, const TArray<TArray<uint8>>& Grid);
    EMazeDirection GetDirectionBetween(const TPair<int32, int32>& From, const TPair<int32, int32>& To);

    //=========================================================================
    // KRUSKAL'S HELPERS
    //=========================================================================

    /** Union-Find data structure for Kruskal's */
    struct FKruskalSet
    {
        FKruskalSet* Parent = nullptr;
        
        FKruskalSet* GetRoot()
        {
            if (Parent) return Parent->GetRoot();
            return this;
        }

        bool IsConnectedTo(FKruskalSet* Other)
        {
            return GetRoot() == Other->GetRoot();
        }

        void ConnectTo(FKruskalSet* Other)
        {
            Other->GetRoot()->Parent = this;
        }
    };

    struct FKruskalEdge
    {
        int32 X;
        int32 Y;
        EMazeDirection Direction;

        FKruskalEdge(int32 InX, int32 InY, EMazeDirection InDir)
            : X(InX), Y(InY), Direction(InDir) {}
    };
};
