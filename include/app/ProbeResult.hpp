#pragma once

struct ProbeResult
{
    bool valid = false;

    int x = 0;
    int y = 0;
    int z = 0;

    float density = 0.0f;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;

    float speed = 0.0f;

    bool obstacle = false;
};

struct ViewportRectangle
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};