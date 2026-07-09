#pragma once

#include "ProbeResult.hpp"
#include "SliceExtractor.hpp"

#include <vector>

std::vector<float> buildProbeOverlayVertices(
    const ProbeResult& probe,
    SliceOrientation orientation,
    int currentSlice,
    int planeWidth,
    int planeHeight
);