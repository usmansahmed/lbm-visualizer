#include <cstdio>
#include "lbm/grid.hpp"
#include "app/common.cuh"

__constant__ int D3Q19_X[19] = {0, 1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0};

__constant__ int D3Q19_Y[19] = {0, 0, 0, 1, -1, 0, 0, 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1};

__constant__ int D3Q19_Z[19] = {0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1};

__constant__ int D3Q19_OPP[19] = {0, 2, 1, 4, 3, 6, 5, 10, 9, 8, 7, 14, 13, 12, 11, 18, 17, 16, 15};

__constant__ float D3Q19_W[19] = {
    1.0f / 3.0f,
    1.0f / 18.0f, 1.0f / 18.0f,
    1.0f / 18.0f, 1.0f / 18.0f,
    1.0f / 18.0f, 1.0f / 18.0f,
    1.0f / 36.0f, 1.0f / 36.0f,
    1.0f / 36.0f, 1.0f / 36.0f,
    1.0f / 36.0f, 1.0f / 36.0f,
    1.0f / 36.0f, 1.0f / 36.0f,
    1.0f / 36.0f, 1.0f / 36.0f,
    1.0f / 36.0f, 1.0f / 36.0f};

__global__ void initKernel(float *f_old, float *f_new,
                           float *rho,
                           float *ux, float *uy, float *uz,
                           int Nx, int Ny, int Nz)
{

    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= Nx || y >= Ny || z >= Nz)
        return;
    float init_rho = 1.0f;
    float init_ux = 0.0f;
    float init_uy = 0.0f;
    float init_uz = 0.0f;
    int N = Nx * Ny * Nz;
    int idx = x + y * Nx + z * Ny * Nx;
    rho[idx] = init_rho;
    ux[idx] = init_ux;
    uy[idx] = init_uy;
    uz[idx] = init_uz;

    float u_sq = init_ux * init_ux + init_uy * init_uy + init_uz * init_uz;
    for (int i = 0; i < Q; ++i)
    {
        float dot_prod = D3Q19_X[i] * init_ux + D3Q19_Y[i] * init_uy + D3Q19_Z[i] * init_uz;
        float feq = D3Q19_W[i] * init_rho * (1.0f + 3.0f * dot_prod + 4.5f * dot_prod * dot_prod - 1.5f * u_sq);
        int f_idx = (i * N) + idx;
        f_old[f_idx] = feq;
        f_new[f_idx] = feq;
    }
}

void launchInitKernel(LbmGridData &grid, dim3 blocks, dim3 threads)
{
    initKernel<<<blocks, threads>>>(grid.d_f_old, grid.d_f_new, grid.d_rho,
                                    grid.d_ux, grid.d_uy, grid.d_uz,
                                    grid.Nx, grid.Ny, grid.Nz);
    cudaDeviceSynchronize();
}