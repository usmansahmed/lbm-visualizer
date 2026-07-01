#include "Application.hpp"

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

Application::Application()
{
    try
    {
        initializeWindow();
        initializeOpenGL();
        initializeResources();
        loadInitialFrame();
    }
    catch (...)
    {
        texture_.reset();
        renderer_.reset();
        shader_.reset();

        if (window_ != nullptr)
        {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }

        glfwTerminate();

        throw;
    }
}

Application::~Application()
{
    if (window_ != nullptr)
    {
        glfwMakeContextCurrent(window_);
    }

    texture_.reset();
    renderer_.reset();
    shader_.reset();

    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void Application::initializeWindow()
{
    if (glfwInit() != GLFW_TRUE)
    {
        throw std::runtime_error(
            "Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(1280, 720, "LBM Visualizer", nullptr, nullptr);

    if (window_ == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwMakeContextCurrent(window_);

    glfwSetWindowUserPointer(window_, this);

    glfwSetKeyCallback(window_, &keyCallback);
    glfwSetFramebufferSizeCallback(window_, &framebufferSizeCallback);
    glfwSwapInterval(1);
}

void Application::initializeOpenGL()
{
    const int gladVersion = gladLoadGL(glfwGetProcAddress);

    if (gladVersion == 0)
    {
        throw std::runtime_error("Failed to load OpenGL functions with GLAD.");
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;

    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    glViewport(0, 0, framebufferWidth, framebufferHeight);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void Application::initializeResources()
{
    shader_ = std::make_unique<Shader>("shaders/vert.glsl", "shaders/frag.glsl");

    renderer_ = std::make_unique<Renderer>();

    texture_ = std::make_unique<ScalarTexture>();

    shader_->use();

    shader_->setInt("texture1", 0);
}

void Application::loadInitialFrame()
{
    currentFileNumber_ = firstFileNumber_;

    const std::filesystem::path filePath = makeVtiFilePath(dataDirectory_, currentFileNumber_);

    frame_ = loadVtiFile(filePath);

    if (frame_.nx <= 0 || frame_.ny <= 0 || frame_.nz <= 0)
    {
        throw std::runtime_error("The initial frame has invalid dimensions.");
    }

    state_.orientation = SliceOrientation::XZ;

    state_.displayField = DisplayField::VelocityMagnitude;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, frame_.nx, frame_.ny, frame_.nz);

    state_.currentSlice = state_.maximumSlice / 2;

    state_.dataChanged = true;
    state_.sliceChanged = true;
    state_.orientationChanged = true;
    state_.fieldChanged = true;

    lastPlaybackTime_ = glfwGetTime();

    std::cout
        << "Loaded initial frame: "
        << filePath.filename().string()
        << '\n'
        << "Grid dimensions: "
        << frame_.nx << " x "
        << frame_.ny << " x "
        << frame_.nz << '\n';
}

void Application::updatePlayback()
{
    if (!state_.playing)
        return;

    const double currentTime = glfwGetTime();

    if (currentTime - lastPlaybackTime_ < playbackInterval_)
    {
        return;
    }

    int nextFileNumber = currentFileNumber_ + fileStep_;

    if (nextFileNumber > lastFileNumber_)
    {
        nextFileNumber = firstFileNumber_;
    }

    const std::filesystem::path nextFilePath = makeVtiFilePath(dataDirectory_, nextFileNumber);
    if (!std::filesystem::exists(nextFilePath))
    {
        std::cerr << "File does not exist: " << nextFilePath << '\n';
        state_.playing = false;
        return;
    }

    SimulationFrame nextFrame = loadVtiFile(nextFilePath);

    if (nextFrame.nx != frame_.nx || nextFrame.ny != frame_.ny || nextFrame.nz != frame_.nz)
    {
        throw std::runtime_error("Grid dimensions changed in file: " + nextFilePath.string());
    }

    frame_ = std::move(nextFrame);

    currentFileNumber_ = nextFileNumber;

    state_.dataChanged = true;

    lastPlaybackTime_ = currentTime;

    std::cout << "Loaded frame: " << nextFilePath.filename().string() << '\n';
}

void Application::updateVisualization()
{
    const bool visualizationChanged = state_.dataChanged || state_.sliceChanged ||
                                      state_.orientationChanged || state_.fieldChanged;

    if (!visualizationChanged)
        return;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, frame_.nx, frame_.ny, frame_.nz);

    state_.currentSlice = std::clamp(state_.currentSlice, 0, state_.maximumSlice);

    getSliceDimensions(state_.orientation, frame_.nx, frame_.ny, frame_.nz, textureWidth_, textureHeight_);

    extractSlice(frame_, slice_, state_.orientation, state_.displayField, state_.currentSlice);

    const std::size_t expectedSliceSize = static_cast<std::size_t>(textureWidth_) * static_cast<std::size_t>(textureHeight_);

    if (slice_.size() != expectedSliceSize)
    {
        throw std::runtime_error("Extracted slice has an unexpected size.");
    }

    texture_->update(textureWidth_, textureHeight_, slice_);

    state_.dataChanged = false;
    state_.sliceChanged = false;
    state_.orientationChanged = false;
    state_.fieldChanged = false;
}

void Application::render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    shader_->use();

    shader_->setInt("texture1", 0);

    shader_->setFloat("minimumValue", state_.minimumValue);

    shader_->setFloat("maximumValue", state_.maximumValue);

    texture_->bind(0);

    renderer_->drawQuad();
}

void Application::run()
{
    while (glfwWindowShouldClose(window_) == GLFW_FALSE)
    {
        glfwPollEvents();

        updatePlayback();
        updateVisualization();
        render();

        glfwSwapBuffers(window_);
    }
}

void Application::keyCallback(
    GLFWwindow *window,
    int key,
    int scancode,
    int action,
    int mods)
{
    auto *application = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (application == nullptr)
        return;

    application->handleKey(key, scancode, action, mods);
}

void Application::framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    auto *application = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (application == nullptr)
        return;

    application->handleFrameBufferSize(width, height);
}

void Application::handleKey(
    int key,
    int scancode,
    int action,
    int mods)
{
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS && action != GLFW_REPEAT)
    {
        return;
    }

    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        return;
    }

    if (key == GLFW_KEY_LEFT)
    {
        if (state_.currentSlice > 0)
        {
            --state_.currentSlice;
            state_.sliceChanged = true;
        }

        return;
    }

    if (key == GLFW_KEY_RIGHT)
    {
        if (state_.currentSlice < state_.maximumSlice)
        {
            ++state_.currentSlice;
            state_.sliceChanged = true;
        }

        return;
    }

    if (action != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_SPACE)
    {
        state_.playing = !state_.playing;
        return;
    }

    if (key == GLFW_KEY_X)
    {
        setOrientation(SliceOrientation::YZ);
        return;
    }

    if (key == GLFW_KEY_Y)
    {
        setOrientation(SliceOrientation::XZ);
        return;
    }

    if (key == GLFW_KEY_Z)
    {
        setOrientation(SliceOrientation::XY);
        return;
    }
}

void Application::handleFrameBufferSize(int width, int height)
{
    (void)window_;
    glViewport(0, 0, width, height);
}

void Application::setOrientation(SliceOrientation orientation)
{
    if (state_.orientation == orientation)
        return;

    state_.orientation = orientation;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, frame_.nx, frame_.ny, frame_.nz);

    state_.currentSlice = state_.maximumSlice / 2;

    state_.orientationChanged = true;
    state_.sliceChanged = true;
}