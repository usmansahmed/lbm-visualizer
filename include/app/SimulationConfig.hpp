#pragma once

enum class ObstaclePreset
{
    None,
    SingleBlock,
    TwoBlocks,
    SingleCylinder,
    TwoCylinders,
    CylinderArray,
    Sphere,
    TwoSpheres,
    BackwardFacingStep,
    WallWithSlit,
    RandomPorousBlock
};

struct SimulationConfig
{
    int nx = 256;
    int ny = 64;
    int nz = 16;

    float omega = 1.0f;
    float inletVelocity = 0.02f;
    float initialDensity = 1.0f;

    ObstaclePreset obstaclePreset = ObstaclePreset::SingleBlock;

    int obstacleSize = 8;
    int obstacleRadius = 5;

    int obstacleSpacingX = 24;
    int obstacleSpacingY = 16;

    int stepHeight = 24;
    int slitHeight = 20;

    float porousSolidProbability = 0.15f;
    unsigned int porousRandomSeed = 12345u;
};