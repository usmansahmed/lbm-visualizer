#include "app/Application.hpp"
#include "geometry/VelocityArrowBuilder.hpp"
#include "geometry/ProbeOverlayBuilder.hpp"
#include "geometry/ObstacleFactory.hpp"

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
    // Convert CUDA errors into exceptions with a useful message.
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
        // Initialize the window, OpenGL resources, and the first simulation.
        initializeWindow();
        initializeOpenGL();
        initializeResources();
        restartSimulation();
    }
    catch (...)
    {
        // Clean up partially created resources if initialization fails.
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
        solver_.reset();

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
    // Make the OpenGL context current before destroying OpenGL/CUDA-interop resources.
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
    solver_.reset();

    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void Application::initializeWindow()
{
    // Create the GLFW window and OpenGL context.
    if (glfwInit() != GLFW_TRUE)
    {
        throw std::runtime_error(
            "Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window_ = glfwCreateWindow(1280, 720, "LBM Visualizer", nullptr, nullptr);

    if (window_ == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwMakeContextCurrent(window_);

    // Store the Application pointer so static callbacks can call member functions.
    glfwSetWindowUserPointer(window_, this);

    glfwSetKeyCallback(window_, &keyCallback);
    // glfwSetFramebufferSizeCallback(window_, &framebufferSizeCallback);
    glfwSwapInterval(1);
}

void Application::initializeOpenGL()
{
    // Load OpenGL functions through GLAD after the context has been created.
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
    // Create shaders, renderers, textures, PBOs, and the ImGui interface.
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

    // These texture units are used by the main fragment shader.
    shader_->setInt("fieldTexture", 0);
    shader_->setInt("obstacleTexture", 1);
}

void Application::updateSimulation()
{
    // Only advance the solver while playback is enabled.
    if (!state_.playing)
        return;

    const double currentTime = glfwGetTime();

    if (currentTime - lastPlaybackTime_ < playbackInterval_)
        return;

    // Run one LBM time step on the GPU.
    solver_->step();

    checkCuda(cudaPeekAtLastError(), "solver step kernel launch");
    checkCuda(cudaDeviceSynchronize(), "solver step kernel execution");

    state_.dataChanged = true;
    lastPlaybackTime_ = currentTime;
}

void Application::updateVisualization()
{
    // Refresh the texture only when the selected data or slice has changed.
    const bool visualizationChanged = state_.dataChanged || state_.sliceChanged ||
                                      state_.orientationChanged || state_.fieldChanged;

    const bool arrowsChanged = state_.dataChanged || state_.sliceChanged ||
                               state_.orientationChanged || state_.arrowsChanged;

    if (visualizationChanged)
    {
        // Clamp the slice index against the current grid and orientation.
        state_.maximumSlice = getMaximumSliceIndex(state_.orientation, state_.nx, state_.ny, state_.nz);

        state_.currentSlice = std::clamp(state_.currentSlice, 0, state_.maximumSlice);

        getSliceDimensions(state_.orientation, state_.nx, state_.ny, state_.nz, textureWidth_, textureHeight_);

        // Map the OpenGL PBO so CUDA can write the selected slice directly into it.
        float *pboDevicePointer = pboBuffer_->mapPBOToCuda(textureWidth_, textureHeight_);

        solver_->prepareVisualizationSlice(pboDevicePointer, state_.displayField, state_.orientation, state_.currentSlice, textureWidth_, textureHeight_, state_.showVelocityArrows, state_.automaticColorScaling);

        checkCuda(cudaGetLastError(), "prepareData kernel launch");
        checkCuda(cudaDeviceSynchronize(), "prepareData kernel execution");

        // Unmap before OpenGL uses the PBO as a texture upload source.
        pboBuffer_->unmapPBOFromCuda();

        // Obstacles are uploaded as a separate texture so the shader can draw them clearly.
        extractSlice(state_, obstacleSlice_, state_.orientation, DisplayField::Obstacle, state_.currentSlice);

        if (state_.automaticColorScaling)
        {
            const auto [min, max] = solver_->getValueRange();
            state_.automaticMinimumValue = min;
            state_.automaticMaximumValue = max;
        }

        state_.finalMinimum = state_.automaticColorScaling ? state_.automaticMinimumValue : state_.minimumValue;
        state_.finalMaximum = state_.automaticColorScaling ? state_.automaticMaximumValue : state_.maximumValue;

        // Copy the CUDA-written PBO into the OpenGL texture used for rendering.
        texture_->updateFromPBO(textureWidth_, textureHeight_, pboBuffer_->getID());
        obstacleTexture_->update(textureWidth_, textureHeight_, obstacleSlice_);
    }

    if (arrowsChanged)
    {
        // Rebuild arrow geometry for the current slice when arrows are enabled.
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

    // Clear change flags after all dependent data has been refreshed.
    state_.dataChanged = false;
    state_.sliceChanged = false;
    state_.orientationChanged = false;
    state_.fieldChanged = false;
    state_.arrowsChanged = false;
}

void Application::renderSimulation()
{
    // Draw the scalar field texture first.
    shader_->use();

    shader_->setInt("fieldTexture", 0);

    shader_->setInt("obstacleTexture", 1);

    shader_->setFloat("minimumValue", state_.finalMinimum);

    shader_->setFloat("maximumValue", state_.finalMaximum);

    texture_->bind(0);

    obstacleTexture_->bind(1);

    renderer_->drawQuad();

    // Draw overlays on top of the scalar texture.
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
    // Main application loop.
    while (glfwWindowShouldClose(window_) == GLFW_FALSE)
    {
        glfwPollEvents();

        if (state_.restartRequested)
        {
            // Recreate CUDA buffers and obstacle data after setup changes.
            restartSimulation();
            state_.restartRequested = false;
        }

        updateSimulation();
        updateVisualization();

        // Start each frame from a clean full-window OpenGL state.
        int framebufferWidth = 0;
        int framebufferHeight = 0;

        glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        userInterface_->beginFrame();

        // Build ImGui first so the Simulation View rectangle is known.
        drawMainDockSpace();
        drawSimulationViewWindow();
        drawControlsWindow();

        const bool validTextureViewport = updateTextureViewport();

        // Render ImGui using the full framebuffer viewport.
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        userInterface_->render();

        if (validTextureViewport)
        {
            // Draw the simulation into the Simulation View panel after ImGui.
            handleProbeInput();
            updateProbeOverlay();
            applyTextureViewport();
            renderSimulation();
            glDisable(GL_SCISSOR_TEST);
        }
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
    // Recover the Application instance stored in the GLFW window.
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

    // Keyboard shortcuts for closing, playback, and slice navigation.

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
    // Changing orientation also resets the current slice to the middle.
    if (state_.orientation == orientation)
        return;

    state_.orientation = orientation;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, state_.nx, state_.ny, state_.nz);

    state_.currentSlice = state_.maximumSlice / 2;

    state_.orientationChanged = true;
    state_.sliceChanged = true;
}

bool Application::updateTextureViewport()
{
    // Calculate the OpenGL viewport that fits the texture inside the ImGui panel.
    if (textureWidth_ <= 0 || textureHeight_ <= 0 || simulationViewSize_.x <= 1.0f || simulationViewSize_.y <= 1.0f)
    {
        return false;
    }

    int windowWidth = 0;
    int windowHeight = 0;

    int framebufferWidth = 0;
    int framebufferHeight = 0;

    glfwGetWindowSize(window_, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    if (windowWidth <= 0 || windowHeight <= 0 || framebufferWidth <= 0 || framebufferHeight <= 0)
    {
        return false;
    }

    const float scaleX = static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);

    const float scaleY = static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight);

    const ImGuiViewport *imguiViewport = ImGui::GetMainViewport();

    const float localPanelX = simulationViewPosition_.x - imguiViewport->Pos.x;
    const float localPanelY = simulationViewPosition_.y - imguiViewport->Pos.y;

    const int panelX = static_cast<int>(localPanelX * scaleX);

    // ImGui uses top-left coordinates, while OpenGL viewports start at the bottom-left.
    const int panelY = static_cast<int>((static_cast<float>(windowHeight) - localPanelY - simulationViewSize_.y) * scaleY);

    const int panelWidth = static_cast<int>(simulationViewSize_.x * scaleX);
    const int panelHeight = static_cast<int>(simulationViewSize_.y * scaleY);

    if (panelWidth <= 0 || panelHeight <= 0)
    {
        return false;
    }

    simulationPanelViewport_.x = panelX;
    simulationPanelViewport_.y = panelY;
    simulationPanelViewport_.width = panelWidth;
    simulationPanelViewport_.height = panelHeight;

    const float panelAspect = static_cast<float>(panelWidth) / static_cast<float>(panelHeight);
    const float textureAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);

    int viewportWidth = panelWidth;
    int viewportHeight = panelHeight;

    int viewportX = panelX;
    int viewportY = panelY;

    if (panelAspect > textureAspect)
    {
        // Panel is wider than the texture, so add horizontal letterboxing.
        viewportHeight = panelHeight;
        viewportWidth = static_cast<int>(static_cast<float>(panelHeight) * textureAspect);

        viewportX = panelX + (panelWidth - viewportWidth) / 2;
    }
    else
    {
        // Panel is taller than the texture, so add vertical letterboxing.
        viewportWidth = panelWidth;
        viewportHeight = static_cast<int>(static_cast<float>(panelWidth) / textureAspect);
        viewportY = panelY + (panelHeight - viewportHeight) / 2;
    }

    textureViewport_.x = viewportX;
    textureViewport_.y = viewportY;
    textureViewport_.width = viewportWidth;
    textureViewport_.height = viewportHeight;

    return true;
}

void Application::applyTextureViewport()
{
    // Clip rendering to the Simulation View panel and draw the texture inside it.
    glEnable(GL_SCISSOR_TEST);

    glScissor(simulationPanelViewport_.x, simulationPanelViewport_.y, simulationPanelViewport_.width, simulationPanelViewport_.height);

    glViewport(textureViewport_.x, textureViewport_.y, textureViewport_.width, textureViewport_.height);
}

void Application::updateProbeValues()
{
    // Refresh the displayed probe values from the selected 3D cell.
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
    // Convert a mouse click in the texture viewport into a 3D cell coordinate.
    const bool leftMousePressed = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    const bool justPressed = leftMousePressed && !previousLeftMousePressed_;

    previousLeftMousePressed_ = leftMousePressed;

    if (!justPressed)
        return;

    if (!simulationViewHovered_)
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

    // Map the 2D slice coordinate back to a 3D grid coordinate.
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
    // Rebuild the small overlay marker around the selected probe cell.
    int planeWidth = 0;
    int planeHeight = 0;

    getSliceDimensions(state_.orientation, state_.nx, state_.ny, state_.nz, planeWidth, planeHeight);
    probeOverlayVertices_ = buildProbeOverlayVertices(probe_, state_.orientation, state_.currentSlice, planeWidth, planeHeight);
    probeOverlayRenderer_->update(probeOverlayVertices_);
}

void Application::restartSimulation()
{
    // Stop playback while the solver and GPU resources are recreated.
    state_.playing = false;

    activeConfig_ = pendingConfig_;

    auto obstacle = createObstacleMask(activeConfig_);

    // Create a new solver using the selected configuration and obstacle mask.
    solver_ = std::make_unique<LbmSolver>(activeConfig_, obstacle);

    state_.nx = activeConfig_.nx;
    state_.ny = activeConfig_.ny;
    state_.nz = activeConfig_.nz;
    state_.obstacle = obstacle;

    state_.orientation = SliceOrientation::XY;
    state_.displayField = DisplayField::VelocityMagnitude;

    state_.maximumSlice = getMaximumSliceIndex(state_.orientation, state_.nx, state_.ny, state_.nz);
    state_.currentSlice = state_.maximumSlice / 2;

    lastPlaybackTime_ = glfwGetTime();

    probe_.valid = false;

    // Force the first visualization update after restart.
    state_.dataChanged = true;
    state_.sliceChanged = true;
    state_.orientationChanged = true;
    state_.fieldChanged = true;
    state_.arrowsChanged = true;
}

void Application::drawMainDockSpace()
{
    // Full-window invisible host for ImGui docking.
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport *viewport =
        ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin(
        "Main DockSpace Window",
        nullptr,
        windowFlags);

    ImGui::PopStyleVar(2);

    ImGuiID dockspaceId =
        ImGui::GetID("MainDockSpace");

    ImGui::DockSpace(
        dockspaceId,
        ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_None);

    ImGui::End();
}

void Application::drawControlsWindow()
{
    // Dockable window containing all UI controls.
    ImGui::Begin("Controls");
    userInterface_->drawControls(state_, pendingConfig_, probe_);
    ImGui::End();
}

void Application::drawSimulationViewWindow()
{
    // Dockable panel used as the target area for OpenGL rendering.
    ImGui::Begin("Simulation View");

    simulationViewPosition_ = ImGui::GetCursorScreenPos();

    simulationViewSize_ = ImGui::GetContentRegionAvail();

    if (simulationViewSize_.x < 1.0f)
    {
        simulationViewSize_.x = 1.0f;
    }

    if (simulationViewSize_.y < 1.0f)
    {
        simulationViewSize_.y = 1.0f;
    }

    // The invisible button reserves space and lets ImGui report hover/click state.
    ImGui::InvisibleButton("Simulation Canvas", simulationViewSize_, ImGuiButtonFlags_MouseButtonLeft);

    simulationViewHovered_ = ImGui::IsItemHovered();

    ImGui::End();
}