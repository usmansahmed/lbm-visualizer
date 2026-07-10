#include "lbm/grid.hpp"
#include "app/common.cuh"

__global__ void collisionKernel(float omega, float* f_old, float* f_new,
                                float* ux, float* uy, float* uz,
                                float* rho,
                                const int Nx, const int Ny, const int Nz){

    int x = threadIdx.x + blockDim.x*blockIdx.x;
    int y = threadIdx.y + blockDim.y*blockIdx.y;
    int z = threadIdx.z + blockDim.z*blockIdx.z;

    if (x>=Nx || y>=Ny || z>=Nz) return;
    //would be using the Bhatnagar-Gross-Krook collision model
    int N = Nx*Ny*Nz;
    int cell = x + y*Nx + z*Nx*Ny;

    float fi[19];
    float r = 0.0f;
    float mx = 0.0f, my = 0.0f, mz = 0.0f; //moments
    for (int i=0; i<Q; ++i){
        fi[i] = f_old[i*N+cell];
        r += fi[i];
        mx += D3Q19_X[i]*fi[i];
        my += D3Q19_Y[i]*fi[i];
        mz += D3Q19_Z[i]*fi[i];
    }
    float vx, vy, vz;
    if (r > 1e-6f) {
        float inv_r = 1.0f / r;
        vx = mx*inv_r;
        vy = my*inv_r;
        vz = mz*inv_r;
    }
    float F_x = 0.0001f;
    vx += F_x/(2.0f*r);
    rho[cell] = r;
    ux[cell] = vx;
    uy[cell] = vy;
    uz[cell] = vz;

    float usq = vx*vx + vy*vy + vz*vz;  
    for (int i=0; i<Q; ++i){
        float eu = D3Q19_X[i]*vx + D3Q19_Y[i]*vy + D3Q19_Z[i]*vz;
        float feq = D3Q19_W[i]*r*(1.0f+ eu/cs2 + eu*eu/(2.0f*cs2*cs2) - usq/(2.0f*cs2));
        f_old[i*N+cell] = fi[i] - omega*(fi[i]-feq); 
    }  
}

void launchCollisionKernel(LbmGridData& grid, float omega, dim3 blocks, dim3 threads){
    collisionKernel<<<blocks,threads>>>(omega, grid.d_f_old, grid.d_f_new,
                                        grid.d_ux, grid.d_uy, grid.d_uz,
                                        grid.d_rho, grid.Nx, grid.Ny, grid.Nz); 
    cudaDeviceSynchronize();   
}