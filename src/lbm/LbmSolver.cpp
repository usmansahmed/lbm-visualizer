#include "lbm/LbmSolver.hpp"
#include "app/common.cuh"

#include <stdlib.h>
#include <cstdio>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <cmath>
#include <iostream>

extern void launchInitKernel(LbmGridData &grid, dim3 blocks, dim3 threads);
extern void launchCollisionKernel(LbmGridData &grid, float omega, dim3 blocks, dim3 threads);
extern void launchStreamingKernel(LbmGridData &grid, dim3 blocks, dim3 threads);
extern void launchComputeMacroscopicKernel(LbmGridData &grid, dim3 blocks, dim3 threads);
extern void launchInletBC(LbmGridData &grid, dim3 blocks, dim3 threads);
extern void launchPrepareVisualizationSliceKernel(LbmGridData grid, float *mappedPixels,
                                                  DisplayField field, SliceOrientation orientation,
                                                  int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice, bool automaticScaling,
                                                  dim3 blocks, dim3 threads);

namespace
{
    void checkCuda(cudaError_t error, const char *message)
    {
        if (error != cudaSuccess)
        {
            throw std::runtime_error(std::string(message) + ": " + cudaGetErrorString(error));
        }
    }
}

LbmSolver::LbmSolver(const SimulationConfig &config, const std::vector<unsigned char> &obstacle)
{
    gridData_.Nx = config.nx;
    gridData_.Ny = config.ny;
    gridData_.Nz = config.nz;
    gridData_.N = gridData_.Nx * gridData_.Ny * gridData_.Nz;
    gridData_.d_arrowHorizontal_ = nullptr;
    gridData_.d_arrowVertical_ = nullptr;
    omega_ = config.omega;

    size_t f_size = sizeof(float) * gridData_.N * Q;
    size_t macro_size = sizeof(float) * gridData_.N;
    const std::size_t solid_size = sizeof(unsigned char) * static_cast<std::size_t>(gridData_.N);

    cudaMalloc(&gridData_.d_f_old, f_size);
    cudaMalloc(&gridData_.d_f_new, f_size);
    cudaMalloc(&gridData_.d_rho, macro_size);
    cudaMalloc(&gridData_.d_ux, macro_size);
    cudaMalloc(&gridData_.d_uy, macro_size);
    cudaMalloc(&gridData_.d_uz, macro_size);
    cudaMalloc(&gridData_.d_solid, solid_size);

    gridData_.h_solid = static_cast<unsigned char *>(std::malloc(solid_size));

    if (obstacle.size() != static_cast<std::size_t>(gridData_.N))
    {
        throw std::runtime_error("Obstacle mask size does not match grid size.");
    }

    std::copy(obstacle.begin(), obstacle.end(), gridData_.h_solid);
    cudaMemcpy(gridData_.d_solid, gridData_.h_solid, solid_size, cudaMemcpyHostToDevice);

    threads_ = dim3(8, 8, 4);
    blocks_ = dim3((gridData_.Nx + threads_.x - 1) / threads_.x, (gridData_.Ny + threads_.y - 1) / threads_.y, (gridData_.Nz + threads_.z - 1) / threads_.z);

    launchInitKernel(gridData_, blocks_, threads_);
}

LbmSolver::~LbmSolver()
{
    cudaFree(gridData_.d_f_old);
    cudaFree(gridData_.d_f_new);
    cudaFree(gridData_.d_rho);
    cudaFree(gridData_.d_ux);
    cudaFree(gridData_.d_uy);
    cudaFree(gridData_.d_uz);
    cudaFree(gridData_.d_solid);
    cudaFree(gridData_.d_arrowHorizontal_);
    cudaFree(gridData_.d_arrowVertical_);
    free(gridData_.h_solid);
}

void LbmSolver::step()
{
    // 1. Physics Engine Steps
    launchCollisionKernel(gridData_, omega_, blocks_, threads_);
    launchStreamingKernel(gridData_, blocks_, threads_);
    std::swap(gridData_.d_f_old, gridData_.d_f_new);
}

const unsigned char *LbmSolver::getObstacle() const
{
    return gridData_.h_solid;
}

void LbmSolver::prepareVisualizationSlice(float *mappedPixels, DisplayField field, SliceOrientation orientation,
                                          int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice, bool automaticScaling)
{
    dim3 blockSize(16, 16);
    dim3 gridSize((planeWidth + blockSize.x - 1) / blockSize.x, (planeHeight + blockSize.y - 1) / blockSize.y);

    const std::size_t sliceSize = static_cast<std::size_t>(planeWidth) * static_cast<std::size_t>(planeHeight);

    if (writeArrowSlice)
    {
        ensureArrowSliceCapacity(sliceSize);
    }

    const std::size_t numberOfBlocks = static_cast<std::size_t>(gridSize.x) * static_cast<std::size_t>(gridSize.y);
    if (automaticScaling)
    {
        ensureScaleBufferCapacity(numberOfBlocks);
    }

    cudaGetLastError();
    launchPrepareVisualizationSliceKernel(gridData_, mappedPixels,
                                          field, orientation, sliceIndex,
                                          planeWidth, planeHeight, writeArrowSlice, automaticScaling,
                                          gridSize, blockSize);

    checkCuda(cudaPeekAtLastError(), "prepareData kernel launch");
    checkCuda(cudaDeviceSynchronize(), "prepareData kernel execution");

    if (writeArrowSlice)
    {
        velocitySlice_.width = planeWidth;
        velocitySlice_.height = planeHeight;
        velocitySlice_.horizontal.resize(sliceSize);
        velocitySlice_.vertical.resize(sliceSize);

        checkCuda(cudaMemcpy(velocitySlice_.horizontal.data(), gridData_.d_arrowHorizontal_, sliceSize * sizeof(float), cudaMemcpyDeviceToHost), "copy arrow horizontal slice to host");
        checkCuda(cudaMemcpy(velocitySlice_.vertical.data(), gridData_.d_arrowVertical_, sliceSize * sizeof(float), cudaMemcpyDeviceToHost), "copy arrow vertical slice to host");
    }

    if (automaticScaling)
    {
        h_blockMinimums_.resize(numberOfBlocks);
        h_blockMaximums_.resize(numberOfBlocks);
        cudaMemcpy(h_blockMinimums_.data(), gridData_.d_blockMinimums, numberOfBlocks * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_blockMaximums_.data(), gridData_.d_blockMaximums, numberOfBlocks * sizeof(float), cudaMemcpyDeviceToHost);
    }
}

void LbmSolver::ensureArrowSliceCapacity(std::size_t requiredCount)
{
    if (requiredCount == 0)
    {
        return;
    }

    if (requiredCount <= arrowSliceCapacity_)
    {
        return;
    }

    if (gridData_.d_arrowHorizontal_ != nullptr)
    {
        checkCuda(cudaFree(gridData_.d_arrowHorizontal_), "cudaFree d_arrowHorizontal_");
        gridData_.d_arrowHorizontal_ = nullptr;
    }

    if (gridData_.d_arrowVertical_ != nullptr)
    {
        checkCuda(cudaFree(gridData_.d_arrowVertical_), "cudaFree d_arrowVertical_");
        gridData_.d_arrowVertical_ = nullptr;
    }

    checkCuda(cudaMalloc(&gridData_.d_arrowHorizontal_, requiredCount * sizeof(float)), "cudaMalloc d_arrowHorizontal_");
    checkCuda(cudaMalloc(&gridData_.d_arrowVertical_, requiredCount * sizeof(float)), "cudaMalloc d_arrowVertical_");

    arrowSliceCapacity_ = requiredCount;
}

void LbmSolver::ensureScaleBufferCapacity(std::size_t requiredBlockCount)
{
    if (requiredBlockCount == 0)
    {
        return;
    }

    if (requiredBlockCount <= scaleBlockCapacity_)
    {
        return;
    }

    if (gridData_.d_blockMinimums != nullptr)
    {
        checkCuda(cudaFree(gridData_.d_blockMinimums), "cudaFree d_blockMinimums");
        gridData_.d_blockMinimums = nullptr;
    }

    if (gridData_.d_blockMaximums != nullptr)
    {
        checkCuda(cudaFree(gridData_.d_blockMaximums), "cudaFree d_blockMaximums");
        gridData_.d_blockMaximums = nullptr;
    }

    checkCuda(cudaMalloc(&gridData_.d_blockMinimums, requiredBlockCount * sizeof(float)), "cudaMalloc d_blockMinimums");
    checkCuda(cudaMalloc(&gridData_.d_blockMaximums, requiredBlockCount * sizeof(float)), "cudaMalloc d_blockMaximums");

    scaleBlockCapacity_ = requiredBlockCount;
}

const VelocitySlice2D &LbmSolver::getVelocitySlice2D() const
{
    return velocitySlice_;
}

void LbmSolver::readProbeCell(ProbeResult &result, std::size_t index) const
{
    result.obstacle = gridData_.h_solid != nullptr && gridData_.h_solid[index] != 0;
    if (result.obstacle)
    {
        result.density = 0.0f;
        result.velocityX = 0.0f;
        result.velocityY = 0.0f;
        result.velocityZ = 0.0f;
        result.speed = 0.0f;
        return;
    }

    checkCuda(cudaMemcpy(&result.density, gridData_.d_rho + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe density");
    checkCuda(cudaMemcpy(&result.velocityX, gridData_.d_ux + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe velocityX");
    checkCuda(cudaMemcpy(&result.velocityY, gridData_.d_uy + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe velocityY");
    checkCuda(cudaMemcpy(&result.velocityZ, gridData_.d_uz + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe velocityZ");

    result.speed = std::sqrt(result.velocityX * result.velocityX +
                             result.velocityY * result.velocityY +
                             result.velocityZ * result.velocityZ);  
}

std::pair<float, float> LbmSolver::getValueRange() const
{
    float minimumValue = *std::min_element(h_blockMinimums_.begin(), h_blockMinimums_.end());
    float maximumValue = *std::max_element(h_blockMaximums_.begin(), h_blockMaximums_.end());
    return {minimumValue, maximumValue};
}
