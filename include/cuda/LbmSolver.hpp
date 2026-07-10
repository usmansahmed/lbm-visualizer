#pragma once

#include "grid.cuh"
#include "Enums.hpp"
#include "VelocitySlice.hpp"
#include "ProbeResult.hpp"
#include "SimulationConfig.hpp"

#include <utility>

class LbmSolver
{
public:
    LbmSolver(const SimulationConfig &config, const std::vector<unsigned char> &obstacle);
    ~LbmSolver();

    void step();
    void prepareData(float *mappedPixels, DisplayField field, SliceOrientation orientation,
                     int sliceIndex, int planeWidth, int planeHeight, bool writeArrowSlice);

    const unsigned char *getObstacle() const;
    void ensureArrowSliceCapacity(std::size_t requiredCount);
    void ensureScaleBufferCapacity(std::size_t requiredBlockCount);
    const VelocitySlice2D &getVelocitySlice2D() const;
    void LbmSolver::readProbeCell(ProbeResult &result, std::size_t index) const;
    std::pair<float, float> getValueRange() const;

private:
    Grid grid;
    dim3 blocks;
    dim3 threads;
    VelocitySlice2D velocitySlice;
    std::size_t arrowSliceCapacity_ = 0;
    std::size_t scaleBlockCapacity_ = 0;
    std::vector<float> h_blockMinimums;
    std::vector<float> h_blockMaximums;
    float omega_ = 1.0f;
};