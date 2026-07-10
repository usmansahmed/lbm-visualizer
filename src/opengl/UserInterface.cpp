#include "UserInterface.hpp"
#include "SliceExtractor.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <stdexcept>

UserInterface::UserInterface(GLFWwindow *window)
{
    if (window == nullptr)
    {
        throw std::invalid_argument("Cannot initialize ImGui with a null window.");
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize ImGui GLFW backend.");
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        throw std::runtime_error("Failed to initialize ImGui OpenGL backend.");
    }
}

UserInterface::~UserInterface()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UserInterface::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UserInterface::render()
{
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(
        ImGui::GetDrawData());
}

void UserInterface::drawControls(VisualizationState &state, SimulationConfig &pendingConfig, const ProbeResult &probe)
{
    static const char *orientationNames[] = {
        "XY",
        "XZ",
        "YZ"};

    static const char *displayFieldNames[] = {
        "Velocity magnitude",
        "Velocity X",
        "Velocity Y",
        "Velocity Z",
        "Density",
        "Obstacle"};

    ImGui::Begin("LBM Controls");

    ImGui::SeparatorText("Simulation Setup");

    ImGui::InputInt("Nx", &pendingConfig.nx);
    ImGui::InputInt("Ny", &pendingConfig.ny);
    ImGui::InputInt("Nz", &pendingConfig.nz);

    pendingConfig.nx = std::max(8, pendingConfig.nx);
    pendingConfig.ny = std::max(8, pendingConfig.ny);
    pendingConfig.nz = std::max(1, pendingConfig.nz);

    ImGui::SliderFloat("Omega", &pendingConfig.omega, 0.8f, 1.8f);

    const float tau = 1.0f / pendingConfig.omega;
    const float viscosity = (1.0f / 3.0f) * (tau - 0.5f);

    ImGui::Text("tau = %.4f", tau);
    ImGui::Text("nu  = %.6f", viscosity);

    ImGui::SliderFloat("Inlet velocity", &pendingConfig.inletVelocity, 0.0f, 0.1f);

    const char *obstacleNames[] =
        {
            "None",
            "Single block",
            "Two blocks",
            "Single cylinder",
            "Two cylinders"};

    int preset =
        static_cast<int>(pendingConfig.obstaclePreset);

    if (ImGui::Combo(
            "Obstacle preset",
            &preset,
            obstacleNames,
            IM_ARRAYSIZE(obstacleNames)))
    {
        pendingConfig.obstaclePreset =
            static_cast<ObstaclePreset>(preset);
    }

    ImGui::SliderInt("Obstacle size", &pendingConfig.obstacleSize, 2, 32);

    ImGui::SliderInt("Obstacle radius", &pendingConfig.obstacleRadius, 2, 32);

    if (ImGui::Button("Apply / Restart Simulation"))
    {
        state.restartRequested = true;
    }

    int selectedDisplayField = static_cast<int>(state.displayField);

    if (ImGui::Combo("Display Field", &selectedDisplayField, displayFieldNames, IM_ARRAYSIZE(displayFieldNames)))
    {
        state.displayField = static_cast<DisplayField>(selectedDisplayField);
        state.fieldChanged = true;
    }

    int selectedOrientation = static_cast<int>(state.orientation);

    if (ImGui::Combo("Orientation", &selectedOrientation, orientationNames, IM_ARRAYSIZE(orientationNames)))
    {
        state.orientation = static_cast<SliceOrientation>(selectedOrientation);
        state.maximumSlice = getMaximumSliceIndex(state.orientation, state.nx, state.ny, state.nz);
        state.currentSlice = state.maximumSlice / 2;
        state.orientationChanged = true;
        state.sliceChanged = true;
    }

    if (ImGui::SliderInt("Slice", &state.currentSlice, 0, state.maximumSlice))
    {
        state.sliceChanged = true;
    }

    ImGui::Separator();

    if (ImGui::Button("Previous"))
    {
        if (state.currentSlice > 0)
        {
            --state.currentSlice;
            state.sliceChanged = true;
        }
    }

    if (ImGui::Button(state.playing ? "Pause" : "Resume"))
    {
        state.playing = !state.playing;
    }

    ImGui::SameLine();

    if (ImGui::Button("Next"))
    {
        if (state.currentSlice < state.maximumSlice)
        {
            ++state.currentSlice;
            state.sliceChanged = true;
        }
    }

    ImGui::Separator();

    ImGui::Text("Grid dimensions: %d x %d x %d", state.nx, state.ny, state.nz);

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::Checkbox("Automatic color scaling", &state.automaticColorScaling);

    if (!state.automaticColorScaling)
    {
        ImGui::DragFloatRange2("Color range", &state.minimumValue, &state.maximumValue,
                               0.005f, -10.0f, 10.0f, "Min: %.6f", "Max: %.6f",
                               ImGuiSliderFlags_AlwaysClamp);
    }

    ImGui::Text("Active range: %.6e to %.6e", state.finalMinimum, state.finalMaximum);

    if (state.displayField == DisplayField::VelocityMagnitude)
    {
        const ImU32 colors[] = {
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.5f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 1.0f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.5f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f))};
        drawColorBar(state.finalMinimum, state.finalMaximum, colors, 5);
    }
    else
    {
        const ImU32 colors[] = {
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 1.0f, 1.0f))};
        drawColorBar(state.finalMinimum, state.finalMaximum, colors, 3);
    }

    if (ImGui::Checkbox("Show velocity arrows", &state.showVelocityArrows))
    {
        state.arrowsChanged = true;
    }

    if (state.showVelocityArrows)
    {
        if (ImGui::SliderInt("Arrow spacing", &state.arrowStride, 2, 20))
        {
            state.arrowsChanged = true;
        }

        if (ImGui::SliderFloat("Arrow length", &state.arrowLengthScale, 0.1f, 1.0f))
        {
            state.arrowsChanged = true;
        }
    }

    ImGui::SeparatorText("Probe");
    if (!probe.valid)
    {
        ImGui::TextDisabled("Click the simulation to inspect a cell.");
    }
    else
    {
        ImGui::Text("Cell: (%d, %d, %d)", probe.x, probe.y, probe.z);
        ImGui::Text("Density: %.6e", probe.density);
        ImGui::Text("Velocity X: %.6e", probe.velocityX);
        ImGui::Text("Velocity Y: %.6e", probe.velocityY);
        ImGui::Text("Velocity Z: %.6e", probe.velocityZ);
        ImGui::Text("Speed: %.6e", probe.speed);
        ImGui::Text("Cell type: %s", probe.obstacle ? "Obstacle" : "Fluid");
    }

    ImGui::End();
}

void UserInterface::drawColorBar(float minimumValue, float maximumValue, const ImU32 colors[], int colorCount)
{
    const ImVec2 barStart = ImGui::GetCursorScreenPos();

    const float barWidth = ImGui::GetContentRegionAvail().x;

    constexpr float barHeight = 20.0f;

    const int segmentCount = colorCount - 1;

    const float segmentWidth = barWidth / static_cast<float>(segmentCount);

    ImDrawList *drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i < segmentCount; ++i)
    {
        const ImVec2 segmentStart{barStart.x + static_cast<float>(i) * segmentWidth, barStart.y};
        const ImVec2 segmentEnd{barStart.x + static_cast<float>(i + 1) * segmentWidth, barStart.y + barHeight};

        drawList->AddRectFilledMultiColor(
            segmentStart,
            segmentEnd,
            colors[i],
            colors[i + 1],
            colors[i + 1],
            colors[i]);
    }

    const ImVec2 barEnd{barStart.x + barWidth, barStart.y + barHeight};

    drawList->AddRect(barStart, barEnd, ImGui::GetColorU32(ImGuiCol_Border));

    ImGui::Dummy(ImVec2(barWidth, barHeight));

    char minimumText[32];
    char maximumText[32];

    std::snprintf(minimumText, sizeof(minimumText), "%.3e", minimumValue);

    std::snprintf(maximumText, sizeof(maximumText), "%.3e", maximumValue);

    ImGui::TextUnformatted(minimumText);

    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(maximumText).x);

    ImGui::TextUnformatted(maximumText);
}