#pragma once

#include "VisualizationState.hpp"
#include "SliceExtractor.hpp"
#include "VelocitySlice.hpp"

#include <vector>

std::vector<float> buildVelocityArrowVertices(
    const VisualizationState& frame,
    const VelocitySlice2D& velocities,
    SliceOrientation orientation,
    int sliceIndex,
    int stride,
    float arrowLengthScale
);