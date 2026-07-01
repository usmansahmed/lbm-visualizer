#pragma once
#include <glad/gl.h>

class Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void drawQuad() const;

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};