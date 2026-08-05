#include "common/WebRuntime.h"

#include "GasBridge.h"

#include "physics/GasMode.h"
#include "rendering/GasSceneRenderer.h"
#include "simulation/GasSession.h"

#include <emscripten/emscripten.h>

namespace
{
    class WebGasBrownianApplication
    {
    public:
        WebGasBrownianApplication()
            : sceneRenderer{session}
        {
            physicslab::openCanvasWindow("Physics Lab - Gas and Brownian Motion");
            physicslab::setupImGuiInputStub();
            sceneRenderer.resetCamera();
            physicslab::gas::bindSession(&session);
        }

        ~WebGasBrownianApplication()
        {
            physicslab::gas::bindSession(nullptr);
            physicslab::shutdownWebRuntime();
        }

        void runFrame()
        {
            const float frameTime = physicslab::normalizedFrameTime(GetFrameTime());

            handleSimulationShortcuts();
            sceneRenderer.updateCamera(frameTime);
            updateSimulation(frameTime);
            draw();
            physicslab::gas::publishFrameState();
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

            physim::GasSimulation &simulation = session.getSimulation();

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
                simulation.setMode(physim::GasSimulationMode::IdealGas);
            }

            if (IsKeyPressed(KEY_TWO))
            {
                simulation.setMode(physim::GasSimulationMode::Brownian);
            }

            if (IsKeyPressed(KEY_T))
            {
                simulation.setTracerTrailEnabled(!simulation.isTracerTrailEnabled());
            }
        }

        void updateSimulation(float frameTime)
        {
            physim::GasSimulation &simulation = session.getSimulation();
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

        physim::GasSession session;
        physim::GasSceneRenderer sceneRenderer;
        bool performanceLimited{false};
    };

    WebGasBrownianApplication *applicationInstance{nullptr};

    void runFrame(void *application)
    {
        static_cast<WebGasBrownianApplication *>(application)->runFrame();
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
    applicationInstance = new WebGasBrownianApplication();
    emscripten_set_main_loop_arg(runFrame, applicationInstance, 0, true);
    return 0;
}
