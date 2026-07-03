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

void UserInterface::drawControls(VisualizationState &state, const SimulationFrame &frame)
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
        state.maximumSlice = getMaximumSliceIndex(state.orientation, frame.nx, frame.ny, frame.nz);
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

    ImGui::SameLine();

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

    ImGui::Text("Grid dimensions: %d x %d x %d", frame.nx, frame.ny, frame.nz);

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::Checkbox("Automatic color scaling", &state.automaticColorScaling);

    if (!state.automaticColorScaling)
    {
        ImGui::DragFloatRange2("Color range", &state.finalMinimum, &state.finalMaximum,
                               0.001f, -10.0f, 10.0f, "Min: %.4f", "Max: %.4f",
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