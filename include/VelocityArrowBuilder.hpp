#pragma once

#include "SimulationFrame.hpp"
#include "SliceExtractor.hpp"

#include <vector>

std::vector<float> buildVelocityArrowVertices(
    const SimulationFrame& frame,
    SliceOrientation orientation,
    int sliceIndex,
    int stride,
    float arrowLengthScale
);