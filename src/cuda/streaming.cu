#include "grid.cuh"
#include "common.cuh"

// Ensure D3Q19_OPP is visible here if defined in common.cuh
// __constant__ int D3Q19_OPP[19] = {0, 2,1, 4,3, 6,5, 8,7, 10,9, 12,11, 14,13, 16,15, 18,17};

__global__ void streamingKernel(const float* f_old, float* f_new,
                                const unsigned char*solid, 
                                const int Nx, const int Ny, const int Nz){
    int x = threadIdx.x + blockDim.x * blockIdx.x;
    int y = threadIdx.y + blockDim.y * blockIdx.y;
    int z = threadIdx.z + blockDim.z * blockIdx.z;
    
    if (x >= Nx || y >= Ny || z >= Nz) return;
    
    int N = Nx * Ny * Nz;
    int dest_cell = x + y * Nx + z * Nx * Ny;
    
    // If this destination node is inside an obstacle, it doesn't contain fluid
    if (solid[dest_cell] == 1) return;

    for (int i = 0; i < Q; ++i){
        int src_x = (x - D3Q19_X[i]+Nx)%Nx;
        int src_y = y - D3Q19_Y[i];
        int src_z = (z - D3Q19_Z[i]+Nz)%Nz;

        // 1. TOP & BOTTOM BOUNDARIES (Solid Walls -> Bounce-Back)
        if (src_y < 0 || src_y >= Ny) {
            f_new[i * N + dest_cell] = f_old[D3Q19_OPP[i] * N + dest_cell];
            continue;
        }

        // // 2. FRONT & BACK BOUNDARIES (Periodic in Z-direction)
        // if (src_z < 0 || src_z >= Nz){
        //     src_z = (src_z + Nz) % Nz; 
        // }

        // // 3. LEFT BOUNDARY (Inlet: x = 0)
        // if (src_x < 0) {
        //     // Do nothing here! The populations pointing right into the domain (D3Q19_X[i] > 0)
        //     // will be missing, and your inletBCKernel will fill them explicitly.
        //     continue;
        // }

        // // 4. RIGHT BOUNDARY (Outlet: x = Nx)
        // if (src_x >= Nx) {
        //     // Simple Outflow: Extrapolate from the last fluid node column
        //     int outlet_cell = (Nx - 1) + src_y * Nx + src_z * Nx * Ny;
        //     f_new[i * N + dest_cell] = f_old[i * N + outlet_cell];
        //     continue;
        // }

        // 5. INTERNAL FLUID & OBSTACLES
        int src_cell = src_x + src_y * Nx + src_z * Nx * Ny;
        
        if (solid[src_cell] == 1){
            // Internal obstacle bounce-back (like your square cylinder)
            f_new[i * N + dest_cell] = f_old[D3Q19_OPP[i] * N + dest_cell];
        }
        else {
            // Standard pull streaming
            f_new[i * N + dest_cell] = f_old[i * N + src_cell];
        }
    }
}

void launchStreamingKernel(Grid& grid, dim3 blocks, dim3 threads){
    streamingKernel<<<blocks, threads>>>(grid.d_f_old, grid.d_f_new, grid.d_solid,
                                         grid.Nx, grid.Ny, grid.Nz);
    cudaDeviceSynchronize(); //for speed. Let the stream execute natively.
}