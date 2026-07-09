#pragma once

#include "VisualizationState.hpp"
#include "Enums.hpp"

#include <vector>

void extractSlice(const VisualizationState &frame, std::vector<float> &output, SliceOrientation orientation, DisplayField field, int sliceIndex);
void getSliceDimensions(SliceOrientation orientation, int nx, int ny, int nz, int &width, int &height);
int getMaximumSliceIndex(SliceOrientation orientation, int nx, int ny, int nz);
float getDisplayValue(const VisualizationState& frame, DisplayField field, std::size_t index);
std::size_t index3D(int x, int y, int z, int nx, int ny);
