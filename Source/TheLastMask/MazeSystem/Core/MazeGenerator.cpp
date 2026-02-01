// Copyright The Last Mask Team. All Rights Reserved.

#include "MazeGenerator.h"



/*=============================================================================
    UNREAL ENGINE CONCEPTS EXPLAINED:
    
    NewObject<T>():
        - The correct way to create UObject-derived instances
        - NEVER use "new" for UObjects - breaks garbage collection
        - Example: UMazeGenerator* Gen = NewObject<UMazeGenerator>(this);
    
    GENERATED_BODY():
        - Macro that expands to reflection boilerplate
        - Must be in every UCLASS/USTRUCT
        - Implements things like StaticClass(), GetClass(), etc.
    
    FRandomStream:
        - Initialize with seed: FRandomStream Random(Seed);
        - Get random int in range: Random.RandRange(Min, Max);
        - Deterministic: same seed = same sequence
=============================================================================*/

UMazeGenerator::UMazeGenerator()
{
    // UObjects don't need explicit initialization usually
    // The constructor is called by NewObject<>()
}

TArray<FMazeCell> UMazeGenerator::GenerateMaze(const FMazeGenerationConfig& Config)
{
    // Create seeded random stream for reproducible results
    FRandomStream Random(Config.Seed);

    // The "directions grid" is smaller - it represents rooms, not cells
    // Final grid will be (2*DirectionsSize - 1) to include walls
    const FIntPoint DirectionsSize((Config.SizeX + 1) / 2, (Config.SizeY + 1) / 2);

    // Generate the directions grid based on selected algorithm
    TArray<TArray<uint8>> DirectionsGrid;
    
    switch (Config.Algorithm)
    {
        case EMazeGenerationAlgorithm::RecursiveBacktracker:
            DirectionsGrid = GenerateBacktracker(DirectionsSize, Random);
            break;
            
        case EMazeGenerationAlgorithm::Prims:
            DirectionsGrid = GeneratePrims(DirectionsSize, Random);
            break;
            
        case EMazeGenerationAlgorithm::Kruskals:
            DirectionsGrid = GenerateKruskals(DirectionsSize, Random);
            break;
            
        default:
            DirectionsGrid = GenerateBacktracker(DirectionsSize, Random);
            break;
    }

    // Convert to floor/wall grid
    const FIntPoint FinalSize(Config.SizeX, Config.SizeY);
    CachedGrid = DirectionsToFloorWallGrid(DirectionsGrid, FinalSize);
    CachedSize = FinalSize;

    // Convert raw grid to FMazeCell array with world positions
    TArray<FMazeCell> Cells;
    Cells.Reserve(FinalSize.X * FinalSize.Y);

    for (int32 Y = 0; Y < FinalSize.Y; ++Y)
    {
        for (int32 X = 0; X < FinalSize.X; ++X)
        {
            const bool bIsFloor = (CachedGrid[Y][X] == 1);
            
            // Calculate world position (center of cell)
            // Grid origin is at actor location, cells extend in +X and +Y
            const FVector WorldPos(
                X * Config.CellSize + Config.CellSize * 0.5f,
                Y * Config.CellSize + Config.CellSize * 0.5f,
                0.0f
            );

            Cells.Add(FMazeCell(FIntPoint(X, Y), WorldPos, bIsFloor));
        }
    }

    int32 FloorCount = 0;
    int32 WallCount = 0;
    for (const FMazeCell& Cell : Cells)
    {
        if (Cell.bIsFloor) FloorCount++;
        else WallCount++;
    }
    UE_LOG(LogTemp, Warning, TEXT("Maze generated: %d floors, %d walls (total %d)"), 
        FloorCount, WallCount, Cells.Num());
    return Cells;
}

//=============================================================================
// RECURSIVE BACKTRACKER IMPLEMENTATION
// 
// Algorithm:
// 1. Start at a random cell, mark it as visited
// 2. While there are unvisited cells:
//    a. If current cell has unvisited neighbors:
//       - Choose random unvisited neighbor
//       - Remove wall between current and neighbor
//       - Move to neighbor, push current to stack
//    b. Else (dead end):
//       - Pop cell from stack, make it current
// 
// This creates long, winding corridors with many dead ends.
//=============================================================================

TArray<TArray<uint8>> UMazeGenerator::GenerateBacktracker(const FIntPoint& Size, FRandomStream& Random)
{
    TArray<TArray<uint8>> Grid = CreateZeroedGrid(Size);

    // Start carving from (0,0)
    CarvePassagesFrom(0, 0, Grid, Random);

    return Grid;
}

void UMazeGenerator::CarvePassagesFrom(int32 X, int32 Y, TArray<TArray<uint8>>& Grid, FRandomStream& Random)
{
    // Get all four directions and shuffle them
    TArray<EMazeDirection> Directions = {
        EMazeDirection::East,
        EMazeDirection::West,
        EMazeDirection::North,
        EMazeDirection::South
    };
    ShuffleArray(Directions, Random);

    // Try each direction
    for (EMazeDirection Dir : Directions)
    {
        const int32 NextX = X + GetDirectionDeltaX(Dir);
        const int32 NextY = Y + GetDirectionDeltaY(Dir);

        // Check bounds
        const bool bInBounds = NextX >= 0 && NextX < Grid[0].Num() &&
                               NextY >= 0 && NextY < Grid.Num();

        // If in bounds and not yet visited (value is 0)
        if (bInBounds && Grid[NextY][NextX] == 0)
        {
            // Carve passage: set direction bits on both cells
            Grid[Y][X] |= static_cast<uint8>(Dir);
            Grid[NextY][NextX] |= static_cast<uint8>(GetOppositeDirection(Dir));

            // Recursively carve from the new cell
            CarvePassagesFrom(NextX, NextY, Grid, Random);
        }
    }
}

//=============================================================================
// PRIM'S ALGORITHM IMPLEMENTATION
//
// Algorithm:
// 1. Start with a grid where all cells are "out" of the maze
// 2. Pick a random cell, mark it "in", add neighbors to "frontier"
// 3. While frontier is not empty:
//    a. Pick random frontier cell
//    b. Find its neighbors that are "in" the maze
//    c. Connect to random "in" neighbor
//    d. Mark frontier cell as "in"
//    e. Add its "out" neighbors to frontier
//
// Creates organic, growing patterns radiating from the start.
//=============================================================================

TArray<TArray<uint8>> UMazeGenerator::GeneratePrims(const FIntPoint& Size, FRandomStream& Random)
{
    TArray<TArray<uint8>> Grid = CreateZeroedGrid(Size);
    PrimFrontier.Empty();

    // Start from a random cell
    const int32 StartX = Random.RandRange(0, Size.X - 1);
    const int32 StartY = Random.RandRange(0, Size.Y - 1);

    PrimExpandFrontierFrom(StartX, StartY, Grid);

    // Process frontier until empty
    while (PrimFrontier.Num() > 0)
    {
        // Pick random frontier cell
        const int32 Index = Random.RandRange(0, PrimFrontier.Num() - 1);
        const TPair<int32, int32> Current = PrimFrontier[Index];
        PrimFrontier.RemoveAt(Index);

        // Get neighbors that are already "in" the maze
        TArray<TPair<int32, int32>> InNeighbors = PrimGetInNeighbors(Current.Key, Current.Value, Grid);
        
        if (InNeighbors.Num() > 0)
        {
            // Connect to random "in" neighbor
            const TPair<int32, int32> Neighbor = InNeighbors[Random.RandRange(0, InNeighbors.Num() - 1)];
            const EMazeDirection Dir = GetDirectionBetween(Current, Neighbor);

            Grid[Current.Value][Current.Key] |= static_cast<uint8>(Dir);
            Grid[Neighbor.Value][Neighbor.Key] |= static_cast<uint8>(GetOppositeDirection(Dir));
        }

        // Expand frontier from this cell
        PrimExpandFrontierFrom(Current.Key, Current.Value, Grid);
    }

    return Grid;
}

void UMazeGenerator::PrimExpandFrontierFrom(int32 X, int32 Y, TArray<TArray<uint8>>& Grid)
{
    // Mark this cell as "in"
    Grid[Y][X] |= static_cast<uint8>(EPrimCellState::In);

    // Add neighbors to frontier
    PrimAddToFrontier(X - 1, Y, Grid);
    PrimAddToFrontier(X + 1, Y, Grid);
    PrimAddToFrontier(X, Y - 1, Grid);
    PrimAddToFrontier(X, Y + 1, Grid);
}

void UMazeGenerator::PrimAddToFrontier(int32 X, int32 Y, TArray<TArray<uint8>>& Grid)
{
    const bool bInBounds = X >= 0 && X < Grid[0].Num() && Y >= 0 && Y < Grid.Num();
    
    if (bInBounds && Grid[Y][X] == 0)
    {
        Grid[Y][X] |= static_cast<uint8>(EPrimCellState::Frontier);
        PrimFrontier.Add(TPair<int32, int32>(X, Y));
    }
}

TArray<TPair<int32, int32>> UMazeGenerator::PrimGetInNeighbors(int32 X, int32 Y, const TArray<TArray<uint8>>& Grid)
{
    TArray<TPair<int32, int32>> Neighbors;
    
    const uint8 InFlag = static_cast<uint8>(EPrimCellState::In);

    if (X > 0 && (Grid[Y][X - 1] & InFlag))
        Neighbors.Add(TPair<int32, int32>(X - 1, Y));
    if (X < Grid[0].Num() - 1 && (Grid[Y][X + 1] & InFlag))
        Neighbors.Add(TPair<int32, int32>(X + 1, Y));
    if (Y > 0 && (Grid[Y - 1][X] & InFlag))
        Neighbors.Add(TPair<int32, int32>(X, Y - 1));
    if (Y < Grid.Num() - 1 && (Grid[Y + 1][X] & InFlag))
        Neighbors.Add(TPair<int32, int32>(X, Y + 1));

    return Neighbors;
}

EMazeDirection UMazeGenerator::GetDirectionBetween(const TPair<int32, int32>& From, const TPair<int32, int32>& To)
{
    if (To.Key > From.Key) return EMazeDirection::East;
    if (To.Key < From.Key) return EMazeDirection::West;
    if (To.Value > From.Value) return EMazeDirection::South;
    if (To.Value < From.Value) return EMazeDirection::North;
    return EMazeDirection::None;
}

//=============================================================================
// KRUSKAL'S ALGORITHM IMPLEMENTATION
//
// Algorithm:
// 1. Create a set for each cell (initially each cell is its own set)
// 2. Create a list of all possible edges (walls between adjacent cells)
// 3. Shuffle the edge list
// 4. For each edge:
//    a. If the two cells belong to different sets:
//       - Remove the wall (add passage)
//       - Union the two sets
//
// Creates balanced, uniform mazes with no particular bias.
//=============================================================================

TArray<TArray<uint8>> UMazeGenerator::GenerateKruskals(const FIntPoint& Size, FRandomStream& Random)
{
    TArray<TArray<uint8>> Grid = CreateZeroedGrid(Size);

    // Create a set for each cell using 2D array of unique pointers
    TArray<TArray<TUniquePtr<FKruskalSet>>> Sets;
    Sets.SetNum(Size.Y);
    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        Sets[Y].SetNum(Size.X);
        for (int32 X = 0; X < Size.X; ++X)
        {
            Sets[Y][X] = MakeUnique<FKruskalSet>();
        }
    }

    // Create all possible edges
    TArray<FKruskalEdge> Edges;
    Edges.Reserve(2 * Size.X * Size.Y); // Rough estimate

    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        for (int32 X = 0; X < Size.X; ++X)
        {
            if (X > 0)
                Edges.Add(FKruskalEdge(X, Y, EMazeDirection::West));
            if (Y > 0)
                Edges.Add(FKruskalEdge(X, Y, EMazeDirection::North));
        }
    }

    // Shuffle edges
    ShuffleArray(Edges, Random);

    // Process edges
    for (const FKruskalEdge& Edge : Edges)
    {
        const int32 NextX = Edge.X + GetDirectionDeltaX(Edge.Direction);
        const int32 NextY = Edge.Y + GetDirectionDeltaY(Edge.Direction);

        FKruskalSet* CurrentSet = Sets[Edge.Y][Edge.X].Get();
        FKruskalSet* NextSet = Sets[NextY][NextX].Get();

        // If not already connected, connect them
        if (!CurrentSet->IsConnectedTo(NextSet))
        {
            CurrentSet->ConnectTo(NextSet);

            // Carve passage
            Grid[Edge.Y][Edge.X] |= static_cast<uint8>(Edge.Direction);
            Grid[NextY][NextX] |= static_cast<uint8>(GetOppositeDirection(Edge.Direction));
        }
    }

    return Grid;
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

TArray<TArray<uint8>> UMazeGenerator::CreateZeroedGrid(const FIntPoint& Size)
{
    TArray<TArray<uint8>> Grid;
    Grid.SetNum(Size.Y);
    
    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        Grid[Y].SetNumZeroed(Size.X);
    }
    
    return Grid;
}

TArray<TArray<uint8>> UMazeGenerator::DirectionsToFloorWallGrid(
    const TArray<TArray<uint8>>& DirectionsGrid,
    const FIntPoint& FinalSize)
{
        // Start with all walls (0)
    TArray<TArray<uint8>> Grid = CreateZeroedGrid(FinalSize);

    const int32 DirSizeY = DirectionsGrid.Num();
    const int32 DirSizeX = DirSizeY > 0 ? DirectionsGrid[0].Num() : 0;

    // Debug: Print the first few direction values
    UE_LOG(LogTemp, Warning, TEXT("DirectionsGrid size: %dx%d, FinalGrid size: %dx%d"), 
        DirSizeX, DirSizeY, FinalSize.X, FinalSize.Y);
    
    if (DirSizeY > 0 && DirSizeX > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sample directions - [0,0]=%d, [0,1]=%d, [1,0]=%d"), 
            DirectionsGrid[0][0], 
            DirSizeX > 1 ? DirectionsGrid[0][1] : -1,
            DirSizeY > 1 ? DirectionsGrid[1][0] : -1);
    }

    for (int32 Y = 0; Y < DirSizeY; ++Y)
    {
        for (int32 X = 0; X < DirSizeX; ++X)
        {
            // Each room in directions grid maps to even coordinates in final grid
            const int32 FinalX = X * 2;
            const int32 FinalY = Y * 2;

            // Bounds check for final grid
            if (FinalX >= FinalSize.X || FinalY >= FinalSize.Y)
                continue;

            // The room itself is always floor
            Grid[FinalY][FinalX] = 1;

            const uint8 Directions = DirectionsGrid[Y][X];

            // East = 1: If this room connects East, carve passage to the right
            if ((Directions & 1) && (FinalX + 1 < FinalSize.X))
            {
                Grid[FinalY][FinalX + 1] = 1;
            }

            // North = 2: If this room connects North, carve passage upward (Y-1 in grid terms)
            if ((Directions & 2) && (FinalY - 1 >= 0))
            {
                Grid[FinalY - 1][FinalX] = 1;
            }

            // South = 4: If this room connects South, carve passage downward
            if ((Directions & 4) && (FinalY + 1 < FinalSize.Y))
            {
                Grid[FinalY + 1][FinalX] = 1;
            }

            // West = 8: If this room connects West, carve passage to the left
            if ((Directions & 8) && (FinalX - 1 >= 0))
            {
                Grid[FinalY][FinalX - 1] = 1;
            }
        }
    }

    return Grid;
}

template<typename T>
void UMazeGenerator::ShuffleArray(TArray<T>& Array, FRandomStream& Random)
{
    const int32 LastIndex = Array.Num() - 1;
    for (int32 i = 0; i <= LastIndex; ++i)
    {
        const int32 SwapIndex = Random.RandRange(i, LastIndex);
        if (i != SwapIndex)
        {
            Array.Swap(i, SwapIndex);
        }
    }
}

// Explicit template instantiation for the types we use
template void UMazeGenerator::ShuffleArray<EMazeDirection>(TArray<EMazeDirection>&, FRandomStream&);
template void UMazeGenerator::ShuffleArray<UMazeGenerator::FKruskalEdge>(TArray<FKruskalEdge>&, FRandomStream&);
