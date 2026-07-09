#pragma once
#include <cuda_runtime.h>
static constexpr int Q = 19;
static constexpr int D = 3;
static constexpr float omega = 1.2f;
static constexpr float cs2 = 1.0f/3.0f; //lattice speed of sound squared 
//velocity vectors 
extern __constant__ int D3Q19_X[19];
extern __constant__ int D3Q19_Y[19];
extern __constant__ int D3Q19_Z[19];
extern __constant__ int D3Q19_OPP[19];
extern __constant__ float D3Q19_W[19];
