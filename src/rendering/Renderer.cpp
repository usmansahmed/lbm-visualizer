#include "rendering/Renderer.hpp"

Renderer::Renderer() {

    // Full-screen quad with position and texture coordinates.
    float vertices[] = {
        1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, 0.0f
    };

    // Upload the quad vertex data to the GPU.
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Store the vertex layout in a VAO.
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Attribute 0: 3D position.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Attribute 1: 2D texture coordinate.
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

Renderer::~Renderer() {
    // Delete OpenGL resources owned by this renderer.
    if (vao_ != 0) 
    {
        glDeleteVertexArrays(1, &vao_);
    }
    if (vbo_ != 0)
    {
        glDeleteBuffers(1, &vbo_);
    }
}

void Renderer::drawQuad() const {

    // Draw the quad as a triangle strip using the currently bound shader and textures.
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}