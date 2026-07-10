#pragma once

#include <vector>

struct VelocitySlice2D
{
    int width = 0;
    int height = 0;

    std::vector<float> horizontal;
    std::vector<float> vertical;
};