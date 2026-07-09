#include "Application.hpp"
#include "VelocityArrowBuilder.hpp"
#include "ProbeOverlayBuilder.hpp"

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

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

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

Application::Application()
{
    try
    {
        initializeWindow();
        initializeOpenGL();
        initializeResources();
        initializeSimulation();
    }
    catch (...)
    {
        probeOverlayRenderer_.reset();
        probeOverlayShader_.reset();
        pboBuffer_.reset();
        userInterface_.reset();
        texture_.reset();
        obstacleTexture_.reset();
        renderer_.reset();
        arrowRenderer_.reset();
        arrowShader_.reset();
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

    probeOverlayRenderer_.reset();
    probeOverlayShader_.reset();
    pboBuffer_.reset();
    userInterface_.reset();
    texture_.reset();
    obstacleTexture_.reset();
    renderer_.reset();
    arrowRenderer_.reset();
    arrowShader_.reset();
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
    // glfwSetFramebufferSizeCallback(window_, &framebufferSizeCallback);
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
    shader_ = std::make_unique<Shader>("shaders/quad.vert", "shaders/quad.frag");
    arrowShader_ = std::make_unique<Shader>("shaders/velocity_arrows.vert", "shaders/velocity_arrows.frag");
    probeOverlayShader_ = std::make_unique<Shader>("shaders/probe_overlay.vert", "shaders/probe_overlay.frag");

    renderer_ = std::make_unique<Renderer>();
    arrowRenderer_ = std::make_unique<VelocityArrowRenderer>();
    probeOverlayRenderer_ = std::make_unique<VelocityArrowRenderer>();

    texture_ = std::make_unique<ScalarTexture>();
    pboBuffer_ = std::make_unique<PBOBuffer>();

    obstacleTexture_ = std::make_unique<ScalarTexture>();

    userInterface_ = std::make_unique<UserInterface>(window_);

    shader_->use();

    shader_->setInt("fieldTexture", 0);
    shader_->setInt("obstacleTexture", 1);
}

void Application::initializeSimulation()
{
    solver_ = std::make_unique<LbmSolver>();
    state_.nx = 256;
    state_.ny = 64;
    state_.nz = 16;
    state_.obstacle = solver_->getObstacle();

    state_.orientation = SliceOrientation::XY;
    state_.displayField = DisplayField::VelocityMagnitude;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, state_.nx, state_.ny, state_.nz);

    state_.currentSlice = state_.maximumSlice / 2;

    state_.dataChanged = true;
    state_.sliceChanged = true;
    state_.orientationChanged = true;
    state_.fieldChanged = true;

    lastPlaybackTime_ = glfwGetTime();
}

void Application::updateSimulation()
{
    if (!state_.playing)
        return;

    const double currentTime = glfwGetTime();

    if (currentTime - lastPlaybackTime_ < playbackInterval_)
        return;

    solver_->step();

    checkCuda(cudaPeekAtLastError(), "solver step kernel launch");
    checkCuda(cudaDeviceSynchronize(), "solver step kernel execution");

    state_.dataChanged = true;
    lastPlaybackTime_ = currentTime;
}

void Application::updateVisualization()
{
    const bool visualizationChanged = state_.dataChanged || state_.sliceChanged ||
                                      state_.orientationChanged || state_.fieldChanged;

    const bool arrowsChanged = state_.dataChanged || state_.sliceChanged ||
                               state_.orientationChanged || state_.arrowsChanged;

    if (visualizationChanged)
    {
        state_.maximumSlice = getMaximumSliceIndex(state_.orientation, state_.nx, state_.ny, state_.nz);

        state_.currentSlice = std::clamp(state_.currentSlice, 0, state_.maximumSlice);

        getSliceDimensions(state_.orientation, state_.nx, state_.ny, state_.nz, textureWidth_, textureHeight_);

        float *pboDevicePointer = pboBuffer_->mapPBOToCuda(textureWidth_, textureHeight_);

        solver_->prepareData(pboDevicePointer, state_.displayField, state_.orientation, state_.currentSlice, textureWidth_, textureHeight_, state_.showVelocityArrows);

        checkCuda(cudaGetLastError(), "prepareData kernel launch");
        checkCuda(cudaDeviceSynchronize(), "prepareData kernel execution");
        pboBuffer_->unmapPBOFromCuda();

        // extractSlice(frame_, slice_, state_.orientation, state_.displayField, state_.currentSlice);
        extractSlice(state_, obstacleSlice_, state_.orientation, DisplayField::Obstacle, state_.currentSlice);

        // if (state_.displayField == DisplayField::VelocityMagnitude)
        // {
        //     state_.minimumValue = 0.0f;
        //     state_.maximumValue = 0.35f;
        // }
        // else
        // {
        //     state_.minimumValue = -0.35f;
        //     state_.maximumValue = 0.35f;
        // }

        // const std::size_t expectedSliceSize = static_cast<std::size_t>(textureWidth_) * static_cast<std::size_t>(textureHeight_);

        // if (slice_.size() != expectedSliceSize)
        // {
        //     throw std::runtime_error("Extracted slice has an unexpected size.");
        // }

        if (state_.automaticColorScaling)
        {
            const auto [min, max] = solver_->getValueRange();
            state_.automaticMinimumValue = min;
            state_.automaticMaximumValue = max;
        }

        state_.finalMinimum = state_.automaticColorScaling ? state_.automaticMinimumValue : state_.minimumValue;
        state_.finalMaximum = state_.automaticColorScaling ? state_.automaticMaximumValue : state_.maximumValue;

        texture_->updateFromPBO(textureWidth_, textureHeight_, pboBuffer_->getID());
        obstacleTexture_->update(textureWidth_, textureHeight_, obstacleSlice_);
    }

    if (arrowsChanged)
    {
        if (state_.showVelocityArrows)
        {
            const VelocitySlice2D slice = solver_->getVelocitySlice2D();
            arrowVertices_ = buildVelocityArrowVertices(state_, slice, state_.orientation, state_.currentSlice,
                                                        state_.arrowStride, state_.arrowLengthScale);
        }
        else
        {
            arrowVertices_.clear();
        }

        arrowRenderer_->update(arrowVertices_);
    }
    if (state_.dataChanged && probe_.valid)
    {
        updateProbeValues();
    }

    state_.dataChanged = false;
    state_.sliceChanged = false;
    state_.orientationChanged = false;
    state_.fieldChanged = false;
    state_.arrowsChanged = false;
}

void Application::render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    shader_->use();

    shader_->setInt("fieldTexture", 0);

    shader_->setInt("obstacleTexture", 1);

    shader_->setFloat("minimumValue", state_.finalMinimum);

    shader_->setFloat("maximumValue", state_.finalMaximum);

    texture_->bind(0);

    obstacleTexture_->bind(1);

    renderer_->drawQuad();

    if (state_.showVelocityArrows)
    {
        arrowShader_->use();
        arrowRenderer_->draw();
    }

    if (!probeOverlayVertices_.empty())
    {
        probeOverlayShader_->use();
        probeOverlayRenderer_->draw();
    }
}

void Application::run()
{
    while (glfwWindowShouldClose(window_) == GLFW_FALSE)
    {

        glfwPollEvents();
        userInterface_->beginFrame();
        userInterface_->drawControls(state_, probe_);

        updateSimulation();
        updateVisualization();

        setTextureViewport();
        handleProbeInput();
        updateProbeOverlay();
        render();

        userInterface_->render();

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

// void Application::framebufferSizeCallback(GLFWwindow *window, int width, int height)
// {
//     auto *application = static_cast<Application *>(glfwGetWindowUserPointer(window));

//     if (application == nullptr)
//         return;

//     application->handleFrameBufferSize(width, height);
// }

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

// void Application::handleFrameBufferSize(int width, int height)
// {
//     (void)window_;
//     glViewport(0, 0, width, height);
// }

void Application::setOrientation(SliceOrientation orientation)
{
    if (state_.orientation == orientation)
        return;

    state_.orientation = orientation;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, state_.nx, state_.ny, state_.nz);

    state_.currentSlice = state_.maximumSlice / 2;

    state_.orientationChanged = true;
    state_.sliceChanged = true;
}

void Application::setTextureViewport()
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;

    glfwGetFramebufferSize(
        window_,
        &framebufferWidth,
        &framebufferHeight);
    glViewport(
        0,
        0,
        framebufferWidth,
        framebufferHeight);

    const float windowAspect = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    const float textureAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);

    int viewportWidth = framebufferWidth;
    int viewportHeight = framebufferHeight;
    int viewportX = 0;
    int viewportY = 0;

    if (windowAspect > textureAspect)
    {
        viewportHeight = framebufferHeight;
        viewportWidth = static_cast<int>(framebufferHeight * textureAspect);
        viewportX = (framebufferWidth - viewportWidth) / 2;
    }
    else
    {
        viewportWidth = framebufferWidth;
        viewportHeight = static_cast<int>(framebufferWidth / textureAspect);
        viewportY = (framebufferHeight - viewportHeight) / 2;
    }

    textureViewport_.x = viewportX;
    textureViewport_.y = viewportY;
    textureViewport_.width = viewportWidth;
    textureViewport_.height = viewportHeight;

    glViewport(textureViewport_.x, textureViewport_.y, textureViewport_.width, textureViewport_.height);
}

void Application::updateProbeValues()
{
    if (!probe_.valid)
        return;

    if (probe_.x < 0 || probe_.x >= state_.nx ||
        probe_.y < 0 || probe_.y >= state_.ny ||
        probe_.z < 0 || probe_.z >= state_.nz)
    {
        probe_.valid = false;
        return;
    }

    const std::size_t index = index3D(probe_.x, probe_.y, probe_.z, state_.nx, state_.ny);

    solver_->readProbeCell(probe_, index);
}

void Application::handleProbeInput()
{
    const bool leftMousePressed = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    const bool justPressed = leftMousePressed && !previousLeftMousePressed_;

    previousLeftMousePressed_ = leftMousePressed;

    if (!justPressed)
        return;

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (textureViewport_.width <= 0 ||
        textureViewport_.height <= 0 ||
        textureWidth_ <= 0 ||
        textureHeight_ <= 0)
    {
        return;
    }

    double cursorX = 0.0;
    double cursorY = 0.0;

    glfwGetCursorPos(window_, &cursorX, &cursorY);

    int windowWidth = 0;
    int windowHeight = 0;

    glfwGetWindowSize(window_, &windowWidth, &windowHeight);

    int framebufferWidth = 0;
    int framebufferHeight = 0;

    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    if (windowWidth <= 0 || windowHeight <= 0)
    {
        return;
    }

    const double scaleX = static_cast<double>(framebufferWidth) / static_cast<double>(windowWidth);

    const double scaleY = static_cast<double>(framebufferHeight) / static_cast<double>(windowHeight);

    const double framebufferX = cursorX * scaleX;

    const double framebufferY = (static_cast<double>(windowHeight) - cursorY) * scaleY;

    const double localX = framebufferX - static_cast<double>(textureViewport_.x);

    const double localY = framebufferY - static_cast<double>(textureViewport_.y);

    if (localX < 0.0 || localY < 0.0 ||
        localX >= textureViewport_.width ||
        localY >= textureViewport_.height)
    {
        return;
    }

    const double u = localX / static_cast<double>(textureViewport_.width);
    const double v = localY / static_cast<double>(textureViewport_.height);

    int planeX = static_cast<int>(u * textureWidth_);
    int planeY = static_cast<int>(v * textureHeight_);

    planeX = std::clamp(planeX, 0, textureWidth_ - 1);
    planeY = std::clamp(planeY, 0, textureHeight_ - 1);

    switch (state_.orientation)
    {
    case SliceOrientation::XY:
    {
        probe_.x = planeX;
        probe_.y = planeY;
        probe_.z = state_.currentSlice;
        break;
    }

    case SliceOrientation::XZ:
    {
        probe_.x = planeX;
        probe_.y = state_.currentSlice;
        probe_.z = planeY;
        break;
    }

    case SliceOrientation::YZ:
    {
        probe_.x = state_.currentSlice;
        probe_.y = planeX;
        probe_.z = planeY;
        break;
    }
    }

    probe_.valid = true;

    updateProbeValues();
}

void Application::updateProbeOverlay()
{
    int planeWidth = 0;
    int planeHeight = 0;

    getSliceDimensions(state_.orientation, state_.nx, state_.ny, state_.nz, planeWidth, planeHeight);
    probeOverlayVertices_ = buildProbeOverlayVertices(probe_, state_.orientation, state_.currentSlice, planeWidth, planeHeight);
    probeOverlayRenderer_->update(probeOverlayVertices_);
}