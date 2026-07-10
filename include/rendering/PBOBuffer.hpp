#pragma once

#include <cstddef>
#include <glad/gl.h>
#include <cuda_gl_interop.h>


class PBOBuffer {
    public:
    PBOBuffer();
    ~PBOBuffer();
    
    float* mapPBOToCuda(int width, int height);
    void unmapPBOFromCuda();
    GLuint getID() const;

    private:
    GLuint pbo_;
    cudaGraphicsResource* cudaPboResource_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    float* data_ = nullptr;

    void RegisterWithCuda();
    void UnregisterWithCuda();
    void resize(int width, int height);
};