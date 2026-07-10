#include "LbmSolver.hpp"
#include "common.cuh"

#include <stdlib.h>
#include <cstdio>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <cmath>
#include <iostream>

extern void launchInitKernel(Grid &grid, dim3 blocks, dim3 threads);
extern void launchCollisionKernel(Grid &grid, float omega, dim3 blocks, dim3 threads);
extern void launchStreamingKernel(Grid &grid, dim3 blocks, dim3 threads);
extern void launchComputeMacroscopicKernel(Grid &grid, dim3 blocks, dim3 threads);
extern void launchInletBC(Grid &grid, dim3 blocks, dim3 threads);
extern void launchprepareDataKernel(Grid grid, float *mappedPixels,
                                    DisplayField field, SliceOrientation orientation,
                                    int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice,
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
    grid.Nx = config.nx;
    grid.Ny = config.ny;
    grid.Nz = config.nz;
    grid.N = grid.Nx * grid.Ny * grid.Nz;
    grid.d_arrowHorizontal_ = nullptr;
    grid.d_arrowVertical_ = nullptr;
    omega_ = config.omega;

    size_t f_size = sizeof(float) * grid.N * Q;
    size_t macro_size = sizeof(float) * grid.N;
    const std::size_t solid_size = sizeof(unsigned char) * static_cast<std::size_t>(grid.N);

    cudaMalloc(&grid.d_f_old, f_size);
    cudaMalloc(&grid.d_f_new, f_size);
    cudaMalloc(&grid.d_rho, macro_size);
    cudaMalloc(&grid.d_ux, macro_size);
    cudaMalloc(&grid.d_uy, macro_size);
    cudaMalloc(&grid.d_uz, macro_size);
    cudaMalloc(&grid.d_solid, solid_size);

    grid.h_solid = static_cast<unsigned char *>(std::malloc(solid_size));

    if (obstacle.size() != static_cast<std::size_t>(grid.N))
    {
        throw std::runtime_error("Obstacle mask size does not match grid size.");
    }

    std::copy(obstacle.begin(), obstacle.end(), grid.h_solid);
    cudaMemcpy(grid.d_solid, grid.h_solid, solid_size, cudaMemcpyHostToDevice);

    threads = dim3(8, 8, 4);
    blocks = dim3((grid.Nx + threads.x - 1) / threads.x, (grid.Ny + threads.y - 1) / threads.y, (grid.Nz + threads.z - 1) / threads.z);

    launchInitKernel(grid, blocks, threads);

    std::vector<unsigned char> solidCheck(grid.N);

    checkCuda(cudaMemcpy(solidCheck.data(), grid.d_solid, static_cast<std::size_t>(grid.N) * sizeof(unsigned char), cudaMemcpyDeviceToHost), "copy d_solid back to host");
}

LbmSolver::~LbmSolver()
{
    cudaFree(grid.d_f_old);
    cudaFree(grid.d_f_new);
    cudaFree(grid.d_rho);
    cudaFree(grid.d_ux);
    cudaFree(grid.d_uy);
    cudaFree(grid.d_uz);
    cudaFree(grid.d_solid);
    cudaFree(grid.d_arrowHorizontal_);
    cudaFree(grid.d_arrowVertical_);
    free(grid.h_solid);
}

void LbmSolver::step()
{
    // 1. Physics Engine Steps
    launchCollisionKernel(grid, omega_, blocks, threads);
    launchStreamingKernel(grid, blocks, threads);
    // Thread and Block configuration for the 2D slice boundary at x=0
    // dim3 boundary_threads(1, 16, 4);
    // dim3 boundary_blocks(1, (grid.Ny + 15) / 16, (grid.Nz + 3) / 4);

    // Inside time loop, right after launchStreamingKernel:
    // launchInletBC(grid, boundary_blocks, boundary_threads);
    std::swap(grid.d_f_old, grid.d_f_new);
}

const unsigned char *LbmSolver::getObstacle() const
{
    return grid.h_solid;
}

void LbmSolver::prepareData(float *mappedPixels, DisplayField field, SliceOrientation orientation,
                            int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice)
{
    dim3 blockSize(16, 16);
    dim3 gridSize((planeWidth + blockSize.x - 1) / blockSize.x, (planeHeight + blockSize.y - 1) / blockSize.y);

    const std::size_t sliceSize = static_cast<std::size_t>(planeWidth) * static_cast<std::size_t>(planeHeight);

    if (writeArrowSlice)
    {
        ensureArrowSliceCapacity(sliceSize);
    }

    const std::size_t numberOfBlocks = static_cast<std::size_t>(gridSize.x) * static_cast<std::size_t>(gridSize.y);
    ensureScaleBufferCapacity(numberOfBlocks);

    cudaGetLastError();
    launchprepareDataKernel(grid, mappedPixels,
                            field, orientation, sliceIndex,
                            planeWidth, planeHeight, writeArrowSlice,
                            gridSize, blockSize);

    checkCuda(cudaPeekAtLastError(), "prepareData kernel launch");
    checkCuda(cudaDeviceSynchronize(), "prepareData kernel execution");

    if (writeArrowSlice)
    {
        velocitySlice.width = planeWidth;
        velocitySlice.height = planeHeight;
        velocitySlice.horizontal.resize(sliceSize);
        velocitySlice.vertical.resize(sliceSize);

        checkCuda(cudaMemcpy(velocitySlice.horizontal.data(), grid.d_arrowHorizontal_, sliceSize * sizeof(float), cudaMemcpyDeviceToHost), "copy arrow horizontal slice to host");
        checkCuda(cudaMemcpy(velocitySlice.vertical.data(), grid.d_arrowVertical_, sliceSize * sizeof(float), cudaMemcpyDeviceToHost), "copy arrow vertical slice to host");
    }

    h_blockMinimums.resize(numberOfBlocks);
    h_blockMaximums.resize(numberOfBlocks);
    cudaMemcpy(h_blockMinimums.data(), grid.d_blockMinimums, numberOfBlocks * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_blockMaximums.data(), grid.d_blockMaximums, numberOfBlocks * sizeof(float), cudaMemcpyDeviceToHost);
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

    if (grid.d_arrowHorizontal_ != nullptr)
    {
        checkCuda(cudaFree(grid.d_arrowHorizontal_), "cudaFree d_arrowHorizontal_");
        grid.d_arrowHorizontal_ = nullptr;
    }

    if (grid.d_arrowVertical_ != nullptr)
    {
        checkCuda(cudaFree(grid.d_arrowVertical_), "cudaFree d_arrowVertical_");
        grid.d_arrowVertical_ = nullptr;
    }

    checkCuda(cudaMalloc(&grid.d_arrowHorizontal_, requiredCount * sizeof(float)), "cudaMalloc d_arrowHorizontal_");
    checkCuda(cudaMalloc(&grid.d_arrowVertical_, requiredCount * sizeof(float)), "cudaMalloc d_arrowVertical_");

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

    if (grid.d_blockMinimums != nullptr)
    {
        checkCuda(cudaFree(grid.d_blockMinimums), "cudaFree d_blockMinimums");
        grid.d_blockMinimums = nullptr;
    }

    if (grid.d_blockMaximums != nullptr)
    {
        checkCuda(cudaFree(grid.d_blockMaximums), "cudaFree d_blockMaximums");
        grid.d_blockMaximums = nullptr;
    }

    checkCuda(cudaMalloc(&grid.d_blockMinimums, requiredBlockCount * sizeof(float)), "cudaMalloc d_blockMinimums");
    checkCuda(cudaMalloc(&grid.d_blockMaximums, requiredBlockCount * sizeof(float)), "cudaMalloc d_blockMaximums");

    scaleBlockCapacity_ = requiredBlockCount;
}

const VelocitySlice2D &LbmSolver::getVelocitySlice2D() const
{
    return velocitySlice;
}

void LbmSolver::readProbeCell(ProbeResult &result, std::size_t index) const
{
    checkCuda(cudaMemcpy(&result.density, grid.d_rho + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe density");
    checkCuda(cudaMemcpy(&result.velocityX, grid.d_ux + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe velocityX");
    checkCuda(cudaMemcpy(&result.velocityY, grid.d_uy + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe velocityY");
    checkCuda(cudaMemcpy(&result.velocityZ, grid.d_uz + index, sizeof(float), cudaMemcpyDeviceToHost), "copy probe velocityZ");

    result.speed = std::sqrt(result.velocityX * result.velocityX + result.velocityY * result.velocityY + result.velocityZ * result.velocityZ);
    result.obstacle = grid.h_solid != nullptr && grid.h_solid[index];
}

std::pair<float, float> LbmSolver::getValueRange() const
{
    const auto [minIt, maxIt] = std::minmax_element(h_blockMinimums.begin(), h_blockMinimums.end());
    return {*minIt, *maxIt};
}
