#pragma once

#include <glad/gl.h>

#include <vector>

class ScalarTexture
{
public:
    ScalarTexture();
    ~ScalarTexture();

    ScalarTexture(const ScalarTexture &) = delete;
    ScalarTexture &operator=(const ScalarTexture &) = delete;

    void update(int width, int height, const std::vector<float> &values);
    void updateFromPBO(int width, int height, GLuint pbo);

    void bind(unsigned int textureUnit = 0) const;

    int width() const;
    int height() const;

    
private:
    GLuint textureId_ = 0;
    int width_ = 0;
    int height_ = 0;

    void allocate(int width, int height);
};