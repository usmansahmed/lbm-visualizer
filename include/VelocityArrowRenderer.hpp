
#pragma once

#include <glad/gl.h>

#include <vector>

class VelocityArrowRenderer
{
public:
    VelocityArrowRenderer();
    ~VelocityArrowRenderer();

    VelocityArrowRenderer(const VelocityArrowRenderer&) = delete;
    VelocityArrowRenderer& operator=(const VelocityArrowRenderer&) = delete;

    void update(const std::vector<float>& vertices);
    void draw() const;

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    GLsizei vertexCount_ = 0;
};