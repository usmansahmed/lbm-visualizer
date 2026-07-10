#pragma once
#include <cuda_runtime.h>

struct Grid{
    int Nx, Ny, Nz;
    int N;

    float* d_f_old;
    float* d_f_new;
    float* d_rho;
    float* d_ux;
    float* d_uy;
    float* d_uz;
    float* d_arrowHorizontal_ = nullptr;
    float* d_arrowVertical_ = nullptr;
    float* d_blockMaximums = nullptr;
    float* d_blockMinimums = nullptr;

    unsigned char* h_solid = nullptr;
    unsigned char* d_solid = nullptr;

};

__device__ inline int get_idx(int x, int y, int z, int i, int Nx, int Ny, int Nz){
    return (x + y*(Nx) + z*(Nx*Ny) + i*(Nx*Ny*Nz));
}