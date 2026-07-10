#pragma once

#include "app/Enums.hpp"
#include "app/ProbeResult.hpp"
#include "app/SliceExtractor.hpp"

#include <vector>

std::vector<float> buildProbeOverlayVertices(const ProbeResult &probe, SliceOrientation orientation,
                                             int currentSlice, int planeWidth, int planeHeight);