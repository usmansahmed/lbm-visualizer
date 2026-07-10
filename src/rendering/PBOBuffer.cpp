#include "rendering/PBOBuffer.hpp"

#include <stdexcept>
#include <string>
#include <iostream>

namespace
{
    void checkCuda(cudaError_t error, const char *message)
    {
        if (error != cudaSuccess)
        {
            throw std::runtime_error(std::string(message) + ": " + cudaGetErrorString(error));
        }
    }
}

PBOBuffer::PBOBuffer()
{
    glGenBuffers(1, &pbo_);
}

PBOBuffer::~PBOBuffer()
{
    if (cudaPboResource_ != nullptr)
    {
        cudaError_t error =
            cudaGraphicsUnregisterResource(
                cudaPboResource_
            );

        if (error != cudaSuccess)
        {
            std::cerr << "Failed to unregister CUDA PBO resource: " << cudaGetErrorString(error) << '\n';
        }
        cudaPboResource_ = nullptr;
    }

    if (pbo_ != 0)
    {
        glDeleteBuffers(1, &pbo_);
        pbo_ = 0;
    }
}

void PBOBuffer::resize(int width, int height)
{
    width_ = width;
    height_ = height;

    const std::size_t byteSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * sizeof(float);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, static_cast<GLsizeiptr>(byteSize), nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void PBOBuffer::RegisterWithCuda()
{
    checkCuda(cudaGraphicsGLRegisterBuffer(&cudaPboResource_, pbo_, cudaGraphicsRegisterFlagsWriteDiscard), "Failed to register PBO with CUDA");
}

void PBOBuffer::UnregisterWithCuda()
{
    if (cudaPboResource_ != nullptr)
    {
        checkCuda(cudaGraphicsUnregisterResource(cudaPboResource_), "Failed to unregister CUDA PBO resource");
        cudaPboResource_ = nullptr;
    }
}

float *PBOBuffer::mapPBOToCuda(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        throw std::runtime_error("Cannot map PBO with invalid dimensions.");
    }

    if (width != width_ || height != height_ || cudaPboResource_ == nullptr)
    {
        UnregisterWithCuda();
        resize(width, height);
        RegisterWithCuda();
    }

    checkCuda(cudaGraphicsMapResources(1, &cudaPboResource_, 0), "Failed to map CUDA PBO resource");
    std::size_t mappedSize = 0;

    checkCuda(cudaGraphicsResourceGetMappedPointer(reinterpret_cast<void **>(&data_), &mappedSize, cudaPboResource_), "Failed to get mapped CUDA PBO pointer");

    const std::size_t expectedSize = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * sizeof(float);

    if (mappedSize < expectedSize)
    {
        throw std::runtime_error("Mapped PBO size is smaller than expected.");
    }
    return data_;
}

void PBOBuffer::unmapPBOFromCuda()
{
    if (cudaPboResource_ == nullptr)
    {
        return;
    }

    checkCuda(cudaGraphicsUnmapResources(1, &cudaPboResource_, 0), "Failed to unmap CUDA PBO resource");

    data_ = nullptr;
}

GLuint PBOBuffer::getID() const
{
    return pbo_;
}