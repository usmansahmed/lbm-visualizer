#include "ProbeOverlayBuilder.hpp"

#include <cstddef>

namespace
{

    void appendLine(
        std::vector<float> &vertices,
        float x1,
        float y1,
        float x2,
        float y2)
    {
        vertices.push_back(x1);
        vertices.push_back(y1);

        vertices.push_back(x2);
        vertices.push_back(y2);
    }

}

std::vector<float> buildProbeOverlayVertices(
    const ProbeResult &probe,
    SliceOrientation orientation,
    int currentSlice,
    int planeWidth,
    int planeHeight)
{
    std::vector<float> vertices;

    if (!probe.valid || planeWidth <= 0 || planeHeight <= 0)
    {
        return vertices;
    }

    int planeX = 0;
    int planeY = 0;

    bool probeIsOnCurrentSlice = false;

    switch (orientation)
    {
    case SliceOrientation::XY:
    {
        planeX = probe.x;
        planeY = probe.y;
        probeIsOnCurrentSlice = probe.z == currentSlice;
        break;
    }

    case SliceOrientation::XZ:
    {
        planeX = probe.x;
        planeY = probe.z;
        probeIsOnCurrentSlice = probe.y == currentSlice;
        break;
    }

    case SliceOrientation::YZ:
    {
        planeX = probe.y;
        planeY = probe.z;
        probeIsOnCurrentSlice = probe.x == currentSlice;

        break;
    }
    }

    if (!probeIsOnCurrentSlice)
    {
        return vertices;
    }

    if (planeX < 0 || planeX >= planeWidth ||
        planeY < 0 || planeY >= planeHeight)
    {
        return vertices;
    }

    const auto convertXToNdc = [planeWidth](float x)
    {
        return -1.0f + 2.0f * x / static_cast<float>(planeWidth);
    };

    const auto convertYToNdc = [planeHeight](float y)
    {
        return -1.0f + 2.0f * y / static_cast<float>(planeHeight);
    };

    const float left = convertXToNdc(static_cast<float>(planeX));
    const float right = convertXToNdc(static_cast<float>(planeX + 1));
    const float bottom = convertYToNdc(static_cast<float>(planeY));
    const float top = convertYToNdc(static_cast<float>(planeY + 1));

    appendLine(vertices, left, bottom, right, bottom);
    appendLine(vertices, right, bottom, right, top);
    appendLine(vertices, right, top, left, top);
    appendLine(vertices, left, top, left, bottom);

    const float centerX = 0.5f * (left + right);
    const float centerY = 0.5f * (bottom + top);
    const float cellWidth = right - left;
    const float cellHeight = top - bottom;

    const float crossHalfWidth = 1.25f * cellWidth;
    const float crossHalfHeight = 1.25f * cellHeight;

    appendLine(vertices, centerX - crossHalfWidth, centerY, centerX + crossHalfWidth, centerY);
    appendLine(vertices, centerX, centerY - crossHalfHeight, centerX, centerY + crossHalfHeight);

    return vertices;
}