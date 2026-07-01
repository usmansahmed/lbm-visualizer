#pragma once

#include <vector>

struct SimulationFrame
{
    int nx = 0;
    int ny = 0;
    int nz = 0;

    std::vector<float> density;

    std::vector<float> velocityX;
    std::vector<float> velocityY;
    std::vector<float> velocityZ;

    std::vector<float> obstacle;
};