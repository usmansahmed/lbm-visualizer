#include "lbm/grid.hpp"
#include "app/common.cuh"

// Macros for 4D Structure of Arrays (SoA) indexing matching your collision/streaming layout
#define F_IDX(x, y, z, i) ((i) * (Nx * Ny * Nz) + (x) + (y) * Nx + (z) * Nx * Ny)

__global__ void inletBCKernel(float* f_new, const int Nx, const int Ny, const int Nz) {
    // 2D thread grid mapping across the Y and Z dimensions of the inlet plane
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    int x = 0; // Rigidly locked at the inlet column

    if (y >= Ny || z >= Nz) return;

    // Target wind-tunnel inflow velocity
    const float u0 = 0.04f; 
    const float u_sq = u0 * u0;

    // 1. Calculate the local density (rho) at the inlet boundary node.
    // We sum the known populations that stayed (0) or streamed from inside the domain.
    // Directions pointing right (inward) are currently unknown.
    float rho = f_new[F_IDX(x, y, z, 0)] + 
                f_new[F_IDX(x, y, z, 2)] + 
                f_new[F_IDX(x, y, z, 3)] + 
                f_new[F_IDX(x, y, z, 4)] + 
                f_new[F_IDX(x, y, z, 5)] + 
                f_new[F_IDX(x, y, z, 6)] +
                2.0f * (f_new[F_IDX(x, y, z, 2)] + 
                        f_new[F_IDX(x, y, z, 9)] + 
                        f_new[F_IDX(x, y, z, 10)] + 
                        f_new[F_IDX(x, y, z, 14)] + 
                        f_new[F_IDX(x, y, z, 16)]);
    
    // Adjust density based on the target inlet velocity scale factor
    rho = rho / (1.0f - u0);

    // 2. Overwrite the 5 distributions pointing straight/diagonally into the domain
    // These directions are: 1 (+x), 7 (+x+y), 8 (+x-y), 11 (+x+z), 13 (+x-z)
    const int inlet_directions[5] = {1, 7, 8, 11, 13}; 
    
    for (int idx = 0; idx < 5; ++idx) {
        int i = inlet_directions[idx];
        float dot_prod = D3Q19_X[i] * u0;
        
        // Compute equilibrium profile and inject directly into f_new
        f_new[F_IDX(x, y, z, i)] = D3Q19_W[i] * rho * (1.0f + 3.0f * dot_prod + 4.5f * dot_prod * dot_prod - 1.5f * u_sq);
    }
}

// Host wrapper function called directly from your main simulation loop
void launchInletBC(LbmGridData& grid, dim3 blocks, dim3 threads) {
    // Blocks/threads are mapped across the 2D Y-Z profile face
    inletBCKernel<<<blocks, threads>>>(
        grid.d_f_new, // Targets the fresh post-streaming buffer
        grid.Nx, grid.Ny, grid.Nz
    );
}