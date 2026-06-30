#include "CudaCheck.hpp"

#include <tinyxml2.h>

#include <array>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <cstdlib>
#include <iostream>
#include <cmath>
#include <vector>

#include <filesystem>
#include <iomanip>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

enum class SliceOrientation
{
    XY,
    XZ,
    YZ
};

struct ApplicationState
{
    int nx = 0;
    int ny = 0;
    int nz = 0;
    int currentSlice = 0;
    int maximumSlice = 0;
    bool sliceChanged = true;
    bool orientationChanged = true;
    SliceOrientation orientation = SliceOrientation::XY;
};

struct VtuPointData
{
    std::vector<float> density;

    std::vector<float> velocityX;
    std::vector<float> velocityY;
    std::vector<float> velocityZ;
};

std::filesystem::path makeVtiFilePath(
    const std::filesystem::path& directory,
    int fileNumber)
{
    std::ostringstream filename;

    filename
        << "channel_"
        << std::setw(5)
        << std::setfill('0')
        << fileNumber
        << ".vti";

    return directory / filename.str();
}

tinyxml2::XMLElement* findDataArray(
    tinyxml2::XMLElement* parent,
    std::string_view requiredName)
{
    if (parent == nullptr)
        return nullptr;

    for (
        tinyxml2::XMLElement* array =
            parent->FirstChildElement("DataArray");

        array != nullptr;

        array =
            array->NextSiblingElement("DataArray"))
    {
        const char* name =
            array->Attribute("Name");

        if (
            name != nullptr &&
            requiredName == name
        )
        {
            return array;
        }
    }

    return nullptr;
}

std::array<int, 6> parseExtent(
    const char* extentText)
{
    if (extentText == nullptr)
    {
        throw std::runtime_error(
            "VTI extent is missing."
        );
    }

    std::istringstream stream{
        std::string{extentText}
    };

    std::array<int, 6> extent{};

    if (!(stream
            >> extent[0]
            >> extent[1]
            >> extent[2]
            >> extent[3]
            >> extent[4]
            >> extent[5]))
    {
        throw std::runtime_error(
            "Failed to parse VTI extent."
        );
    }

    return extent;
}

void loadAsciiVtiVelocity(
    const std::string& filePath,
    int& nx,
    int& ny,
    int& nz,
    std::vector<float>& velocityX,
    std::vector<float>& velocityY,
    std::vector<float>& velocityZ)
{
    tinyxml2::XMLDocument document;

    const tinyxml2::XMLError loadResult =
        document.LoadFile(filePath.c_str());

    if (loadResult != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error(
            "Could not load VTI file: " +
            filePath +
            "\nTinyXML2 error: " +
            document.ErrorStr()
        );
    }

    tinyxml2::XMLElement* vtkFile =
        document.FirstChildElement("VTKFile");

    if (vtkFile == nullptr)
    {
        throw std::runtime_error(
            "VTKFile element was not found."
        );
    }

    const char* vtkType =
        vtkFile->Attribute("type");

    if (
        vtkType == nullptr ||
        std::string_view{vtkType} != "ImageData"
    )
    {
        throw std::runtime_error(
            "The file is not a VTI ImageData file."
        );
    }

    tinyxml2::XMLElement* imageData =
        vtkFile->FirstChildElement("ImageData");

    if (imageData == nullptr)
    {
        throw std::runtime_error(
            "ImageData element was not found."
        );
    }

    tinyxml2::XMLElement* piece =
        imageData->FirstChildElement("Piece");

    if (piece == nullptr)
    {
        throw std::runtime_error(
            "Piece element was not found."
        );
    }

    /*
     * Normally Piece has Extent. WholeExtent is used
     * as a fallback.
     */
    const char* extentText =
        piece->Attribute("Extent");

    if (extentText == nullptr)
    {
        extentText =
            imageData->Attribute("WholeExtent");
    }

    const std::array<int, 6> extent =
        parseExtent(extentText);

    nx = extent[1] - extent[0] + 1;
    ny = extent[3] - extent[2] + 1;
    nz = extent[5] - extent[4] + 1;

    if (nx <= 0 || ny <= 0 || nz <= 0)
    {
        throw std::runtime_error(
            "Invalid VTI grid dimensions."
        );
    }

    const std::size_t pointCount =
        static_cast<std::size_t>(nx) *
        static_cast<std::size_t>(ny) *
        static_cast<std::size_t>(nz);

    tinyxml2::XMLElement* pointData =
        piece->FirstChildElement("PointData");

    if (pointData == nullptr)
    {
        throw std::runtime_error(
            "PointData element was not found."
        );
    }

    tinyxml2::XMLElement* velocityArray =
        findDataArray(
            pointData,
            "Velocity"
        );

    if (velocityArray == nullptr)
    {
        throw std::runtime_error(
            "Velocity DataArray was not found."
        );
    }

    const char* format =
        velocityArray->Attribute("format");

    if (
        format == nullptr ||
        std::string_view{format} != "ascii"
    )
    {
        throw std::runtime_error(
            "This loader supports only "
            "format=\"ascii\" VTI files."
        );
    }

    int numberOfComponents = 1;

    velocityArray->QueryIntAttribute(
        "NumberOfComponents",
        &numberOfComponents
    );

    if (numberOfComponents != 3)
    {
        throw std::runtime_error(
            "Velocity must contain three components."
        );
    }

    const char* velocityText =
        velocityArray->GetText();

    if (velocityText == nullptr)
    {
        throw std::runtime_error(
            "Velocity DataArray is empty."
        );
    }

    velocityX.resize(pointCount);
    velocityY.resize(pointCount);
    velocityZ.resize(pointCount);

    std::istringstream velocityStream{
        std::string{velocityText}
    };

    for (std::size_t i = 0; i < pointCount; ++i)
    {
        if (!(velocityStream
                >> velocityX[i]
                >> velocityY[i]
                >> velocityZ[i]))
        {
            throw std::runtime_error(
                "Failed to read velocity vector " +
                std::to_string(i)
            );
        }
    }

    std::cout
        << "Loaded: " << filePath << '\n'
        << "Grid dimensions: "
        << nx << " x "
        << ny << " x "
        << nz << '\n'
        << "Velocity vectors: "
        << pointCount << '\n';
}

int getMaximumSliceIndex(
    SliceOrientation orientation,
    int nx,
    int ny,
    int nz)
{
    switch (orientation)
    {
        case SliceOrientation::XY:
            return nz - 1;

        case SliceOrientation::XZ:
            return ny - 1;

        case SliceOrientation::YZ:
            return nx - 1;
    }

    return 0;
}

void framebufferSizeCallback(
    GLFWwindow* window,
    int width,
    int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

void keyCallback(
    GLFWwindow* window,
    int key,
    int scancode,
    int action,
    int mods)
{
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS)
        return;

    auto* state = static_cast<ApplicationState*>(
        glfwGetWindowUserPointer(window)
    );

    if (state == nullptr)
        return;

    if (key == GLFW_KEY_X) {
        if (state->orientation != SliceOrientation::YZ) {
            state->orientation = SliceOrientation::YZ;
            state->currentSlice = state->nx / 2;
            state->maximumSlice = getMaximumSliceIndex(state->orientation, state->nx, state->ny, state->nz);
            state->orientationChanged = true;
            state->sliceChanged = true;
        }
    }

    if (key == GLFW_KEY_Y) {
        if (state->orientation != SliceOrientation::XZ) {
            state->orientation = SliceOrientation::XZ;
            state->currentSlice = state->ny / 2;
            state->maximumSlice = getMaximumSliceIndex(state->orientation, state->nx, state->ny, state->nz);
            state->orientationChanged = true;
            state->sliceChanged = true;
        }
    }

    if (key == GLFW_KEY_Z) {
        if (state->orientation != SliceOrientation::XY) {
            state->orientation = SliceOrientation::XY;
            state->currentSlice = state->nz / 2;
            state->maximumSlice = getMaximumSliceIndex(state->orientation, state->nx, state->ny, state->nz);
            state->orientationChanged = true;
            state->sliceChanged = true;
        }
    }

    if (key == GLFW_KEY_RIGHT)
    {
        state->currentSlice =
            std::min(
                state->currentSlice + 1,
                state->maximumSlice
            );

        state->sliceChanged = true;
    }

    if (key == GLFW_KEY_LEFT)
    {
        state->currentSlice =
            std::max(
                state->currentSlice - 1,
                0
            );

        state->sliceChanged = true;
    }
}

std::size_t index3D(
    int x,
    int y,
    int z,
    int nx,
    int ny)
{
    return static_cast<std::size_t>(x)
         + static_cast<std::size_t>(nx)
         * (
             static_cast<std::size_t>(y)
             + static_cast<std::size_t>(ny) * z
           );
}

void getSliceDimensions(
    SliceOrientation orientation,
    int nx,
    int ny,
    int nz,
    int& width,
    int& height)
{
    switch (orientation)
    {
        case SliceOrientation::XY:
            width = nx;
            height = ny;
            break;

        case SliceOrientation::XZ:
            width = nx;
            height = nz;
            break;

        case SliceOrientation::YZ:
            width = ny;
            height = nz;
            break;
    }
}

void extractXYSlice(
    const std::vector<float>& field,
    std::vector<float>& slice,
    int nx,
    int ny,
    int nz,
    int fixedZ)
{   
    slice.resize(nx*ny);
    for (int y = 0; y < ny; ++y)
    {
        for (int x = 0; x < nx; ++x)
        {
            const std::size_t sourceIndex =
                index3D(x, y, fixedZ, nx, ny);

            const std::size_t destinationIndex =
                static_cast<std::size_t>(x)
                + static_cast<std::size_t>(nx) * y;

            slice[destinationIndex] =
                field[sourceIndex];
        }
    }
}

void extractXZSlice(
    const std::vector<float>& field,
    std::vector<float>& slice,
    int nx,
    int ny,
    int nz,
    int fixedY)
{   
    slice.resize(nx*nz);
    for (int z = 0; z < nz; ++z)
    {
        for (int x = 0; x < nx; ++x)
        {
            const std::size_t sourceIndex =
                index3D(x, fixedY, z, nx, ny);

            const std::size_t destinationIndex =
                static_cast<std::size_t>(x)
                + static_cast<std::size_t>(nx) * z;

            slice[destinationIndex] =
                field[sourceIndex];
        }
    }
}

void extractYZSlice(
    const std::vector<float>& field,
    std::vector<float>& slice,
    int nx,
    int ny,
    int nz,
    int fixedX)
{   
    slice.resize(ny*nz);
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            const std::size_t sourceIndex =
                index3D(fixedX, y, z, nx, ny);

            const std::size_t destinationIndex =
                static_cast<std::size_t>(y)
                + static_cast<std::size_t>(ny) * z;

            slice[destinationIndex] =
                field[sourceIndex];
        }
    }
}

void extractVelocityMagnitudeXYSlice(
    const std::vector<float>& velocityX,
    const std::vector<float>& velocityY,
    const std::vector<float>& velocityZ,
    std::vector<float>& slice,
    int nx,
    int ny,
    int nz,
    int fixedZ)
{

    slice.resize(
        static_cast<std::size_t>(nx) *
        static_cast<std::size_t>(ny)
    );

    for (int y = 0; y < ny; ++y)
    {
        for (int x = 0; x < nx; ++x)
        {
            const std::size_t sourceIndex =
                index3D(x, y, fixedZ, nx, ny);

            const float ux = velocityX[sourceIndex];
            const float uy = velocityY[sourceIndex];
            const float uz = velocityZ[sourceIndex];

            const float speed =
                std::sqrt(
                    ux * ux +
                    uy * uy +
                    uz * uz
                );

            const std::size_t destinationIndex =
                static_cast<std::size_t>(x)
                + static_cast<std::size_t>(nx) * y;

            slice[destinationIndex] = speed;
        }
    }
}

void extractVelocityMagnitudeYZSlice(
    const std::vector<float>& velocityX,
    const std::vector<float>& velocityY,
    const std::vector<float>& velocityZ,
    std::vector<float>& slice,
    int nx,
    int ny,
    int nz,
    int fixedX)
{
    slice.resize(
        static_cast<std::size_t>(ny) *
        static_cast<std::size_t>(nz)
    );

    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            const std::size_t sourceIndex =
                index3D(fixedX, y, z, nx, ny);

            const float ux = velocityX[sourceIndex];
            const float uy = velocityY[sourceIndex];
            const float uz = velocityZ[sourceIndex];

            const float speed =
                std::sqrt(
                    ux * ux +
                    uy * uy +
                    uz * uz
                );

            const std::size_t destinationIndex =
                static_cast<std::size_t>(y)
                + static_cast<std::size_t>(ny) * z;

            slice[destinationIndex] = speed;
        }
    }
}

void extractVelocityMagnitudeXZSlice(
    const std::vector<float>& velocityX,
    const std::vector<float>& velocityY,
    const std::vector<float>& velocityZ,
    std::vector<float>& slice,
    int nx,
    int ny,
    int nz,
    int fixedY)
{
    slice.resize(
        static_cast<std::size_t>(nx) *
        static_cast<std::size_t>(nz)
    );

    for (int z = 0; z < nz; ++z)
    {
        for (int x = 0; x < nx; ++x)
        {
            const std::size_t sourceIndex =
                index3D(x, fixedY, z, nx, ny);

            const float ux = velocityX[sourceIndex];
            const float uy = velocityY[sourceIndex];
            const float uz = velocityZ[sourceIndex];

            const float speed =
                std::sqrt(
                    ux * ux +
                    uy * uy +
                    uz * uz
                );

            const std::size_t destinationIndex =
                static_cast<std::size_t>(x)
                + static_cast<std::size_t>(nx) * z;

            slice[destinationIndex] = speed;
        }
    }
}

std::vector<float> createDummySphere(int nx, int ny, int nz)
{
    const float centerX = 0.5f * static_cast<float>(nx - 1);
    const float centerY = 0.5f * static_cast<float>(ny - 1);
    const float centerZ = 0.5f * static_cast<float>(nz - 1);

    std::vector<float> data(nx*ny*nz);
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const float dx =
                    (static_cast<float>(x) - centerX) /
                    static_cast<float>(nx);

                const float dy =
                    (static_cast<float>(y) - centerY) /
                    static_cast<float>(ny);

                const float dz =
                    (static_cast<float>(z) - centerZ) /
                    static_cast<float>(nz);

                const float distanceSquared =
                    dx * dx + dy * dy + dz * dz;

                const float value =
                    std::exp(-100.0f * distanceSquared);

                data[index3D(x, y, z, nx, ny)] =
                    value;
            }
        }
    }
    return data;
}

void createDummyVelocityField(
    std::vector<float>& velocityX,
    std::vector<float>& velocityY,
    std::vector<float>& velocityZ,
    int nx,
    int ny,
    int nz, float time)
{
    const float centerX = 0.5f * static_cast<float>(nx - 1);
    const float centerY = 0.5f * static_cast<float>(ny - 1);
    const float centerZ = 0.5f * static_cast<float>(nz - 1);

    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const float px =
                    (static_cast<float>(x) - centerX) /
                    static_cast<float>(nx);

                const float py =
                    (static_cast<float>(y) - centerY) /
                    static_cast<float>(ny);

                const float pz =
                    (static_cast<float>(z) - centerZ) /
                    static_cast<float>(nz);

                const std::size_t index =
                    index3D(x, y, z, nx, ny);

                velocityX[index] = -py * std::cos(time);
                velocityY[index] = px * std::sin(time);
                velocityZ[index] =0.1f * std::sin(12.0f * pz + time);
            }
        }
    }
}

int main()
{
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }

    GLFWwindow* window = glfwCreateWindow(
        720,
        720,
        "LBM Visualizer",
        nullptr,
        nullptr
    );

    if (window == nullptr) {
        std::cerr << "Failed to create the GLFW window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);

    const int gladVersion = gladLoadGL(glfwGetProcAddress);

    if (gladVersion == 0) {
        std::cerr << "Failed to initialize GLAD.\n";

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    glfwSetFramebufferSizeCallback(
        window,
        framebufferSizeCallback
    );
    glfwSetKeyCallback(window, keyCallback);

    int framebufferWidth = 0;
    int framebufferHeight = 0;

    glfwGetFramebufferSize(
        window,
        &framebufferWidth,
        &framebufferHeight
    );

    glViewport(
        0,
        0,
        framebufferWidth,
        framebufferHeight
    );

    ApplicationState state;

    // state.nx = 100;
    // state.ny = 10;
    // state.nz = 40;

    const std::filesystem::path dataDirectory{
    "assets/simulation_output"
    };

    constexpr int firstFileNumber = 0;
    constexpr int lastFileNumber = 17200;
    constexpr int fileStep = 400;

    int currentFileNumber = firstFileNumber;

    std::vector<float> velocityX;
    std::vector<float> velocityY;
    std::vector<float> velocityZ;
    std::vector<float> slice;

    try
    {
        const std::filesystem::path firstFile =
            makeVtiFilePath(
                dataDirectory,
                currentFileNumber
            );

        loadAsciiVtiVelocity(
            firstFile.string(),
            state.nx,
            state.ny,
            state.nz,
            velocityX,
            velocityY,
            velocityZ
        );
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Could not load initial VTI file:\n"
            << exception.what()
            << '\n';

        return EXIT_FAILURE;
    }

    state.orientation = SliceOrientation::XZ;

    state.currentSlice = state.ny / 2;

    state.maximumSlice =
        getMaximumSliceIndex(
            state.orientation,
            state.nx,
            state.ny,
            state.nz
        );

    int width = 0;
    int height = 0;

    getSliceDimensions(
        state.orientation,
        state.nx,
        state.ny,
        state.nz,
        width,
        height
    );

    extractVelocityMagnitudeXZSlice(
    velocityX,
    velocityY,
    velocityZ,
    slice,
    state.nx,
    state.ny,
    state.nz,
    state.currentSlice
);

    // state.orientation = SliceOrientation::XY;
    // state.currentSlice = state.nz / 2;
    // state.maximumSlice = state.nz - 1;

    // int width = state.nx;
    // int height = state.ny;

    // std::vector<float> slice;
    // const std::size_t cellCount =
    // static_cast<std::size_t>(state.nx) *
    // static_cast<std::size_t>(state.ny) *
    // static_cast<std::size_t>(state.nz);

    // std::vector<float> velocityX(cellCount);
    // std::vector<float> velocityY(cellCount);
    // std::vector<float> velocityZ(cellCount);
    
    

    
    // createDummyVelocityField(velocityX, velocityY, velocityZ, state.nx, state.ny, state.nz);
    //auto sphere = createDummySphere(state.nx, state.ny, state.nz);
    
    glfwSetWindowUserPointer(window, &state);
    
    // extractVelocityMagnitudeXYSlice(velocityX, velocityY, velocityZ, slice, state.nx, state.ny, state.nz, state.currentSlice);

   
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, slice.data());

    float vertices[] = {
        1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, 0.0f
    };

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    const char* vertexShaderSource = R"(
        #version 330 core

        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTex;

        out vec2 TexCoord;

        void main()
        {
            TexCoord = aTex; 
            gl_Position = vec4(aPos, 1.0);
        }
        )";

    const char* fragmentShaderSource = R"(
        #version 330 core

        uniform float maximumValue;
        uniform float minimumValue;
        uniform sampler2D texture1;
        in vec2 TexCoord;
        out vec4 FragColor;

        vec3 applyColorMap(float t)
        {
            if (t < 0.25)
    {
        return mix(
            vec3(0.0, 0.0, 0.5),
            vec3(0.0, 0.5, 1.0),
            t / 0.25
        );
    }
    else if (t < 0.5)
    {
        return mix(
            vec3(0.0, 0.5, 1.0),
            vec3(0.0, 1.0, 0.5),
            (t - 0.25) / 0.25
        );
    }
    else if (t < 0.75)
    {
        return mix(
            vec3(0.0, 1.0, 0.5),
            vec3(1.0, 1.0, 0.0),
            (t - 0.5) / 0.25
        );
    }
    else
    {
        return mix(
            vec3(1.0, 1.0, 0.0),
            vec3(1.0, 0.0, 0.0),
            (t - 0.75) / 0.25
        );
    }
        }

        void main()
        {
            float value = texture(texture1, TexCoord).r;
            float range = maximumValue - minimumValue;
            float normalizedValue = (value - minimumValue) / range;
            normalizedValue = clamp(normalizedValue, 0.0, 1);
            vec3 color = applyColorMap(normalizedValue);
            FragColor = vec4(color, 1.0);
        }
        )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);

    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniform1f(glGetUniformLocation(shaderProgram, "maximumValue"), 0.35f);
    glUniform1f(glGetUniformLocation(shaderProgram, "minimumValue"), 0.0f);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // float time = 0;
    // float timestep = 0.01f;


    bool playing = true;

    /*
    * 0.1 seconds per saved file means
    * approximately 10 VTI files per second.
    */
    const double filePlaybackInterval = 0.1;

    double lastFileLoadTime = glfwGetTime();

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const double currentTime =
        glfwGetTime();

        if (
            playing &&
            currentTime - lastFileLoadTime >=
                filePlaybackInterval
        )
        {
            int nextFileNumber =
                currentFileNumber + fileStep;

            if (nextFileNumber > lastFileNumber)
            {
                nextFileNumber =
                    firstFileNumber;
            }

            const std::filesystem::path nextFile =
                makeVtiFilePath(
                    dataDirectory,
                    nextFileNumber
                );

            if (!std::filesystem::exists(nextFile))
            {
                std::cerr
                    << "VTI file does not exist: "
                    << nextFile
                    << '\n';

                playing = false;
            }
            else
            {
                int frameNx = 0;
                int frameNy = 0;
                int frameNz = 0;

                try
                {
                    loadAsciiVtiVelocity(
                        nextFile.string(),
                        frameNx,
                        frameNy,
                        frameNz,
                        velocityX,
                        velocityY,
                        velocityZ
                    );

                    if (
                        frameNx != state.nx ||
                        frameNy != state.ny ||
                        frameNz != state.nz
                    )
                    {
                        throw std::runtime_error(
                            "Grid dimensions changed in: " +
                            nextFile.string()
                        );
                    }

                    currentFileNumber =
                        nextFileNumber;

                    /*
                    * The slice index did not change, but its
                    * underlying velocity data changed.
                    */
                    state.sliceChanged = true;

                    std::cout
                        << "Displaying "
                        << nextFile.filename().string()
                        << '\n';
                }
                catch (const std::exception& exception)
                {
                    std::cerr
                        << "Could not load frame:\n"
                        << exception.what()
                        << '\n';

                    playing = false;
                }
            }

            lastFileLoadTime =
                currentTime;
        }

        // time += timestep;
        // const float time = static_cast<float>(glfwGetTime());
        // createDummyVelocityField(velocityX, velocityY, velocityZ, state.nx, state.ny, state.nz, time);

        if (state.orientationChanged) {
            getSliceDimensions(state.orientation, state.nx, state.ny, state.nz, width, height);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
            state.orientationChanged = false;
        }

        if (state.orientation == SliceOrientation::XY) {
            extractVelocityMagnitudeXYSlice(velocityX, velocityY, velocityZ, slice, state.nx, state.ny, state.nz, state.currentSlice);
        } else if (state.orientation == SliceOrientation::YZ) {
            extractVelocityMagnitudeYZSlice(velocityX, velocityY, velocityZ, slice, state.nx, state.ny, state.nz, state.currentSlice);
        } else if (state.orientation == SliceOrientation::XZ) {
            extractVelocityMagnitudeXZSlice(velocityX, velocityY, velocityZ, slice, state.nx, state.ny, state.nz, state.currentSlice);
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, slice.data());

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}