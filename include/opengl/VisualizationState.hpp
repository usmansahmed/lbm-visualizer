#pragma once

#include "Enums.hpp"

#include <cstddef>
#include <vector>

struct VisualizationState
{
    int nx = 254;
    int ny = 64;
    int nz = 16;

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
    bool automaticColorScaling = false;
    bool restartRequested = false;

    float minimumValue = 0.0f;
    float maximumValue = 0.003f;
    float automaticMinimumValue;
    float automaticMaximumValue;
    float finalMinimum;
    float finalMaximum;

    bool showVelocityArrows = true;
    int arrowStride = 8;
    float arrowLengthScale = 0.75f;
    bool arrowsChanged = true;

    std::vector<unsigned char> obstacle;
};