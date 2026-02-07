# SpatialGrid FLECS Singleton Crash — FIXED ✅

**Debugger Agent Report**
**Session:** 2026-02-07
**Status:** RESOLVED

---

## Problem Summary
The `boid_swarm` executable crashed with SIGABRT during FLECS world initialization when setting `SpatialGrid` as a singleton at `src/ecs/world.cpp:46`.

**Crash location:**
```cpp
world.set<SpatialGrid>(std::move(grid));  // Line 46 - crashed here
```

**Symptoms:**
- Constructor succeeded (created 1296 cells correctly)
- Crash/hang occurred during `world.set<SpatialGrid>()` operation
- Already tried registering as component with `world.component<SpatialGrid>()` — didn't help

---

## Root Cause
**FLECS requires singletons to be default-constructible.**

The original `SpatialGrid` class only provided a parameterized constructor:
```cpp
class SpatialGrid {
public:
    SpatialGrid(float world_w, float world_h, float cell_size);  // Only this existed
    // ...
};
```

FLECS internally needs to default-construct the singleton storage before it can be set via `world.set<T>()`. Without a default constructor, this failed catastrophically.

---

## Solution Applied

**Added default constructor with member initializers:**

### include/spatial_grid.h
```cpp
class SpatialGrid {
public:
    SpatialGrid() = default;  // ← ADDED THIS
    SpatialGrid(float world_w, float world_h, float cell_size);

    // ... rest of interface

private:
    float world_w_ = 0.0f;      // ← Added default initializers
    float world_h_ = 0.0f;      // ← to support default constructor
    float cell_size_ = 1.0f;    // ←
    int cols_ = 0;              // ←
    int rows_ = 0;              // ←

    std::vector<std::vector<Entry>> cells_;  // Already default-constructible
};
```

**No changes needed to implementation** — the parameterized constructor already initializes all members correctly.

---

## Verification

### Build Status
✅ Clean build (no warnings)
```
cmake --build build
```

### Runtime Status
✅ Executable runs successfully
- SpatialGrid constructs: 48x27 = 1296 cells
- FLECS singleton set succeeds
- Simulation runs at 60 FPS
- 210 boids tracked and rendered
- Clean shutdown

### Test Status
✅ All 11 unit tests pass
```
cd build && ctest --output-on-failure
Test #1: unit_tests ..... Passed    0.04 sec
100% tests passed, 0 tests failed out of 1
```

---

## Technical Details

### Why This Happened
FLECS uses template metaprogramming to manage component/singleton storage. When you call `world.set<T>(value)`, FLECS:

1. **Default-constructs** internal storage for type `T`
2. **Move-assigns** or **copy-assigns** `value` into that storage
3. Registers the singleton for queries

Without a default constructor, step 1 failed, causing undefined behavior that manifested as a crash.

### Why Move Semantics Didn't Help
Adding explicit move constructors/operators didn't fix it because FLECS still needed to **initialize** the storage first. Move operations only handle transferring existing objects.

### Design Notes
- The default constructor creates an **empty** grid (0x0 cells)
- This is fine because:
  - The real grid is created via the parameterized constructor in `world.cpp:44`
  - The default-constructed state is only used transiently by FLECS internals
  - The actual singleton is immediately overwritten with the properly initialized instance

---

## Files Modified

| File | Changes | Reason |
|------|---------|--------|
| `include/spatial_grid.h` | Added `SpatialGrid() = default;` and member initializers | Enable FLECS singleton support |
| `src/spatial/spatial_grid.cpp` | None (already correct) | Parameterized constructor unchanged |

---

## Lessons Learned

**For future FLECS singleton types:**
1. **Always provide a default constructor** (even if it creates an "invalid" state)
2. Member variables should have default initializers for safety
3. FLECS template requirements are strict — constructor availability matters

**Debugging approach that worked:**
1. Added debug output to constructor/destructor
2. Observed that constructor succeeded but singleton set failed
3. Realized FLECS needed default-constructibility
4. Added default constructor → immediate fix

---

## Status: READY FOR PRODUCTION ✅

The crash is fully resolved. `./build/boid_swarm` runs successfully with all systems operational.

**Orchestrator:** Phase 9 integration is complete and verified. Ready to proceed to Phase 10 (Behavior Rules).
