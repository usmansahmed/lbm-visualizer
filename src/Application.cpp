#include "Application.hpp"
#include "VelocityArrowBuilder.hpp"

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
        obstacleTexture_.reset();
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

    renderer_ = std::make_unique<Renderer>();
    arrowRenderer_ = std::make_unique<VelocityArrowRenderer>();

    texture_ = std::make_unique<ScalarTexture>();

    obstacleTexture_ = std::make_unique<ScalarTexture>();

    userInterface_ = std::make_unique<UserInterface>(window_);

    shader_->use();

    shader_->setInt("fieldTexture", 0);
    shader_->setInt("obstacleTexture", 1);
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
}

void Application::updateVisualization()
{
    const bool visualizationChanged = state_.dataChanged || state_.sliceChanged ||
                                      state_.orientationChanged || state_.fieldChanged;

    const bool arrowsChanged = state_.dataChanged || state_.sliceChanged ||
                               state_.orientationChanged || state_.arrowsChanged;

    if (visualizationChanged)
    {

        state_.maximumSlice = getMaximumSliceIndex(state_.orientation, frame_.nx, frame_.ny, frame_.nz);

        state_.currentSlice = std::clamp(state_.currentSlice, 0, state_.maximumSlice);

        getSliceDimensions(state_.orientation, frame_.nx, frame_.ny, frame_.nz, textureWidth_, textureHeight_);

        extractSlice(frame_, slice_, state_.orientation, state_.displayField, state_.currentSlice);
        extractSlice(frame_, obstacleSlice_, state_.orientation, DisplayField::Obstacle, state_.currentSlice);

        if (state_.displayField == DisplayField::VelocityMagnitude)
        {
            state_.minimumValue = 0.0f;
            state_.maximumValue = 0.35f;
        }
        else
        {
            state_.minimumValue = -0.35f;
            state_.maximumValue = 0.35f;
        }

        const std::size_t expectedSliceSize = static_cast<std::size_t>(textureWidth_) * static_cast<std::size_t>(textureHeight_);

        if (slice_.size() != expectedSliceSize)
        {
            throw std::runtime_error("Extracted slice has an unexpected size.");
        }

        if (state_.automaticColorScaling && !slice_.empty())
        {
            const auto [minimumIterator, maximumIterator] = std::minmax_element(slice_.begin(), slice_.end());

            state_.automaticMinimumValue = *minimumIterator;
            state_.automaticMaximumValue = *maximumIterator;
        }

        state_.finalMinimum = state_.automaticColorScaling ? state_.automaticMinimumValue : state_.minimumValue;
        state_.finalMaximum = state_.automaticColorScaling ? state_.automaticMaximumValue : state_.maximumValue;

        texture_->update(textureWidth_, textureHeight_, slice_);
        obstacleTexture_->update(textureWidth_, textureHeight_, obstacleSlice_);
    }

    if (arrowsChanged)
    {
        if (state_.showVelocityArrows)
        {
            arrowVertices_ = buildVelocityArrowVertices(frame_, state_.orientation, state_.currentSlice,
                                                        state_.arrowStride, state_.arrowLengthScale);
        }
        else
        {
            arrowVertices_.clear();
        }

        arrowRenderer_->update(arrowVertices_);
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
}

void Application::run()
{
    while (glfwWindowShouldClose(window_) == GLFW_FALSE)
    {
        glfwPollEvents();

        userInterface_->beginFrame();
        userInterface_->drawControls(state_, frame_);

        updatePlayback();
        updateVisualization();
        setTextureViewport();
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

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, frame_.nx, frame_.ny, frame_.nz);

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

    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
}