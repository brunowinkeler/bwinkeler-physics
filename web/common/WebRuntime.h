#ifndef PHYSICS_LAB_WEB_RUNTIME_H
#define PHYSICS_LAB_WEB_RUNTIME_H

#include "core/RaylibDefinitions.h"

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include <algorithm>
#include <cmath>
#include <emscripten/html5.h>

// Browser-only presentation and frame policy shared by every simulation
// adapter. The native repository keeps its own desktop lifecycle untouched.
namespace physicslab
{
    inline constexpr int DEFAULT_CANVAS_WIDTH = 1280;
    inline constexpr int DEFAULT_CANVAS_HEIGHT = 720;
    inline constexpr float DEFAULT_FRAME_TIME = 1.0f / 60.0f;
    inline constexpr float MAX_FRAME_TIME = 1.0f / 30.0f;
    inline constexpr float MAX_PHYSICS_SUBSTEPS_PER_FRAME = 1000.0f;
    inline constexpr float WEB_UI_FONT_SIZE = 18.0f;
    inline constexpr float WEB_UI_CONTROL_SCALE = 1.15f;

    struct CanvasSize
    {
        int width;
        int height;
    };

    inline CanvasSize canvasCssSize()
    {
        double width = 0.0;
        double height = 0.0;
        const EMSCRIPTEN_RESULT result = emscripten_get_element_css_size("#canvas", &width, &height);

        if (result != EMSCRIPTEN_RESULT_SUCCESS || !std::isfinite(width) || !std::isfinite(height) ||
            width < 1.0 || height < 1.0)
        {
            return {DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT};
        }

        return {
            std::max(1, static_cast<int>(std::lround(width))),
            std::max(1, static_cast<int>(std::lround(height))),
        };
    }

    // Opens the Raylib window using one drawing coordinate per canvas CSS pixel
    // so pointer input stays aligned with the rendered scene.
    inline void openCanvasWindow(const char *title)
    {
        const CanvasSize initialCanvasSize = canvasCssSize();
        InitWindow(initialCanvasSize.width, initialCanvasSize.height, title);
        SetExitKey(KEY_NULL);
    }

    inline void setupWebImGui()
    {
        rlImGuiBeginInitImGui();
        ImGui::StyleColorsDark();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.Fonts->Clear();

        ImFontConfig fontConfig;
        fontConfig.SizePixels = WEB_UI_FONT_SIZE;
        fontConfig.PixelSnapH = true;
        io.Fonts->AddFontDefault(&fontConfig);

        ImGui::GetStyle().ScaleAllSizes(WEB_UI_CONTROL_SCALE);
        rlImGuiEndInitImGui();
    }

    inline void shutdownWebRuntime()
    {
        rlImGuiShutdown();
        CloseWindow();
    }

    inline bool keyboardAvailable()
    {
        return !ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantTextInput;
    }

    inline float normalizedFrameTime(float frameTime)
    {
        if (!std::isfinite(frameTime) || frameTime <= 0.0f)
        {
            return DEFAULT_FRAME_TIME;
        }

        return std::min(frameTime, MAX_FRAME_TIME);
    }

    // Caps how much simulated time a single rendered frame may request so a
    // heavy scene slows down instead of blocking the browser thread.
    inline float boundedTimeStep(float requestedTimeStep, float physicsTimeStep)
    {
        return std::min(requestedTimeStep, physicsTimeStep * MAX_PHYSICS_SUBSTEPS_PER_FRAME);
    }

    inline void drawPerformanceWarning()
    {
        DrawText("Performance limited: simulation time is advancing more slowly.",
                 20,
                 GetScreenHeight() - 52,
                 16,
                 physim::colors::Yellow);
    }

    inline void resizeCanvas(int width, int height)
    {
        width = std::max(1, width);
        height = std::max(1, height);

        if (width != GetScreenWidth() || height != GetScreenHeight())
        {
            SetWindowSize(width, height);
        }
    }
} // namespace physicslab

#endif // PHYSICS_LAB_WEB_RUNTIME_H
