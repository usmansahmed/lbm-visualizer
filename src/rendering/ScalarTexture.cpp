#include "rendering/ScalarTexture.hpp"

ScalarTexture::ScalarTexture() {
    // Create one OpenGL 2D texture for scalar float data.
    glGenTextures(1, &textureId_);

    bind();

    // Clamp at the edges so sampling outside the texture does not repeat.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Use nearest filtering so each simulation cell is shown directly.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

ScalarTexture::~ScalarTexture() {
    // Release the OpenGL texture.
    glDeleteTextures(1, &textureId_);
}

void ScalarTexture::bind(unsigned int textureUnit) const {
    // Bind this texture to the requested texture unit.
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureId_);
}

void ScalarTexture::allocate(int width, int height) {
    bind();

    width_ = width;
    height_ = height;

    // Allocate a single-channel 32-bit float texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
}

void ScalarTexture::update(int width, int height, const std::vector<float>& values) {

    bind();

    // Reallocate only when the slice dimensions change.
    if (width != width_ || height != height_) {
        allocate(width, height);
    }

    // Upload CPU-side scalar values into the texture.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, values.data());
}

void ScalarTexture::updateFromPBO(int width, int height, GLuint pbo) {
    bind();

    // Reallocate only when the slice dimensions change.
    if (width != width_ || height != height_) {
        allocate(width, height);
    }

    // When a PBO is bound, nullptr means offset 0 inside the PBO.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, nullptr);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}