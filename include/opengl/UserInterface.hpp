#pragma once

#include "SimulationFrame.hpp"
#include "VisualizationState.hpp"
#include "ProbeResult.hpp"
#include "SimulationConfig.hpp"

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