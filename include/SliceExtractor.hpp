#pragma once

#include "SimulationFrame.hpp"

#include <vector>

enum class SliceOrientation
{
    XY,
    XZ,
    YZ
};

enum class DisplayField
{
    VelocityMagnitude,
    VelocityX,
    VelocityY,
    VelocityZ,
    Density
};

void extractSlice(const SimulationFrame &frame, std::vector<float> &output, SliceOrientation orientation, DisplayField field, int sliceIndex);
void getSliceDimensions(SliceOrientation orientation, int nx, int ny, int nz, int &width, int &height);
int getMaximumSliceIndex(SliceOrientation orientation, int nx, int ny, int nz);