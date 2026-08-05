#ifndef PHYSICS_LAB_WEB_RUNTIME_H
#define PHYSICS_LAB_WEB_RUNTIME_H

#include "core/RaylibDefinitions.h"

#include "imgui.h"
#include "raylib.h"

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

    // Controls live in the DOM, but the pinned native camera classes still read
    // Dear ImGui IO to decide whether pointer and keyboard input belong to the
    // scene. Creating the context without ever starting a frame keeps every
    // WantCapture flag false, which is the answer those classes need.
    inline void setupImGuiInputStub()
    {
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
    }

    inline void shutdownWebRuntime()
    {
        ImGui::DestroyContext();
        CloseWindow();
    }

    // Raylib listens for key events on the document, so the control panel has to
    // announce when it owns the keyboard or typing a parameter would also
    // trigger simulation shortcuts.
    inline bool &keyboardCapturedByDocument()
    {
        static bool captured = false;
        return captured;
    }

    inline bool keyboardAvailable()
    {
        return !keyboardCapturedByDocument();
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
