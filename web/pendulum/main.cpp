#include "core/RaylibDefinitions.h"
#include "physics/PendulumMode.h"
#include "rendering/PendulumOverlayRenderer.h"
#include "rendering/PendulumSceneRenderer.h"
#include "simulation/PendulumSession.h"
#include "ui/PendulumUiMenus.h"

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include <algorithm>
#include <cmath>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace
{
    constexpr int DEFAULT_CANVAS_WIDTH = 1280;
    constexpr int DEFAULT_CANVAS_HEIGHT = 720;
    constexpr float DEFAULT_FRAME_TIME = 1.0f / 60.0f;
    constexpr float MAX_FRAME_TIME = 1.0f / 30.0f;
    constexpr float MAX_PHYSICS_SUBSTEPS_PER_FRAME = 1000.0f;
    constexpr float WEB_UI_FONT_SIZE = 18.0f;
    constexpr float WEB_UI_CONTROL_SCALE = 1.15f;

    struct CanvasSize
    {
        int width;
        int height;
    };

    CanvasSize canvasCssSize()
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

    void setupWebImGui()
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

    class WebPendulumApplication
    {
    public:
        WebPendulumApplication()
            : uiMenus{session},
              sceneRenderer{session},
              overlayRenderer{session}
        {
                        const CanvasSize initialCanvasSize = canvasCssSize();
                        InitWindow(initialCanvasSize.width, initialCanvasSize.height, "Physics Lab - Pendulum");
            SetExitKey(KEY_NULL);
                        setupWebImGui();
            sceneRenderer.resetCamera();
        }

        ~WebPendulumApplication()
        {
            rlImGuiShutdown();
            CloseWindow();
        }

        void runFrame()
        {
            const float frameTime = normalizedFrameTime(GetFrameTime());

            handleSimulationShortcuts();
            sceneRenderer.updateCamera(frameTime);
            updateSimulation(frameTime);
            draw();
        }

        float simulationTime() const
        {
            return session.getSimulation().getTimeGlobal();
        }

        void resizeCanvas(int width, int height)
        {
            width = std::max(1, width);
            height = std::max(1, height);

            if (width != GetScreenWidth() || height != GetScreenHeight())
            {
                SetWindowSize(width, height);
            }
        }

    private:
        static float normalizedFrameTime(float frameTime)
        {
            if (!std::isfinite(frameTime) || frameTime <= 0.0f)
            {
                return DEFAULT_FRAME_TIME;
            }

            return std::min(frameTime, MAX_FRAME_TIME);
        }

        void handleSimulationShortcuts()
        {
            if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput)
            {
                return;
            }

            physim::PendulumSimulation &simulation = session.getSimulation();

            if (IsKeyPressed(KEY_SPACE))
            {
                simulation.isRunning() ? simulation.stop() : simulation.start();
            }

            if (IsKeyPressed(KEY_R))
            {
                simulation.reset();
            }

            if (IsKeyPressed(KEY_ONE))
            {
                simulation.setMode(physim::PendulumMode::Simple);
            }

            if (IsKeyPressed(KEY_TWO))
            {
                simulation.setMode(physim::PendulumMode::Double);
            }
        }

        void updateSimulation(float frameTime)
        {
            physim::PendulumSimulation &simulation = session.getSimulation();
            const float requestedTimeStep = frameTime * session.getEnvironment().timeScale;
            const float maximumTimeStep = simulation.getPhysicsTimeStep() * MAX_PHYSICS_SUBSTEPS_PER_FRAME;
            const float boundedTimeStep = std::min(requestedTimeStep, maximumTimeStep);

            performanceLimited = boundedTimeStep < requestedTimeStep;
            simulation.update(boundedTimeStep);
        }

        void draw()
        {
            BeginDrawing();
            ClearBackground(physim::colors::CatppuccinMocha);

            sceneRenderer.render();
            overlayRenderer.render();

            if (performanceLimited)
            {
                DrawText("Performance limited: simulation time is advancing more slowly.",
                         20,
                         GetScreenHeight() - 52,
                         16,
                         physim::colors::Yellow);
            }

            uiMenus.render();
            EndDrawing();
        }

        physim::PendulumSession session;
        physim::PendulumUiMenus uiMenus;
        physim::PendulumSceneRenderer sceneRenderer;
        physim::PendulumOverlayRenderer overlayRenderer;
        bool performanceLimited{false};
    };

    WebPendulumApplication *applicationInstance{nullptr};

    void runFrame(void *application)
    {
        static_cast<WebPendulumApplication *>(application)->runFrame();
    }
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabPendulumSimulationTime()
{
    return applicationInstance == nullptr ? 0.0f : applicationInstance->simulationTime();
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabPendulumMouseX()
{
    return GetMousePosition().x;
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabPendulumMouseY()
{
    return GetMousePosition().y;
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabResizePendulumCanvas(int width, int height)
{
    if (applicationInstance != nullptr)
    {
        applicationInstance->resizeCanvas(width, height);
    }
}

int main()
{
    applicationInstance = new WebPendulumApplication();
    emscripten_set_main_loop_arg(runFrame, applicationInstance, 0, true);
    return 0;
}
