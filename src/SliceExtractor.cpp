#include "SliceExtractor.hpp"

#include <cmath>

namespace
{
    std::size_t index3D(
        int x,
        int y,
        int z,
        int nx,
        int ny)
    {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(y) + static_cast<std::size_t>(ny) * z);
    }

    void extractVelocityMagnitudeXYSlice(
        const std::vector<float> &velocityX,
        const std::vector<float> &velocityY,
        const std::vector<float> &velocityZ,
        std::vector<float> &slice,
        int nx,
        int ny,
        int nz,
        int fixedZ)
    {

        slice.resize(
            static_cast<std::size_t>(nx) *
            static_cast<std::size_t>(ny));

        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const std::size_t sourceIndex =
                    index3D(x, y, fixedZ, nx, ny);

                const float ux = velocityX[sourceIndex];
                const float uy = velocityY[sourceIndex];
                const float uz = velocityZ[sourceIndex];

                const float speed =
                    std::sqrt(
                        ux * ux +
                        uy * uy +
                        uz * uz);

                const std::size_t destinationIndex =
                    static_cast<std::size_t>(x) + static_cast<std::size_t>(nx) * y;

                slice[destinationIndex] = speed;
            }
        }
    }

    void extractVelocityMagnitudeYZSlice(
        const std::vector<float> &velocityX,
        const std::vector<float> &velocityY,
        const std::vector<float> &velocityZ,
        std::vector<float> &slice,
        int nx,
        int ny,
        int nz,
        int fixedX)
    {
        slice.resize(
            static_cast<std::size_t>(ny) *
            static_cast<std::size_t>(nz));

        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < ny; ++y)
            {
                const std::size_t sourceIndex =
                    index3D(fixedX, y, z, nx, ny);

                const float ux = velocityX[sourceIndex];
                const float uy = velocityY[sourceIndex];
                const float uz = velocityZ[sourceIndex];

                const float speed =
                    std::sqrt(
                        ux * ux +
                        uy * uy +
                        uz * uz);

                const std::size_t destinationIndex =
                    static_cast<std::size_t>(y) + static_cast<std::size_t>(ny) * z;

                slice[destinationIndex] = speed;
            }
        }
    }

    void extractVelocityMagnitudeXZSlice(
        const std::vector<float> &velocityX,
        const std::vector<float> &velocityY,
        const std::vector<float> &velocityZ,
        std::vector<float> &slice,
        int nx,
        int ny,
        int nz,
        int fixedY)
    {
        slice.resize(
            static_cast<std::size_t>(nx) *
            static_cast<std::size_t>(nz));

        for (int z = 0; z < nz; ++z)
        {
            for (int x = 0; x < nx; ++x)
            {
                const std::size_t sourceIndex =
                    index3D(x, fixedY, z, nx, ny);

                const float ux = velocityX[sourceIndex];
                const float uy = velocityY[sourceIndex];
                const float uz = velocityZ[sourceIndex];

                const float speed =
                    std::sqrt(
                        ux * ux +
                        uy * uy +
                        uz * uz);

                const std::size_t destinationIndex =
                    static_cast<std::size_t>(x) + static_cast<std::size_t>(nx) * z;

                slice[destinationIndex] = speed;
            }
        }
    }
}

void getSliceDimensions(
    SliceOrientation orientation,
    int nx,
    int ny,
    int nz,
    int &width,
    int &height)
{
    switch (orientation)
    {
    case SliceOrientation::XY:
        width = nx;
        height = ny;
        break;

    case SliceOrientation::XZ:
        width = nx;
        height = nz;
        break;

    case SliceOrientation::YZ:
        width = ny;
        height = nz;
        break;
    }
}

int getMaximumSliceIndex(
    SliceOrientation orientation,
    int nx,
    int ny,
    int nz)
{
    switch (orientation)
    {
    case SliceOrientation::XY:
        return nz - 1;

    case SliceOrientation::XZ:
        return ny - 1;

    case SliceOrientation::YZ:
        return nx - 1;
    }

    return 0;
}

void extractSlice(const SimulationFrame &frame, std::vector<float> &output, SliceOrientation orientation, DisplayField field, int sliceIndex)
{

    if (orientation == SliceOrientation::XY)
    {
        extractVelocityMagnitudeXYSlice(frame.velocityX, frame.velocityY, frame.velocityZ, output, frame.nx, frame.ny, frame.nz, sliceIndex);
    }
    else if (orientation == SliceOrientation::YZ)
    {
        extractVelocityMagnitudeYZSlice(frame.velocityX, frame.velocityY, frame.velocityZ, output, frame.nx, frame.ny, frame.nz, sliceIndex);
    }
    else
    {
        extractVelocityMagnitudeXZSlice(frame.velocityX, frame.velocityY, frame.velocityZ, output, frame.nx, frame.ny, frame.nz, sliceIndex);
    }
}