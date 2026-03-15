#pragma once

#include "gpu/gpu_buffers.h"

struct GridParams {
    float world_w, world_h, cell_size;
    int cols, rows, total_cells;
    bool toroidal;
};

// Build spatial hash: calcHash -> CUB sort -> findCellStart -> reorder -> pointer swap
void gpu_spatial_hash_build(GpuBuffers& buf, const GridParams& grid);
