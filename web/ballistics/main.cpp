#include "common/WebRuntime.h"

#include "BallisticsBridge.h"

#include "rendering/SimulationSceneRenderer.h"
#include "simulation/SimulationSession.h"

#include <emscripten/emscripten.h>

namespace
{
    class WebBallisticsApplication
    {
    public:
        WebBallisticsApplication()
            : sceneRenderer{session}
        {
            physicslab::openCanvasWindow("Physics Lab - Ballistics");
            physicslab::setupImGuiInputStub();
            physicslab::ballistics::bindSession(&session);
        }

        ~WebBallisticsApplication()
        {
            physicslab::ballistics::bindSession(nullptr);
            physicslab::shutdownWebRuntime();
        }

        void runFrame()
        {
            const float frameTime = physicslab::normalizedFrameTime(GetFrameTime());

            handleSimulationShortcuts();
            sceneRenderer.updateCamera(frameTime);
            updateSimulation(frameTime);
            draw();
            physicslab::ballistics::publishFrameState();
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
            const physicslab::ballistics::HighlightedLaunches highlights =
                physicslab::ballistics::highlightedLaunches();
            sceneRenderer.setHighlightedLaunches(highlights.selectedId, highlights.hoveredId);
        }

        // The control panel lives in the DOM, so the canvas only draws the scene.
        void draw()
        {
            BeginDrawing();
            ClearBackground(physim::colors::CatppuccinMocha);

            syncSceneHighlights();
            sceneRenderer.render();

            if (performanceLimited)
            {
                physicslab::drawPerformanceWarning();
            }

            EndDrawing();
        }

        physim::SimulationSession session;
        physim::SimulationSceneRenderer sceneRenderer;
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
