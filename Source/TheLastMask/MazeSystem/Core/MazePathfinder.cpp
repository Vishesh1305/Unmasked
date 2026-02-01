// Copyright The Last Mask Team. All Rights Reserved.

#include "MazePathfinder.h"

/*=============================================================================
    BFS (BREADTH-FIRST SEARCH) ALGORITHM
    
    Why BFS for maze pathfinding?
    - Guarantees shortest path in unweighted graphs
    - Mazes are unweighted (all steps cost the same)
    - Simple to implement and understand
    - Fast enough for game-jam-sized mazes
    
    How it works:
    1. Start at source cell, add to queue
    2. Mark source as visited
    3. While queue not empty:
       a. Dequeue front cell (current)
       b. If current == target, we're done!
       c. For each unvisited walkable neighbor:
          - Mark as visited
          - Record "parent" (where we came from)
          - Add to queue
    4. Reconstruct path by following parents backward
=============================================================================*/

UMazePathfinder::UMazePathfinder()
    : MazeSize(FIntPoint::ZeroValue)
    , CellSize(200.0f)
    , bIsInitialized(false)
{
}

void UMazePathfinder::Initialize(const TArray<FMazeCell>& InCells, FIntPoint InMazeSize, float InCellSize)
{
    CachedCells = InCells;
    MazeSize = InMazeSize;
    CellSize = InCellSize;
    bIsInitialized = (CachedCells.Num() == MazeSize.X * MazeSize.Y);

    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("MazePathfinder: Cell count (%d) doesn't match size (%d x %d = %d)"),
            CachedCells.Num(), MazeSize.X, MazeSize.Y, MazeSize.X * MazeSize.Y);
    }
}

FMazePathResult UMazePathfinder::FindPath(FIntPoint Start, FIntPoint End)
{
    FMazePathResult Result;
    Result.bSuccess = false;
    Result.PathLength = 0;

    // Validation
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("MazePathfinder: Not initialized!"));
        return Result;
    }

    if (!IsValidCell(Start))
    {
        UE_LOG(LogTemp, Warning, TEXT("MazePathfinder: Start position (%d, %d) is not walkable"),
            Start.X, Start.Y);
        return Result;
    }

    if (!IsValidCell(End))
    {
        UE_LOG(LogTemp, Warning, TEXT("MazePathfinder: End position (%d, %d) is not walkable"),
            End.X, End.Y);
        return Result;
    }

    // Early out if start == end
    if (Start == End)
    {
        Result.bSuccess = true;
        Result.PathGridCoordinates.Add(Start);
        Result.PathWorldPositions.Add(GridToWorld(Start));
        Result.PathLength = 1;
        return Result;
    }

    //=========================================================================
    // BFS IMPLEMENTATION
    //=========================================================================

    // Track visited cells (using TSet for O(1) lookup)
    TSet<FIntPoint> Visited;

    // Track parent of each cell for path reconstruction
    // Key = cell, Value = parent (where we came from)
    TMap<FIntPoint, FIntPoint> Parents;

    // BFS queue
    TQueue<FIntPoint> Queue;

    // Initialize with start
    Queue.Enqueue(Start);
    Visited.Add(Start);
    Parents.Add(Start, FIntPoint(-1, -1)); // Start has no parent

    bool bFoundPath = false;

    // BFS loop
    while (!Queue.IsEmpty())
    {
        FIntPoint Current;
        Queue.Dequeue(Current);

        // Did we reach the end?
        if (Current == End)
        {
            bFoundPath = true;
            break;
        }

        // Process all walkable neighbors
        TArray<FIntPoint> Neighbors = GetWalkableNeighbors(Current);
        for (const FIntPoint& Neighbor : Neighbors)
        {
            if (!Visited.Contains(Neighbor))
            {
                Visited.Add(Neighbor);
                Parents.Add(Neighbor, Current);
                Queue.Enqueue(Neighbor);
            }
        }
    }

    // If we didn't find a path, return failure
    if (!bFoundPath)
    {
        UE_LOG(LogTemp, Warning, TEXT("MazePathfinder: No path found from (%d,%d) to (%d,%d)"),
            Start.X, Start.Y, End.X, End.Y);
        return Result;
    }

    //=========================================================================
    // PATH RECONSTRUCTION
    // Walk backwards from End to Start using parent pointers
    //=========================================================================

    TArray<FIntPoint> ReversedPath;
    FIntPoint Current = End;

    while (Current != FIntPoint(-1, -1))
    {
        ReversedPath.Add(Current);
        
        if (FIntPoint* ParentPtr = Parents.Find(Current))
        {
            Current = *ParentPtr;
        }
        else
        {
            break;
        }
    }

    // Reverse to get Start -> End order
    Algo::Reverse(ReversedPath);

    // Populate result
    Result.bSuccess = true;
    Result.PathGridCoordinates = ReversedPath;
    Result.PathLength = ReversedPath.Num();

    // Convert to world positions
    Result.PathWorldPositions.Reserve(ReversedPath.Num());
    for (const FIntPoint& GridPos : ReversedPath)
    {
        Result.PathWorldPositions.Add(GridToWorld(GridPos));
    }

    return Result;
}

FMazePathResult UMazePathfinder::FindPathFromWorld(FVector WorldStart, FIntPoint GridEnd)
{
    FIntPoint GridStart = WorldToGrid(WorldStart);

    // If the world position maps to a wall, find nearest walkable cell
    if (!IsValidCell(GridStart))
    {
        GridStart = FindNearestWalkableCell(GridStart);
        
        if (GridStart == FIntPoint(-1, -1))
        {
            FMazePathResult FailResult;
            FailResult.bSuccess = false;
            return FailResult;
        }
    }

    return FindPath(GridStart, GridEnd);
}

FIntPoint UMazePathfinder::WorldToGrid(FVector WorldPosition) const
{
    // Grid origin is at (0,0,0), cells extend in +X and +Y
    // Cell center is at (X * CellSize + CellSize/2, Y * CellSize + CellSize/2, 0)
    // So to reverse: GridX = Floor(WorldX / CellSize)
    
    const int32 GridX = FMath::FloorToInt(WorldPosition.X / CellSize);
    const int32 GridY = FMath::FloorToInt(WorldPosition.Y / CellSize);
    
    return FIntPoint(GridX, GridY);
}

FVector UMazePathfinder::GridToWorld(FIntPoint GridPosition) const
{
    return FVector(
        GridPosition.X * CellSize + CellSize * 0.5f,
        GridPosition.Y * CellSize + CellSize * 0.5f,
        0.0f
    );
}

bool UMazePathfinder::IsValidCell(FIntPoint GridPosition) const
{
    // Check bounds
    if (GridPosition.X < 0 || GridPosition.X >= MazeSize.X ||
        GridPosition.Y < 0 || GridPosition.Y >= MazeSize.Y)
    {
        return false;
    }

    // Check if floor (walkable)
    const int32 Index = GridToIndex(GridPosition);
    if (Index >= 0 && Index < CachedCells.Num())
    {
        return CachedCells[Index].bIsFloor;
    }

    return false;
}

FIntPoint UMazePathfinder::FindNearestWalkableCell(FIntPoint GridPosition) const
{
    // Expanding square search
    // Check in increasing radius until we find a walkable cell
    
    const int32 MaxRadius = FMath::Max(MazeSize.X, MazeSize.Y);
    
    for (int32 Radius = 0; Radius <= MaxRadius; ++Radius)
    {
        // Check all cells at this radius
        for (int32 DX = -Radius; DX <= Radius; ++DX)
        {
            for (int32 DY = -Radius; DY <= Radius; ++DY)
            {
                // Only check perimeter of this radius (optimization)
                if (FMath::Abs(DX) != Radius && FMath::Abs(DY) != Radius)
                {
                    continue;
                }

                const FIntPoint TestPos(GridPosition.X + DX, GridPosition.Y + DY);
                if (IsValidCell(TestPos))
                {
                    return TestPos;
                }
            }
        }
    }

    // No walkable cell found
    return FIntPoint(-1, -1);
}

int32 UMazePathfinder::GridToIndex(FIntPoint GridPos) const
{
    // Cells are stored row by row: Index = Y * Width + X
    return GridPos.Y * MazeSize.X + GridPos.X;
}

TArray<FIntPoint> UMazePathfinder::GetWalkableNeighbors(FIntPoint GridPos) const
{
    TArray<FIntPoint> Neighbors;
    Neighbors.Reserve(4);

    // Four cardinal directions
    const FIntPoint Offsets[] = {
        FIntPoint(1, 0),   // East
        FIntPoint(-1, 0),  // West
        FIntPoint(0, 1),   // South
        FIntPoint(0, -1)   // North
    };

    for (const FIntPoint& Offset : Offsets)
    {
        const FIntPoint NeighborPos = GridPos + Offset;
        if (IsValidCell(NeighborPos))
        {
            Neighbors.Add(NeighborPos);
        }
    }

    return Neighbors;
}
