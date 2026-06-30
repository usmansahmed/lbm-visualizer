#include "CudaCheck.hpp"

#include <cuda_runtime.h>

#include <iostream>

namespace
{
__global__ void incrementKernel(int* value)
{
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        *value += 1;
    }
}

bool checkCuda(cudaError_t result, const char* operation)
{
    if (result == cudaSuccess) {
        return true;
    }

    std::cerr << operation << " failed: "
              << cudaGetErrorString(result)
              << '\n';

    return false;
}
}

bool runCudaCheck()
{
    int deviceCount = 0;

    if (!checkCuda(
            cudaGetDeviceCount(&deviceCount),
            "cudaGetDeviceCount")) {
        return false;
    }

    if (deviceCount == 0) {
        std::cerr << "No CUDA-capable GPU was found.\n";
        return false;
    }

    cudaDeviceProp properties{};

    if (!checkCuda(
            cudaGetDeviceProperties(&properties, 0),
            "cudaGetDeviceProperties")) {
        return false;
    }

    std::cout << "CUDA GPU: " << properties.name << '\n';
    std::cout << "Compute capability: "
              << properties.major << '.'
              << properties.minor << '\n';

    int hostValue = 41;
    int* deviceValue = nullptr;

    if (!checkCuda(
            cudaMalloc(&deviceValue, sizeof(int)),
            "cudaMalloc")) {
        return false;
    }

    if (!checkCuda(
            cudaMemcpy(
                deviceValue,
                &hostValue,
                sizeof(int),
                cudaMemcpyHostToDevice),
            "Host-to-device copy")) {
        cudaFree(deviceValue);
        return false;
    }

    incrementKernel<<<1, 1>>>(deviceValue);

    if (!checkCuda(
            cudaGetLastError(),
            "CUDA kernel launch")) {
        cudaFree(deviceValue);
        return false;
    }

    if (!checkCuda(
            cudaDeviceSynchronize(),
            "CUDA kernel execution")) {
        cudaFree(deviceValue);
        return false;
    }

    if (!checkCuda(
            cudaMemcpy(
                &hostValue,
                deviceValue,
                sizeof(int),
                cudaMemcpyDeviceToHost),
            "Device-to-host copy")) {
        cudaFree(deviceValue);
        return false;
    }

    cudaFree(deviceValue);

    std::cout << "CUDA test result: " << hostValue << '\n';

    return hostValue == 42;
}