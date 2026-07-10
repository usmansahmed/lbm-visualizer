#pragma once

#include "lbm/grid.hpp"
#include "app/Enums.hpp"
#include "app/VelocitySlice.hpp"
#include "app/ProbeResult.hpp"
#include "app/SimulationConfig.hpp"

#include <utility>
#include <cuda_runtime.h>

class LbmSolver
{
public:
    LbmSolver(const SimulationConfig &config, const std::vector<unsigned char> &obstacle);
    ~LbmSolver();

    void step();
    void prepareVisualizationSlice(float *mappedPixels, DisplayField field, SliceOrientation orientation,
                                   int sliceIndex, int planeWidth, int planeHeight,
                                   bool writeArrowSlice, bool automaticScaling);

    const unsigned char *getObstacle() const;
    const VelocitySlice2D &getVelocitySlice2D() const;
    void LbmSolver::readProbeCell(ProbeResult &result, std::size_t index) const;
    std::pair<float, float> getValueRange() const;

private:
    LbmGridData gridData_;
    dim3 blocks_;
    dim3 threads_;
    VelocitySlice2D velocitySlice_;
    std::size_t arrowSliceCapacity_ = 0;
    std::size_t scaleBlockCapacity_ = 0;
    std::vector<float> h_blockMinimums_;
    std::vector<float> h_blockMaximums_;
    float omega_ = 1.0f;

    void ensureArrowSliceCapacity(std::size_t requiredCount);
    void ensureScaleBufferCapacity(std::size_t requiredBlockCount);
};