#pragma once

#include "app/VisualizationState.hpp"
#include "app/SliceExtractor.hpp"
#include "app/VelocitySlice.hpp"

#include <vector>

std::vector<float> buildVelocityArrowVertices(
    const VisualizationState& frame,
    const VelocitySlice2D& velocities,
    SliceOrientation orientation,
    int sliceIndex,
    int stride,
    float arrowLengthScale
);