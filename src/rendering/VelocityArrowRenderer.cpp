#include "rendering/VelocityArrowRenderer.hpp"

VelocityArrowRenderer::VelocityArrowRenderer()
{
    // Create the VAO and VBO used for line-based overlays.
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // Start with an empty dynamic buffer. The arrow vertices are updated when needed.
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Each vertex contains only a 2D position in normalized device coordinates.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

VelocityArrowRenderer::~VelocityArrowRenderer()
{
    // Release OpenGL resources owned by this renderer.
    if (vbo_ != 0)
    {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }

    if (vao_ != 0)
    {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

void VelocityArrowRenderer::update(const std::vector<float> &vertices)
{
    // Two floats make one 2D vertex.
    vertexCount_ = static_cast<GLsizei>(vertices.size() / 2);

    // Upload the latest arrow or overlay line vertices.
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VelocityArrowRenderer::draw() const
{
    if (vertexCount_ == 0)
    {
        return;
    }

    // Draw all stored vertices as independent line segments.
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, vertexCount_);
    glBindVertexArray(0);
}