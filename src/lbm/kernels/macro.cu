#include "lbm/grid.hpp"
#include "app/common.cuh"

__global__ void computeMacroscopicKernel(const float* f_old, 
                                         float* rho, float* ux, float* uy, float* uz,
                                         const int Nx, const int Ny, const int Nz) {
    // 1. Calculate thread global coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    // Boundary guard
    if (x >= Nx || y >= Ny || z >= Nz) return;

    int N = Nx * Ny * Nz;
    int cell = x + y * Nx + z * Nx * Ny;

    // 2. Initialize local register accumulators
    float r = 0.0f;
    float mx = 0.0f;
    float my = 0.0f;
    float mz = 0.0f;

    // 3. Compute the zero-th and first moments (Density and Momentum)
    // We pull the lattice directions directly from your global constants
    for (int i = 0; i < 19; ++i) {
        float fi = f_old[i * N + cell];
        r  += fi;
        mx += D3Q19_X[i] * fi;
        my += D3Q19_Y[i] * fi;
        mz += D3Q19_Z[i] * fi;
    }

    // 4. Save the computed macroscopic values to global device memory
    rho[cell] = r;
    
    // Protect against division by zero if the fluid is completely empty
    if (r > 1e-6f) {
        float inv_r = 1.0f / r;
        ux[cell] = mx * inv_r;
        uy[cell] = my * inv_r;
        uz[cell] = mz * inv_r;
    } else {
        ux[cell] = 0.0f;
        uy[cell] = 0.0f;
        uz[cell] = 0.0f;
    }
}

// Host Wrapper Function
void launchComputeMacroscopicKernel(LbmGridData& grid, dim3 blocks, dim3 threads) {
    computeMacroscopicKernel<<<blocks, threads>>>(
        grid.d_f_old, // Pass the current active population buffer
        grid.d_rho, grid.d_ux, grid.d_uy, grid.d_uz,
        grid.Nx, grid.Ny, grid.Nz
    );
    cudaDeviceSynchronize();
}