#include "geometry/ObstacleFactory.hpp"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <random>

namespace
{
    std::size_t index3D(int x, int y, int z, int nx, int ny)
    {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(y) + static_cast<std::size_t>(ny) * static_cast<std::size_t>(z));
    }

    void setSolid(std::vector<unsigned char> &solid, int nx, int ny, int nz, int x, int y, int z)
    {
        if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
        {
            return;
        }
        solid[index3D(x, y, z, nx, ny)] = 1;
    }

    void addBlock(std::vector<unsigned char> &solid, int nx, int ny, int nz, int centerX, int centerY, int centerZ, int sizeX, int sizeY, int sizeZ)
    {
        for (int z = centerZ - sizeZ / 2; z < centerZ + sizeZ / 2; ++z)
        {
            for (int y = centerY - sizeY / 2; y < centerY + sizeY / 2; ++y)
            {
                for (int x = centerX - sizeX / 2; x < centerX + sizeX / 2; ++x)
                {
                    if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
                    {
                        continue;
                    }
                    solid[index3D(x, y, z, nx, ny)] = 1;
                }
            }
        }
    }

    void addCylinderAlongZ(std::vector<unsigned char> &solid, int nx, int ny, int nz, int centerX, int centerY, int radius)
    {
        const int r2 = radius * radius;
        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < ny; ++y)
            {
                for (int x = 0; x < nx; ++x)
                {
                    const int dx = x - centerX;
                    const int dy = y - centerY;

                    if (dx * dx + dy * dy <= r2)
                    {
                        solid[index3D(x, y, z, nx, ny)] = 1;
                    }
                }
            }
        }
    }

    void addCylinderArray(std::vector<unsigned char> &solid, int nx, int ny, int nz,
                          int radius, int spacingX, int spacingY)
    {
        spacingX = std::max(2 * radius + 2, spacingX);
        spacingY = std::max(2 * radius + 2, spacingY);

        const int startX = nx / 4;
        const int endX = nx / 2;

        for (int x = startX; x <= endX; x += spacingX)
        {
            for (int y = spacingY; y < ny; y += spacingY)
            {
                addCylinderAlongZ(solid, nx, ny, nz,
                                  x, y, radius);
            }
        }
    }

    void addSphere(std::vector<unsigned char> &solid, int nx, int ny, int nz,
                   int centerX, int centerY, int centerZ, int radius)
    {
        const int radiusSquared = radius * radius;

        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < ny; ++y)
            {
                for (int x = 0; x < nx; ++x)
                {
                    const int dx = x - centerX;
                    const int dy = y - centerY;
                    const int dz = z - centerZ;

                    if (dx * dx + dy * dy + dz * dz <= radiusSquared)
                    {
                        setSolid(solid, nx, ny, nz, x, y, z);
                    }
                }
            }
        }
    }

    void addBackwardFacingStep(std::vector<unsigned char> &solid, int nx, int ny, int nz, int stepLength, int stepHeight)
    {
        stepLength = std::clamp(stepLength, 1, nx);
        stepHeight = std::clamp(stepHeight, 1, ny - 1);
        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < stepHeight; ++y)
            {
                for (int x = 0; x < stepLength; ++x)
                {
                    setSolid(solid, nx, ny, nz, x, y, z);
                }
            }
        }
    }

    void addWallWithSlit(std::vector<unsigned char> &solid, int nx, int ny, int nz,
                         int wallX, int wallThickness, int slitCenterY, int slitHeight)
    {
        wallX = std::clamp(wallX, 0, nx - 1);
        wallThickness = std::max(1, wallThickness);
        slitHeight = std::clamp(slitHeight, 1, ny);

        const int slitMinY = slitCenterY - slitHeight / 2;
        const int slitMaxY = slitCenterY + slitHeight / 2;
        const int maxWallX = std::min(nx, wallX + wallThickness);

        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < ny; ++y)
            {
                const bool insideSlit = y >= slitMinY && y <= slitMaxY;
                if (insideSlit)
                {
                    continue;
                }

                for (int x = wallX; x < maxWallX; ++x)
                {
                    setSolid(solid, nx, ny, nz, x, y, z);
                }
            }
        }
    }

    void addRandomPorousBlock(
        std::vector<unsigned char> &solid, int nx, int ny, int nz,
        int minX, int maxX, float solidProbability, unsigned int seed)
    {
        minX = std::clamp(minX, 0, nx);
        maxX = std::clamp(maxX, 0, nx);
        if (maxX <= minX)
        {
            return;
        }

        solidProbability = std::clamp(solidProbability, 0.0f, 1.0f);
        std::mt19937 generator(seed);
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < ny; ++y)
            {
                for (int x = minX; x < maxX; ++x)
                {
                    const float randomValue = distribution(generator);
                    if (randomValue < solidProbability)
                    {
                        setSolid(solid, nx, ny, nz, x, y, z);
                    }
                }
            }
        }
    }
}

std::vector<unsigned char> createObstacleMask(const SimulationConfig &config)
{
    const std::size_t count = static_cast<std::size_t>(config.nx) * static_cast<std::size_t>(config.ny) * static_cast<std::size_t>(config.nz);

    std::vector<unsigned char> solid(count, 0);

    switch (config.obstaclePreset)
    {
    case ObstaclePreset::None:
    {
        break;
    }

    case ObstaclePreset::SingleBlock:
    {
        addBlock(solid, config.nx, config.ny, config.nz,
                 config.nx / 4, config.ny / 2, config.nz / 2,
                 config.obstacleSize, config.obstacleSize, config.nz);

        break;
    }

    case ObstaclePreset::TwoBlocks:
    {
        addBlock(solid, config.nx, config.ny, config.nz,
                 config.nx / 4, config.ny / 3, config.nz / 2,
                 config.obstacleSize, config.obstacleSize, config.nz);

        addBlock(solid, config.nx, config.ny, config.nz,
                 config.nx / 4, 2 * config.ny / 3, config.nz / 2,
                 config.obstacleSize, config.obstacleSize, config.nz);

        break;
    }

    case ObstaclePreset::SingleCylinder:
    {
        addCylinderAlongZ(solid, config.nx, config.ny, config.nz,
                          config.nx / 4, config.ny / 2, config.obstacleRadius);

        break;
    }

    case ObstaclePreset::TwoCylinders:
    {
        addCylinderAlongZ(solid, config.nx, config.ny, config.nz,
                          config.nx / 4, config.ny / 3, config.obstacleRadius);

        addCylinderAlongZ(solid, config.nx, config.ny, config.nz,
                          config.nx / 4, 2 * config.ny / 3, config.obstacleRadius);

        break;
    }
    case ObstaclePreset::CylinderArray:
    {
        addCylinderArray(solid, config.nx, config.ny, config.nz,
                         config.obstacleRadius, config.obstacleSpacingX, config.obstacleSpacingY);

        break;
    }

    case ObstaclePreset::Sphere:
    {
        addSphere(solid, config.nx, config.ny, config.nz,
                  config.nx / 4, config.ny / 2, config.nz / 2, config.obstacleRadius);

        break;
    }

    case ObstaclePreset::TwoSpheres:
    {
        addSphere(solid, config.nx, config.ny, config.nz,
                  config.nx / 4, config.ny / 3, config.nz / 2, config.obstacleRadius);

        addSphere(solid, config.nx, config.ny, config.nz,
                  config.nx / 4, 2 * config.ny / 3, config.nz / 2, config.obstacleRadius);

        break;
    }

    case ObstaclePreset::BackwardFacingStep:
    {
        addBackwardFacingStep(solid, config.nx, config.ny, config.nz, config.nx / 5, config.stepHeight);
        break;
    }

    case ObstaclePreset::WallWithSlit:
    {
        addWallWithSlit(solid, config.nx, config.ny, config.nz,
                        config.nx / 3, std::max(1, config.obstacleSize / 4), config.ny / 2, config.slitHeight);

        break;
    }

    case ObstaclePreset::RandomPorousBlock:
    {
        addRandomPorousBlock(solid, config.nx, config.ny, config.nz,
                             config.nx / 3, 2 * config.nx / 3, config.porousSolidProbability, config.porousRandomSeed);

        break;
    }
    }

    return solid;
}