<p align="center">
  <img src="https://img.shields.io/badge/Unreal%20Engine-5.5-blue?style=for-the-badge&logo=unrealengine" alt="Unreal Engine 5.5"/>
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus" alt="C++17"/>
  <img src="https://img.shields.io/badge/Global%20Game%20Jam-2026-red?style=for-the-badge" alt="GGJ 2026"/>
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="MIT License"/>
</p>

<h1 align="center">🎭 THE LAST MASK</h1>

<p align="center">
  <i>A first-person psychological horror puzzle game about irreversible choices</i>
</p>

<p align="center">
  <b>Global Game Jam 2026</b>
</p>

---

## 🌑 About The Game

**The Last Mask** is a first-person psychological horror experience set within an ever-shifting maze. You possess ancient masks — each granting a unique power, but each can only be used **once**. Choose wisely, for every decision is permanent.

> *"In the maze of the mind, the path you don't take haunts you more than the one you do."*

### 🎮 Core Mechanics

| Mask | Ability | Strategic Use |
|------|---------|---------------|
| 🛤️ **Path Mask** | Reveals the route to your current objective | Navigate to the Exit or Key |
| 👁️ **Hollow Mask** | X-ray vision to see the Key through walls | Only unlocks if you find the Exit first |
| ❓ **???** | *Unknown* | *Discover in-game* |

---

## 🏗️ Technical Architecture

This project features a **custom C++ maze system** built from scratch for Unreal Engine 5, demonstrating clean architecture, efficient pathfinding, and seamless Blueprint integration.

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        MAZE MANAGER                             │
│              (Central orchestrator & game state)                │
├─────────────────────────────────────────────────────────────────┤
│                              │                                  │
│    ┌─────────────────┐       │       ┌─────────────────┐        │
│    │  MazeGenerator  │       │       │  MazePathfinder │        │
│    │                 │       │       │                 │        │
│    │ • Recursive     │       │       │ • BFS Algorithm │        │
│    │   Backtracker   │       │       │ • Dynamic Target│        │
│    │ • Prim's        │       │       │ • World ↔ Grid  │        │
│    │ • Kruskal's     │       │       │   Conversion    │        │
│    └────────┬────────┘       │       └────────┬────────┘        │
│             │                │                │                 │
│             ▼                │                ▼                 │
│    ┌─────────────────┐       │       ┌─────────────────┐        │
│    │  MazeGridData   │◄──────┴──────►│   Game State    │        │
│    │   (UDataAsset)  │               │  (Runtime FSM)  │        │
│    └─────────────────┘               └─────────────────┘        │
└─────────────────────────────────────────────────────────────────┘
```

### 🔧 Key Features

- **🎲 Procedural Generation** — Three algorithms with seed-based reproducibility
- **⚡ Baked Geometry** — Editor-time baking for zero runtime overhead
- **🧭 Dynamic Pathfinding** — BFS with automatic target switching (Exit ↔ Key)
- **🎭 State-Driven Masks** — Conditional unlock system based on player choices
- **📦 Data-Driven Design** — `UDataAsset` for maze persistence and fast iteration
- **🔗 Blueprint Integration** — Full event system with delegates

---

## 📁 Project Structure

```
TheLastMask/
├── Source/
│   └── TheLastMask/
│       ├── TheLastMask.h/.cpp          # Module definition
│       ├── TheLastMask.Build.cs        # Build configuration
│       └── MazeSystem/
│           ├── MazeManager.h/.cpp      # Main orchestrator
│           └── Core/
│               ├── MazeTypes.h         # Enums, structs, configs
│               ├── MazeGenerator.h/.cpp    # Procedural algorithms
│               ├── MazeGridData.h/.cpp     # Persistent data asset
│               └── MazePathfinder.h/.cpp   # BFS pathfinding
├── Content/
│   └── Maze/
│       └── MazeGridData.uasset         # Baked maze data
├── Config/                             # Project settings
└── TheLastMask.uproject
```

---

## 🚀 Getting Started

### Prerequisites

- **Unreal Engine 5.5+**
- **Visual Studio 2022** with C++ game development workload
- **Git LFS** (for large asset files)

### Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/YOUR_USERNAME/TheLastMask.git
   cd TheLastMask
   git lfs pull
   ```

2. **Generate project files**
   - Right-click `TheLastMask.uproject` → **Generate Visual Studio project files**

3. **Open in Unreal**
   - Double-click `TheLastMask.uproject`, or
   - Open the `.sln` in Visual Studio and press `F5`

### Building the Maze

1. Place `BP_MazeManager` in your level
2. Assign `FloorMesh` and `WallMesh` in the Details panel
3. Configure generation settings (Size, Seed, Algorithm)
4. Click **"Bake Maze to Level"** button
5. Assign the generated `MazeGridData` asset
6. Place your `BP_Exit` and `BP_Key` actors
7. Wire up trigger volumes to notify the MazeManager

---

## 🧠 How It Works

### Maze Generation Algorithms

| Algorithm | Character | Best For |
|-----------|-----------|----------|
| **Recursive Backtracker** | Long, winding dead-ends | Horror (disorientation) |
| **Prim's** | Organic, radial growth | Natural caves |
| **Kruskal's** | Balanced, uniform | Fair puzzles |

### Pathfinding State Machine

```
┌──────────────┐     Player discovers Exit      ┌──────────────┐
│              │    (without Key)               │              │
│  Target:     │ ──────────────────────────────►│  Target:     │
│    EXIT      │                                │    KEY       │
│              │◄────────────────────────────── │              │
└──────────────┘     Player collects Key        └──────────────┘
       ▲                                               │
       │                                               │
       └───────────────────────────────────────────────┘
                    Player has Key
```

### Hollow Mask Unlock Logic

```cpp
// Only unlocks if Exit is discovered BEFORE finding the Key
if (bExitDiscovered && !bHasKey) {
    bHollowMaskUnlocked = true;  // Reward for exploration
}

if (bHasKey && !bExitDiscovered) {
    bHollowMaskPermanentlyLocked = true;  // Consequence of choice
}
```

---

## 📡 Blueprint Events

| Event | Fires When | Use Case |
|-------|------------|----------|
| `OnMazeReady` | Maze data loaded | Initialize UI, spawn player |
| `OnPathUpdated` | Path recalculated | Update visual effects |
| `OnExitDiscovered` | Player reaches exit area | Trigger narrative beat |
| `OnKeyCollected` | Player picks up key | Update HUD, play sound |
| `OnTargetChanged` | Pathfinding target switches | Update objective marker |
| `OnHollowMaskUnlocked` | Hollow Mask becomes available | Enable mask in UI |

---

## 🎯 API Quick Reference

```cpp
// Show/hide the path (Mask 1)
MazeManager->ShowPath(PlayerLocation);
MazeManager->HidePath();

// Notify game events (call from triggers/pickups)
MazeManager->NotifyExitDiscovered();
MazeManager->NotifyKeyCollected();

// Query state
bool bCanExit = MazeManager->CanExitMaze();
bool bHasXRay = MazeManager->IsHollowMaskAvailable();

// Get positions
FIntPoint ExitGrid = MazeManager->GetExitGridPosition();
FIntPoint KeyGrid = MazeManager->GetKeyGridPosition();
```

---

## 🤝 Contributing

This is a game jam project, but suggestions and improvements are welcome!

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📜 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **Global Game Jam 2026** — For the inspiration and deadline pressure
- **Unreal Engine** — For the powerful framework
- **The Horror Genre** — For teaching us that the scariest monster is choice itself

---

<p align="center">
  <i>Made with 🎭 and sleepless nights for Global Game Jam 2026</i>
</p>

<p align="center">
  <b>Remember: Every mask can only be used once. Choose wisely.</b>
</p>
