#include "ScalarTexture.hpp"

ScalarTexture::ScalarTexture() {
    glGenTextures(1, &textureId_);

    bind();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

ScalarTexture::~ScalarTexture() {
    glDeleteTextures(1, &textureId_);
}

void ScalarTexture::bind(unsigned int textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureId_);
}

void ScalarTexture::allocate(int width, int height) {
    bind();
    width_ = width;
    height_ = height;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
}

void ScalarTexture::update(int width, int height, const std::vector<float>& values) {

    bind();
    if (width != width_ || height != height_) {
        allocate(width, height);
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, values.data());
}