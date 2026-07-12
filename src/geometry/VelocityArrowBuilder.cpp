#include "geometry/VelocityArrowBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace
{
    struct Sample
    {
        int x = 0;
        int y = 0;
        int z = 0;

        // Coordinates inside the selected 2D plane.
        int planeX = 0;
        int planeY = 0;

        // Velocity components shown by the 2D arrow.
        float velocityHorizontal = 0.0f;
        float velocityVertical = 0.0f;
    };

    Sample makeSample(const VisualizationState &frame, const VelocitySlice2D& velocities, SliceOrientation orientation,
                      int planeX, int planeY, int sliceIndex)
    {
        Sample sample;

        sample.planeX = planeX;
        sample.planeY = planeY;

        // Convert the 2D slice coordinate back to the matching 3D grid cell.
        switch (orientation)
        {
            case SliceOrientation::XY:
            {
                sample.x = planeX;
                sample.y = planeY;
                sample.z = sliceIndex;
                break;
            }

            case SliceOrientation::XZ:
            {
                sample.x = planeX;
                sample.y = sliceIndex;
                sample.z = planeY;
                break;
            }

            case SliceOrientation::YZ:
            {
                sample.x = sliceIndex;
                sample.y = planeX;
                sample.z = planeY;
                break;
            }
        }

        // VelocitySlice2D is already a 2D slice, so use the 2D index here.
        const std::size_t index2D = static_cast<std::size_t>(planeX) + static_cast<std::size_t>(velocities.width) * static_cast<std::size_t>(planeY);


        sample.velocityHorizontal = velocities.horizontal[index2D];
        sample.velocityVertical = velocities.vertical[index2D];

        return sample;
    }

    void appendLine(
        std::vector<float> &vertices, float x1, float y1, float x2, float y2)
    {
        // Store one line segment as two 2D vertices.
        vertices.push_back(x1);
        vertices.push_back(y1);

        vertices.push_back(x2);
        vertices.push_back(y2);
    }

}

std::vector<float> buildVelocityArrowVertices(const VisualizationState &frame, const VelocitySlice2D& velocities, SliceOrientation orientation,
                                              int sliceIndex, int stride, float arrowLengthScale)
{
    if (stride <= 0)
    {
        throw std::invalid_argument("Arrow stride must be positive.");
    }

    int planeWidth = 0;
    int planeHeight = 0;

    getSliceDimensions(orientation, frame.nx, frame.ny, frame.nz, planeWidth, planeHeight);

    const int maximumSlice = getMaximumSliceIndex(orientation, frame.nx, frame.ny, frame.nz);

    if (sliceIndex < 0 || sliceIndex > maximumSlice)
    {
        throw std::out_of_range("Arrow slice index is invalid.");
    }

    std::vector<Sample> samples;

    float maximumMagnitude = 0.0f;

    // Sample the slice on a regular grid instead of drawing one arrow per cell.
    for (int planeY = stride / 2; planeY < planeHeight; planeY += stride)
    {
        for (int planeX = stride / 2; planeX < planeWidth; planeX += stride)
        {
            Sample sample = makeSample(frame, velocities, orientation, planeX, planeY, sliceIndex);
            const std::size_t sourceIndex = index3D(sample.x, sample.y, sample.z, frame.nx, frame.ny);

            // Do not draw arrows inside obstacle cells.
            if (!frame.obstacle.empty() && frame.obstacle[sourceIndex] != 0)
            {
                continue;
            }

            const float magnitude = std::sqrt(sample.velocityHorizontal * sample.velocityHorizontal +
                                              sample.velocityVertical * sample.velocityVertical);

            maximumMagnitude = std::max(maximumMagnitude, magnitude);

            samples.push_back(sample);
        }
    }

    std::vector<float> vertices;

    constexpr float nearZero = 1.0e-8f;

    // If all in-plane velocities are almost zero, there is nothing useful to draw.
    if (maximumMagnitude < nearZero)
    {
        return vertices;
    }

    vertices.reserve(samples.size() * 6 * 2);

    const float maximumLengthInCells = arrowLengthScale * static_cast<float>(stride);

    // Convert slice cell coordinates into OpenGL normalized device coordinates.
    auto convertToNdc = [planeWidth, planeHeight](float cellX, float cellY)
    {
        const float ndcX = -1.0f + 2.0f * cellX / static_cast<float>(planeWidth);
        const float ndcY = -1.0f + 2.0f * cellY / static_cast<float>(planeHeight);
        return std::pair<float, float>{ndcX, ndcY};
    };

    for (const Sample &sample : samples)
    {
        const float vx = sample.velocityHorizontal;
        const float vy = sample.velocityVertical;

        const float magnitude = std::sqrt(vx * vx + vy * vy);
        if (magnitude < nearZero)
        {
            continue;
        }

        // Normalize velocity so it can be used only as a direction.
        const float directionX = vx / magnitude;
        const float directionY = vy / magnitude;

        const float normalizedMagnitude = std::clamp(magnitude / maximumMagnitude, 0.0f, 1.0f);

        const float arrowLength = maximumLengthInCells * normalizedMagnitude;

        if (arrowLength < 0.05f)
        {
            continue;
        }

        // Start the arrow in the center of the sampled cell.
        const float startX = static_cast<float>(sample.planeX) + 0.5f;
        const float startY = static_cast<float>(sample.planeY) + 0.5f;

        const float tipX = startX + directionX * arrowLength;
        const float tipY = startY + directionY * arrowLength;

        const float headLength = 0.30f * arrowLength;
        const float headWidth = 0.18f * arrowLength;

        const float perpendicularX = -directionY;
        const float perpendicularY = directionX;

        const float headBaseX = tipX - directionX * headLength;
        const float headBaseY = tipY - directionY * headLength;

        const float leftX = headBaseX + perpendicularX * headWidth;
        const float leftY = headBaseY + perpendicularY * headWidth;

        const float rightX = headBaseX - perpendicularX * headWidth;
        const float rightY = headBaseY - perpendicularY * headWidth;

        const auto [startNdcX, startNdcY] = convertToNdc(startX, startY);
        const auto [tipNdcX, tipNdcY] = convertToNdc(tipX, tipY);

        const auto [leftNdcX, leftNdcY] = convertToNdc(leftX, leftY);
        const auto [rightNdcX, rightNdcY] = convertToNdc(rightX, rightY);

        // Main arrow line plus two small lines for the arrow head.
        appendLine(vertices, startNdcX, startNdcY, tipNdcX, tipNdcY);
        appendLine(vertices, tipNdcX, tipNdcY, leftNdcX, leftNdcY);
        appendLine(vertices, tipNdcX, tipNdcY, rightNdcX, rightNdcY);
    }

    return vertices;
}