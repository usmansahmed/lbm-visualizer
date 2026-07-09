#include "grid.cuh"
#include "common.cuh"
#include "Enums.hpp"

#include <cfloat>

__global__ void writeSliceToPboKernel(Grid fields, float *scalarOut, SliceOrientation orientation,
                                      DisplayField displayField, int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice)
{
    const int planeX = blockIdx.x * blockDim.x + threadIdx.x;
    const int planeY = blockIdx.y * blockDim.y + threadIdx.y;

    const int localIndex = threadIdx.x + blockDim.x * threadIdx.y;

    const int localThreadCount = blockDim.x * blockDim.y;

    extern __shared__ float shared[];

    float *sharedMin = shared;
    float *sharedMax = shared + localThreadCount;

    bool validCell = planeX < planeWidth && planeY < planeHeight;

    if (validCell)
    {

        int x = 0;
        int y = 0;
        int z = 0;

        if (orientation == SliceOrientation::XY)
        {
            x = planeX;
            y = planeY;
            z = sliceIndex;
        }
        else if (orientation == SliceOrientation::XZ)
        {
            x = planeX;
            y = sliceIndex;
            z = planeY;
        }
        else
        {
            x = sliceIndex;
            y = planeX;
            z = planeY;
        }

        const std::size_t index3D = static_cast<std::size_t>(x) +
                                    static_cast<std::size_t>(fields.Nx) * (static_cast<std::size_t>(y) + static_cast<std::size_t>(fields.Ny) * static_cast<std::size_t>(z));

        const std::size_t index2D = static_cast<std::size_t>(planeX) + static_cast<std::size_t>(planeWidth) * static_cast<std::size_t>(planeY);

        const float ux = fields.d_ux[index3D];
        const float uy = fields.d_uy[index3D];
        const float uz = fields.d_uz[index3D];

        float value = 0.0f;

        if (displayField == DisplayField::VelocityMagnitude)
        {
            value = sqrtf(ux * ux + uy * uy + uz * uz);
        }
        else if (displayField == DisplayField::VelocityX)
        {
            value = ux;
        }
        else if (displayField == DisplayField::VelocityY)
        {
            value = uy;
        }
        else if (displayField == DisplayField::VelocityZ)
        {
            value = uz;
        }
        else if (displayField == DisplayField::Density)
        {
            value = fields.d_rho[index3D];
        }
        // else if (displayField == 5)
        // {
        //     value = fields.obstacle[index3D];
        // }

        scalarOut[index2D] = value;
        if (writeArrowSlice)
        {
            if (orientation == SliceOrientation::XY)
            {
                fields.d_arrowHorizontal_[index2D] = ux;
                fields.d_arrowVertical_[index2D] = uy;
            }
            else if (orientation == SliceOrientation::XZ)
            {
                fields.d_arrowHorizontal_[index2D] = ux;
                fields.d_arrowHorizontal_[index2D] = uz;
            }
            else
            {
                fields.d_arrowHorizontal_[index2D] = uy;
                fields.d_arrowHorizontal_[index2D] = uz;
            }
        }

        const bool isObstacle = fields.d_solid[index3D];

        if (isObstacle)
        {
            sharedMin[localIndex] = FLT_MAX;
            sharedMax[localIndex] = -FLT_MAX;
        }
        else
        {
            sharedMin[localIndex] = value;
            sharedMax[localIndex] = value;
        }
    }
    else
    {
        sharedMin[localIndex] = FLT_MAX;
        sharedMax[localIndex] = -FLT_MAX;
    }

    __syncthreads();

    for (int offset = localThreadCount / 2; offset > 0; offset /= 2)
    {
        if (localIndex < offset)
        {
            sharedMin[localIndex] = fminf(sharedMin[localIndex], sharedMin[localIndex + offset]);
            sharedMax[localIndex] = fmaxf(sharedMax[localIndex], sharedMax[localIndex + offset]);
        }
        __syncthreads();
    }

    if (localIndex == 0)
    {
        const int blockIndex = blockIdx.x + gridDim.x * blockIdx.y;

        fields.d_blockMinimums[blockIndex] = sharedMin[0];
        fields.d_blockMaximums[blockIndex] = sharedMax[0];
    }
}

void launchprepareDataKernel(Grid grid, float *mappedPixels,
                             DisplayField field, SliceOrientation orientation,
                             int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice,
                             dim3 blocks, dim3 threads)
{
    const int threadsPerBlock = threads.x * threads.y;
    const std::size_t sharedMemorySize = 2 * threadsPerBlock * sizeof(float);
    writeSliceToPboKernel<<<blocks, threads, sharedMemorySize>>>(grid, mappedPixels, orientation, field, sliceIndex, planeWidth, planeHeight, writeArrowSlice);
}