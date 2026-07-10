#pragma once

struct LbmGridData{
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
