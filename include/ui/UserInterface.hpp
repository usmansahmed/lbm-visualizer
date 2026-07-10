#pragma once

#include "app/VisualizationState.hpp"
#include "app/ProbeResult.hpp"
#include "app/SimulationConfig.hpp"

#include <imgui.h>

struct GLFWwindow;

class UserInterface
{
public:
    explicit UserInterface(GLFWwindow* window);
    ~UserInterface();

    UserInterface(const UserInterface&) = delete;
    UserInterface& operator=(const UserInterface&) = delete;

    void beginFrame();
    void drawControls(VisualizationState& state, SimulationConfig &pendingConfig ,const ProbeResult& probe);
    void render();

private:
    void drawColorBar(float minimumValue, float maximumValue, const ImU32 colors[], int colorCount);
};