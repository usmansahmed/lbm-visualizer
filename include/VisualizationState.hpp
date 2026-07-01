#pragma once

#include "SliceExtractor.hpp"

#include <cstddef>

struct VisualizationState
{
    SliceOrientation orientation = SliceOrientation::XZ;
    DisplayField displayField = DisplayField::VelocityMagnitude;
    int currentSlice = 0;
    int maximumSlice = 0;

    std::size_t currentFrame = 0;

    bool playing = true;
    bool dataChanged = true;
    bool sliceChanged = true;
    bool orientationChanged = true;
    bool fieldChanged = true;

    float minimumValue = 0.0f;
    float maximumValue = 0.35f;
};