#include "geometry/ObstacleFactory.hpp"

#include <cmath>
#include <cstddef>

namespace
{
    std::size_t index3D(int x, int y, int z, int nx, int ny)
    {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(y) + static_cast<std::size_t>(ny) * static_cast<std::size_t>(z));
    }

    void addBlock(
        std::vector<unsigned char> &solid, int nx, int ny, int nz, int centerX, int centerY, int centerZ, int sizeX, int sizeY, int sizeZ)
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
}

std::vector<unsigned char> createObstacleMask(const SimulationConfig &config)
{
    const std::size_t count =
        static_cast<std::size_t>(config.nx) *
        static_cast<std::size_t>(config.ny) *
        static_cast<std::size_t>(config.nz);

    std::vector<unsigned char> solid(count, 0);

    switch (config.obstaclePreset)
    {
    case ObstaclePreset::None:
    {
        break;
    }

    case ObstaclePreset::SingleBlock:
    {
        addBlock(
            solid,
            config.nx,
            config.ny,
            config.nz,
            config.nx / 4,
            config.ny / 2,
            config.nz / 2,
            config.obstacleSize,
            config.obstacleSize,
            config.nz);

        break;
    }

    case ObstaclePreset::TwoBlocks:
    {
        addBlock(
            solid,
            config.nx,
            config.ny,
            config.nz,
            config.nx / 4,
            config.ny / 3,
            config.nz / 2,
            config.obstacleSize,
            config.obstacleSize,
            config.nz);

        addBlock(
            solid,
            config.nx,
            config.ny,
            config.nz,
            config.nx / 4,
            2 * config.ny / 3,
            config.nz / 2,
            config.obstacleSize,
            config.obstacleSize,
            config.nz);

        break;
    }

    case ObstaclePreset::SingleCylinder:
    {
        addCylinderAlongZ(
            solid,
            config.nx,
            config.ny,
            config.nz,
            config.nx / 4,
            config.ny / 2,
            config.obstacleRadius);

        break;
    }

    case ObstaclePreset::TwoCylinders:
    {
        addCylinderAlongZ(
            solid,
            config.nx,
            config.ny,
            config.nz,
            config.nx / 4,
            config.ny / 3,
            config.obstacleRadius);

        addCylinderAlongZ(
            solid,
            config.nx,
            config.ny,
            config.nz,
            config.nx / 4,
            2 * config.ny / 3,
            config.obstacleRadius);

        break;
    }
    }

    return solid;
}