#pragma once

#include "rendering/Renderer.hpp"
#include "rendering/ScalarTexture.hpp"
#include "rendering/Shader.hpp"
#include "app/VisualizationState.hpp"
#include "ui/UserInterface.hpp"
#include "rendering/VelocityArrowRenderer.hpp"
#include "app/ProbeResult.hpp"
#include "lbm/LbmSolver.hpp"
#include "rendering/PBOBuffer.hpp"
#include "app/SimulationConfig.hpp"

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

    void initializeWindow();
    void initializeOpenGL();
    void initializeResources();

    void updateSimulation();
    void updateVisualization();
    void renderSimulation();
    void setOrientation(SliceOrientation orientation);
    bool updateTextureViewport();
    void applyTextureViewport();
    void updateProbeValues();
    void handleProbeInput();
    void updateProbeOverlay();
    void restartSimulation();
    void drawMainDockSpace();
    void drawControlsWindow();
    void drawSimulationViewWindow();

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
    ViewportRectangle simulationPanelViewport_;
    bool previousLeftMousePressed_ = false;

    std::unique_ptr<VelocityArrowRenderer> probeOverlayRenderer_;
    std::unique_ptr<Shader> probeOverlayShader_;
    std::vector<float> probeOverlayVertices_;

    SimulationConfig pendingConfig_;
    SimulationConfig activeConfig_;

    ImVec2 simulationViewPosition_{0.0f, 0.0f};
    ImVec2 simulationViewSize_{1.0f, 1.0f};
    bool simulationViewHovered_ = false;
};