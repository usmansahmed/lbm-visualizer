#pragma once

#include "Renderer.hpp"
#include "ScalarTexture.hpp"
#include "Shader.hpp"
#include "SimulationFrame.hpp"
#include "VisualizationState.hpp"
#include "UserInterface.hpp"
#include "VelocityArrowRenderer.hpp"
#include "ProbeResult.hpp"
#include "LbmSolver.hpp"
#include "PBOBuffer.hpp"
#include "SimulationConfig.hpp"

#include <GLFW/glfw3.h>

#include <filesystem>
#include <vector>
#include <memory>

class Application
{
public:
    Application();
    ~Application();

    void run();

private:
    // static void framebufferSizeCallback(GLFWwindow *window, int width, int height);
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    void handleKey(int key, int scancode, int action, int mods);
    // void handleFrameBufferSize(int width, int height);

    void initializeWindow();
    void initializeOpenGL();
    void initializeResources();

    // void initializeSimulation();
    void updateSimulation();
    void updateVisualization();
    void render();
    void setOrientation(SliceOrientation orientation);
    void setTextureViewport();
    void updateProbeValues();
    void handleProbeInput();
    void updateProbeOverlay();
    void restartSimulation();

    GLFWwindow *window_ = nullptr;

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ScalarTexture> texture_;
    std::unique_ptr<ScalarTexture> obstacleTexture_;
    std::unique_ptr<UserInterface> userInterface_;
    std::unique_ptr<LbmSolver> solver_;
    std::unique_ptr<PBOBuffer> pboBuffer_;

    std::unique_ptr<Shader> arrowShader_;
    std::unique_ptr<VelocityArrowRenderer> arrowRenderer_;
    std::vector<float> arrowVertices_;

    VisualizationState state_;
    std::vector<float> obstacleSlice_;

    int textureWidth_;
    int textureHeight_;

    double lastFrameTime_ = 0.0;
    double playbackInterval_ = 0.1;
    double lastPlaybackTime_ = 0.0;

    ProbeResult probe_;
    ViewportRectangle textureViewport_;
    bool previousLeftMousePressed_ = false;

    std::unique_ptr<VelocityArrowRenderer> probeOverlayRenderer_;
    std::unique_ptr<Shader> probeOverlayShader_;
    std::vector<float> probeOverlayVertices_;

    SimulationConfig pendingConfig_;
    SimulationConfig activeConfig_;
};