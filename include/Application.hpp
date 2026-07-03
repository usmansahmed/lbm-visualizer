#pragma once

#include "Renderer.hpp"
#include "ScalarTexture.hpp"
#include "Shader.hpp"
#include "SimulationFrame.hpp"
#include "VisualizationState.hpp"
#include "VtiReader.hpp"
#include "UserInterface.hpp"

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

    void loadInitialFrame();
    void updatePlayback();
    void updateVisualization();
    void render();
    void setOrientation(SliceOrientation orientation);
    void setTextureViewport();
    GLFWwindow *window_ = nullptr;

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ScalarTexture> texture_;
    std::unique_ptr<ScalarTexture> obstacleTexture_;
    std::unique_ptr<UserInterface> userInterface_;

    SimulationFrame frame_;
    VisualizationState state_;

    std::vector<float> slice_;
    std::vector<float> obstacleSlice_;

    int textureWidth_;
    int textureHeight_;

    double lastFrameTime_ = 0.0;

    std::filesystem::path dataDirectory_{"assets/simulation_output"};
    int currentFileNumber_ = 0;

    int firstFileNumber_ = 0;
    int lastFileNumber_ = 17200;
    int fileStep_ = 400;

    double playbackInterval_ = 0.1;
    double lastPlaybackTime_ = 0.0;
};