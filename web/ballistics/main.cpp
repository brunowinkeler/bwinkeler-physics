#include "common/WebRuntime.h"

#include "rendering/SimulationOverlayRenderer.h"
#include "rendering/SimulationSceneRenderer.h"
#include "simulation/SimulationSession.h"
#include "ui/UiMenus.h"

#include <emscripten/emscripten.h>

namespace
{
    class WebBallisticsApplication
    {
    public:
        WebBallisticsApplication()
            : uiMenus{session},
              sceneRenderer{session},
              overlayRenderer{session.getSimulation()}
        {
            physicslab::openCanvasWindow("Physics Lab - Ballistics");
            physicslab::setupWebImGui();
        }

        ~WebBallisticsApplication()
        {
            physicslab::shutdownWebRuntime();
        }

        void runFrame()
        {
            const float frameTime = physicslab::normalizedFrameTime(GetFrameTime());

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
        void handleSimulationShortcuts()
        {
            if (!physicslab::keyboardAvailable())
            {
                return;
            }

            physim::Simulation &simulation = session.getSimulation();

            if (IsKeyPressed(KEY_SPACE))
            {
                simulation.isRunning() ? simulation.stop() : simulation.start();
            }

            if (IsKeyPressed(KEY_R))
            {
                simulation.reset();
            }
        }

        void updateSimulation(float frameTime)
        {
            physim::Simulation &simulation = session.getSimulation();
            const float requestedTimeStep = frameTime * session.getEnvironment().timeScale;
            const float timeStep =
                physicslab::boundedTimeStep(requestedTimeStep, simulation.getPhysicsTimeStep());

            performanceLimited = timeStep < requestedTimeStep;
            simulation.update(timeStep);
        }

        void syncSceneHighlights()
        {
            sceneRenderer.setHighlightedLaunches(uiMenus.getSelectedHistoryEntryId(),
                                                 uiMenus.getHoveredHistoryEntryId());
        }

        void draw()
        {
            BeginDrawing();
            ClearBackground(physim::colors::CatppuccinMocha);

            syncSceneHighlights();
            sceneRenderer.render();
            overlayRenderer.render();

            if (performanceLimited)
            {
                physicslab::drawPerformanceWarning();
            }

            uiMenus.parametersSelectionScreen();
            syncSceneHighlights();
            EndDrawing();
        }

        physim::SimulationSession session;
        physim::UiMenus uiMenus;
        physim::SimulationSceneRenderer sceneRenderer;
        physim::SimulationOverlayRenderer overlayRenderer;
        bool performanceLimited{false};
    };

    WebBallisticsApplication *applicationInstance{nullptr};

    void runFrame(void *application)
    {
        static_cast<WebBallisticsApplication *>(application)->runFrame();
    }
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabSimulationTime()
{
    return applicationInstance == nullptr ? 0.0f : applicationInstance->simulationTime();
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabMouseX()
{
    return GetMousePosition().x;
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabMouseY()
{
    return GetMousePosition().y;
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabResizeCanvas(int width, int height)
{
    physicslab::resizeCanvas(width, height);
}

int main()
{
    applicationInstance = new WebBallisticsApplication();
    emscripten_set_main_loop_arg(runFrame, applicationInstance, 0, true);
    return 0;
}
