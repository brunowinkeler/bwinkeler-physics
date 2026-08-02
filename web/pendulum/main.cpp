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

namespace
{
    constexpr float DEFAULT_FRAME_TIME = 1.0f / 60.0f;
    constexpr float MAX_FRAME_TIME = 1.0f / 30.0f;
    constexpr float MAX_PHYSICS_SUBSTEPS_PER_FRAME = 1000.0f;

    class WebPendulumApplication
    {
    public:
        WebPendulumApplication()
            : uiMenus{session},
              sceneRenderer{session},
              overlayRenderer{session}
        {
            SetConfigFlags(FLAG_WINDOW_RESIZABLE);
            InitWindow(1280, 720, "Physics Lab - Pendulum");
            SetExitKey(KEY_NULL);
            rlImGuiSetup(true);
            ImGui::GetIO().IniFilename = nullptr;
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

int main()
{
    applicationInstance = new WebPendulumApplication();
    emscripten_set_main_loop_arg(runFrame, applicationInstance, 0, true);
    return 0;
}
