#include "GasBridge.h"

#include "common/ControlBridge.h"
#include "common/WebRuntime.h"

#include "physics/GasMode.h"
#include "physics/GasPreset.h"
#include "simulation/GasSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <emscripten/emscripten.h>

namespace
{
    using physicslab::bridge::fromBoolean;
    using physicslab::bridge::sanitize;
    using physicslab::bridge::toBoolean;

    // Ranges mirrored from the native Dear ImGui menus, which are not compiled
    // into the browser bundle. The simulation clamps most of them again, so this
    // layer mainly rejects values the page could not have produced.
    constexpr float MIN_TIME_SCALE = 0.1f;
    constexpr float MAX_TIME_SCALE = 20.0f;
    constexpr float MIN_BOX_WIDTH = 4.0f;
    constexpr float MAX_BOX_WIDTH = 30.0f;
    constexpr float MIN_BOX_HEIGHT = 2.0f;
    constexpr float MAX_BOX_HEIGHT = 20.0f;
    constexpr float MIN_PHYSICS_TIME_STEP = 1.0e-4f;
    constexpr float MAX_PHYSICS_TIME_STEP = 1.0f / 60.0f;
    constexpr float MIN_THERMAL_LEVEL = 20.0f;
    constexpr float MAX_THERMAL_LEVEL = 600.0f;
    constexpr float MIN_TRACER_MASS_SCALE = 1.0f;
    constexpr float MAX_TRACER_MASS_SCALE = 64.0f;
    constexpr float MIN_TRACER_RADIUS_SCALE = 1.0f;
    constexpr float MAX_TRACER_RADIUS_SCALE = 8.0f;
    constexpr float MIN_PARTICLE_COUNT = 1.0f;
    constexpr float MAX_PARTICLE_COUNT = 20000.0f;
    constexpr float MIN_RANDOM_SEED = 0.0f;
    constexpr float MAX_RANDOM_SEED = 1.0e9f;

    physim::GasSession *boundSession = nullptr;

    std::array<float, physicslab::gas::STATE_SLOT_COUNT> stateBuffer{};
    physicslab::bridge::SampleBuffer pressureBuffer;

    // The bath is rebuilt on every change, so it may only be reconfigured while
    // the simulation is not running, exactly as the native menus enforced.
    bool requiresEditableSimulation(int parameter)
    {
        switch (parameter)
        {
        case physicslab::gas::PARAMETER_TIME_SCALE:
        case physicslab::gas::PARAMETER_SHOW_GRID:
        case physicslab::gas::PARAMETER_SHOW_PRESSURE_PLOT:
        case physicslab::gas::PARAMETER_TRACER_TRAIL:
            return false;
        default:
            return true;
        }
    }

    void writeParameter(physim::GasSession &session, int parameter, float value)
    {
        physim::GasSimulation &simulation = session.getSimulation();
        physim::GasEnvironment &environment = session.getEnvironment();
        physim::GasVisualizationSettings &visualization = session.getVisualizationSettings();
        float sanitized = 0.0f;

        switch (parameter)
        {
        case physicslab::gas::PARAMETER_MODE:
            simulation.setMode(toBoolean(value) ? physim::GasSimulationMode::Brownian
                                                : physim::GasSimulationMode::IdealGas);
            break;
        case physicslab::gas::PARAMETER_TIME_SCALE:
            if (sanitize(value, MIN_TIME_SCALE, MAX_TIME_SCALE, sanitized))
            {
                environment.timeScale = sanitized;
            }
            break;
        case physicslab::gas::PARAMETER_PARTICLE_COUNT:
            if (sanitize(value, MIN_PARTICLE_COUNT, MAX_PARTICLE_COUNT, sanitized))
            {
                simulation.setParticleCount(static_cast<int>(std::lround(sanitized)));
            }
            break;
        case physicslab::gas::PARAMETER_GAS_PRESET:
        {
            const int presetIndex = static_cast<int>(std::lround(value));
            if (presetIndex >= 0 && presetIndex < physim::getGasPresetCount())
            {
                simulation.setGasPresetId(physim::getGasPresetId(presetIndex));
            }
            break;
        }
        case physicslab::gas::PARAMETER_BOX_WIDTH:
            if (sanitize(value, MIN_BOX_WIDTH, MAX_BOX_WIDTH, sanitized))
            {
                simulation.setBoxSize(sanitized, environment.boxHeight);
            }
            break;
        case physicslab::gas::PARAMETER_BOX_HEIGHT:
            if (sanitize(value, MIN_BOX_HEIGHT, MAX_BOX_HEIGHT, sanitized))
            {
                simulation.setBoxSize(environment.boxWidth, sanitized);
            }
            break;
        case physicslab::gas::PARAMETER_PHYSICS_TIME_STEP:
            if (sanitize(value, MIN_PHYSICS_TIME_STEP, MAX_PHYSICS_TIME_STEP, sanitized))
            {
                simulation.setPhysicsTimeStep(sanitized);
            }
            break;
        case physicslab::gas::PARAMETER_THERMAL_LEVEL:
            if (sanitize(value, MIN_THERMAL_LEVEL, MAX_THERMAL_LEVEL, sanitized))
            {
                simulation.setInitialTemperatureKelvin(sanitized);
            }
            break;
        case physicslab::gas::PARAMETER_RANDOM_SEED:
            if (sanitize(value, MIN_RANDOM_SEED, MAX_RANDOM_SEED, sanitized))
            {
                simulation.setRandomSeed(static_cast<unsigned int>(std::lround(sanitized)));
            }
            break;
        case physicslab::gas::PARAMETER_SHOW_GRID:
            visualization.showGrid = toBoolean(value);
            break;
        case physicslab::gas::PARAMETER_SHOW_PRESSURE_PLOT:
            visualization.showPressurePlot = toBoolean(value);
            break;
        case physicslab::gas::PARAMETER_TRACER_TRAIL:
            simulation.setTracerTrailEnabled(toBoolean(value));
            break;
        case physicslab::gas::PARAMETER_TRACER_MASS_SCALE:
            if (sanitize(value, MIN_TRACER_MASS_SCALE, MAX_TRACER_MASS_SCALE, sanitized))
            {
                simulation.setTracerMassMultiplier(sanitized);
            }
            break;
        case physicslab::gas::PARAMETER_TRACER_RADIUS_SCALE:
            if (sanitize(value, MIN_TRACER_RADIUS_SCALE, MAX_TRACER_RADIUS_SCALE, sanitized))
            {
                simulation.setTracerRadiusMultiplier(sanitized);
            }
            break;
        default:
            break;
        }
    }

    float readParameter(const physim::GasSession &session, int parameter)
    {
        const physim::GasSimulation &simulation = session.getSimulation();
        const physim::GasEnvironment &environment = session.getEnvironment();
        const physim::GasVisualizationSettings &visualization = session.getVisualizationSettings();

        switch (parameter)
        {
        case physicslab::gas::PARAMETER_MODE:
            return fromBoolean(simulation.isBrownianMode());
        case physicslab::gas::PARAMETER_TIME_SCALE:
            return environment.timeScale;
        case physicslab::gas::PARAMETER_PARTICLE_COUNT:
            return static_cast<float>(simulation.getParticleCount());
        case physicslab::gas::PARAMETER_GAS_PRESET:
            return static_cast<float>(physim::getGasPresetIndex(simulation.getGasPresetId()));
        case physicslab::gas::PARAMETER_BOX_WIDTH:
            return environment.boxWidth;
        case physicslab::gas::PARAMETER_BOX_HEIGHT:
            return environment.boxHeight;
        case physicslab::gas::PARAMETER_PHYSICS_TIME_STEP:
            return simulation.getPhysicsTimeStep();
        case physicslab::gas::PARAMETER_THERMAL_LEVEL:
            return simulation.getInitialTemperatureKelvin();
        case physicslab::gas::PARAMETER_RANDOM_SEED:
            return static_cast<float>(simulation.getRandomSeed());
        case physicslab::gas::PARAMETER_SHOW_GRID:
            return fromBoolean(visualization.showGrid);
        case physicslab::gas::PARAMETER_SHOW_PRESSURE_PLOT:
            return fromBoolean(visualization.showPressurePlot);
        case physicslab::gas::PARAMETER_TRACER_TRAIL:
            return fromBoolean(simulation.isTracerTrailEnabled());
        case physicslab::gas::PARAMETER_TRACER_MASS_SCALE:
            return simulation.getTracerMassMultiplier();
        case physicslab::gas::PARAMETER_TRACER_RADIUS_SCALE:
            return simulation.getTracerRadiusMultiplier();
        default:
            return 0.0f;
        }
    }

    void publishStateSnapshot(const physim::GasSession &session)
    {
        const physim::GasSimulation &simulation = session.getSimulation();

        stateBuffer[physicslab::gas::STATE_SIMULATION_STATE] =
            static_cast<float>(static_cast<int>(simulation.getState()));
        stateBuffer[physicslab::gas::STATE_MODE] = fromBoolean(simulation.isBrownianMode());
        stateBuffer[physicslab::gas::STATE_TIME] = simulation.getTimeGlobal();
        stateBuffer[physicslab::gas::STATE_PRESSURE] = simulation.getCurrentPressureInstant();
        stateBuffer[physicslab::gas::STATE_TEMPERATURE] = simulation.getCurrentTemperatureKelvin();
        stateBuffer[physicslab::gas::STATE_AVERAGE_KINETIC_ENERGY] = simulation.getAverageKineticEnergy();
        stateBuffer[physicslab::gas::STATE_AVERAGE_SPEED] = simulation.getAverageSpeed();
        stateBuffer[physicslab::gas::STATE_ACTIVE_PARTICLES] =
            static_cast<float>(simulation.getParticles().size());
    }
} // namespace

namespace physicslab::gas
{
    void bindSession(physim::GasSession *session)
    {
        boundSession = session;
    }

    void publishFrameState()
    {
        if (boundSession == nullptr)
        {
            return;
        }

        publishStateSnapshot(*boundSession);

        if (!boundSession->getVisualizationSettings().showPressurePlot)
        {
            pressureBuffer.clear();
            return;
        }

        physicslab::bridge::publishSamples(
            pressureBuffer,
            boundSession->getSimulation().getPressureHistory(),
            [](const physim::PressureHistorySample &sample) { return sample.time; },
            [](const physim::PressureHistorySample &sample) { return sample.pressure; });
    }
} // namespace physicslab::gas

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabStart()
{
    if (boundSession != nullptr)
    {
        boundSession->getSimulation().start();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabStop()
{
    if (boundSession != nullptr)
    {
        boundSession->getSimulation().stop();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabReset()
{
    if (boundSession != nullptr)
    {
        boundSession->getSimulation().reset();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabSetParameter(int parameter, float value)
{
    if (boundSession == nullptr || parameter < 0 || parameter >= physicslab::gas::PARAMETER_COUNT)
    {
        return;
    }

    if (requiresEditableSimulation(parameter) && boundSession->getSimulation().isRunning())
    {
        return;
    }

    writeParameter(*boundSession, parameter, value);
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabGetParameter(int parameter)
{
    if (boundSession == nullptr || parameter < 0 || parameter >= physicslab::gas::PARAMETER_COUNT)
    {
        return 0.0f;
    }

    return readParameter(*boundSession, parameter);
}

extern "C" EMSCRIPTEN_KEEPALIVE const float *physicsLabStateBuffer()
{
    return stateBuffer.data();
}

extern "C" EMSCRIPTEN_KEEPALIVE int physicsLabStateLength()
{
    return physicslab::gas::STATE_SLOT_COUNT;
}

extern "C" EMSCRIPTEN_KEEPALIVE const float *physicsLabSeriesSamples()
{
    return pressureBuffer.data();
}

extern "C" EMSCRIPTEN_KEEPALIVE int physicsLabSeriesSampleCount()
{
    return pressureBuffer.count();
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabSetKeyboardCaptured(int captured)
{
    physicslab::keyboardCapturedByDocument() = captured != 0;
}
