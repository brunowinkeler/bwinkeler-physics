#include "common/WebRuntime.h"

#include "PendulumBridge.h"

#include "physics/PendulumMode.h"
#include "rendering/PendulumSceneRenderer.h"
#include "simulation/PendulumSession.h"

#include <emscripten/emscripten.h>

namespace
{
    class WebPendulumApplication
    {
    public:
        WebPendulumApplication()
            : sceneRenderer{session}
        {
            physicslab::openCanvasWindow("Physics Lab - Pendulum");
            physicslab::setupImGuiInputStub();
            sceneRenderer.resetCamera();
            physicslab::pendulum::bindSession(&session);
        }

        ~WebPendulumApplication()
        {
            physicslab::pendulum::bindSession(nullptr);
            physicslab::shutdownWebRuntime();
        }

        void runFrame()
        {
            const float frameTime = physicslab::normalizedFrameTime(GetFrameTime());

            handleSimulationShortcuts();
            sceneRenderer.updateCamera(frameTime);
            updateSimulation(frameTime);
            draw();
            physicslab::pendulum::publishFrameState();
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
            const float timeStep =
                physicslab::boundedTimeStep(requestedTimeStep, simulation.getPhysicsTimeStep());

            performanceLimited = timeStep < requestedTimeStep;
            simulation.update(timeStep);
        }

        // The control panel lives in the DOM, so the canvas only draws the scene.
        void draw()
        {
            BeginDrawing();
            ClearBackground(physim::colors::CatppuccinMocha);

            sceneRenderer.render();

            if (performanceLimited)
            {
                physicslab::drawPerformanceWarning();
            }

            EndDrawing();
        }

        physim::PendulumSession session;
        physim::PendulumSceneRenderer sceneRenderer;
        bool performanceLimited{false};
    };

    WebPendulumApplication *applicationInstance{nullptr};

    void runFrame(void *application)
    {
        static_cast<WebPendulumApplication *>(application)->runFrame();
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
    applicationInstance = new WebPendulumApplication();
    emscripten_set_main_loop_arg(runFrame, applicationInstance, 0, true);
    return 0;
}
