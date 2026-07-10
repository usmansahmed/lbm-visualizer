#include "app/SliceExtractor.hpp"

#include <cmath>

namespace
{
    void extractXYSlice(
        const VisualizationState &frame,
        std::vector<float> &slice,
        DisplayField field,
        int fixedZ)
    {

        slice.resize(
            static_cast<std::size_t>(frame.nx) *
            static_cast<std::size_t>(frame.ny));

        for (int y = 0; y < frame.ny; ++y)
        {
            for (int x = 0; x < frame.nx; ++x)
            {
                const std::size_t sourceIndex =
                    index3D(x, y, fixedZ, frame.nx, frame.ny);

                const std::size_t destinationIndex = static_cast<std::size_t>(x) + static_cast<std::size_t>(frame.nx) * y;

                slice[destinationIndex] = getDisplayValue(frame, field, sourceIndex);
            }
        }
    }

    void extractYZSlice(
        const VisualizationState &frame,
        std::vector<float> &slice,
        DisplayField field,
        int fixedX)
    {
        slice.resize(
            static_cast<std::size_t>(frame.ny) *
            static_cast<std::size_t>(frame.nz));

        for (int z = 0; z < frame.nz; ++z)
        {
            for (int y = 0; y < frame.ny; ++y)
            {
                const std::size_t sourceIndex =
                    index3D(fixedX, y, z, frame.nx, frame.ny);

                const std::size_t destinationIndex =
                    static_cast<std::size_t>(y) + static_cast<std::size_t>(frame.ny) * z;

                slice[destinationIndex] = getDisplayValue(frame, field, sourceIndex);
            }
        }
    }

    void extractXZSlice(
        const VisualizationState &frame,
        std::vector<float> &slice,
        DisplayField field,
        int fixedY)
    {
        slice.resize(
            static_cast<std::size_t>(frame.nx) *
            static_cast<std::size_t>(frame.nz));

        for (int z = 0; z < frame.nz; ++z)
        {
            for (int x = 0; x < frame.nx; ++x)
            {
                const std::size_t sourceIndex =
                    index3D(x, fixedY, z, frame.nx, frame.ny);

                const std::size_t destinationIndex =
                    static_cast<std::size_t>(x) + static_cast<std::size_t>(frame.nx) * z;

                slice[destinationIndex] = getDisplayValue(frame, field, sourceIndex);
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

void extractSlice(const VisualizationState &frame, std::vector<float> &output, SliceOrientation orientation, DisplayField field, int sliceIndex)
{

    if (orientation == SliceOrientation::XY)
    {
        extractXYSlice(frame, output, field, sliceIndex);
    }
    else if (orientation == SliceOrientation::YZ)
    {
        extractYZSlice(frame, output, field, sliceIndex);
    }
    else
    {
        extractXZSlice(frame, output, field, sliceIndex);
    }
}

float getDisplayValue(const VisualizationState &frame, DisplayField field, std::size_t index)
{
    switch (field)
    {
    case DisplayField::Obstacle:
        return frame.obstacle[index] != 0 ? 1.0f : 0.0f;
    }

    return 0.0f;
}

std::size_t index3D(int x, int y, int z, int nx, int ny)
{        
    return static_cast<std::size_t>(x) + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(y) + static_cast<std::size_t>(ny) * z);
}