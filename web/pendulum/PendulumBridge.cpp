#include "PendulumBridge.h"

#include "common/ControlBridge.h"
#include "common/WebRuntime.h"

#include "core/physics/IntegrationMethod.h"
#include "physics/PendulumMode.h"
#include "simulation/PendulumSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <emscripten/emscripten.h>
#include <numbers>
#include <vector>

namespace
{
    using physicslab::bridge::fromBoolean;
    using physicslab::bridge::sanitize;
    using physicslab::bridge::toBoolean;

    // Ranges mirrored from the native Dear ImGui menus, which are not compiled
    // into the browser bundle. The page repeats them as input affordances, but
    // this layer is the authority that keeps the physics in a valid domain.
    constexpr float MIN_GRAVITY = 0.0f;
    constexpr float MAX_GRAVITY = 50.0f;
    constexpr float MIN_TIME_SCALE = 0.05f;
    constexpr float MAX_TIME_SCALE = 20.0f;
    constexpr float MIN_PHYSICS_TIME_STEP = 0.0001f;
    constexpr float MAX_PHYSICS_TIME_STEP = 0.05f;
    constexpr float MIN_LENGTH = 0.1f;
    constexpr float MAX_LENGTH = 5.0f;
    constexpr float MIN_MASS = 0.05f;
    constexpr float MAX_MASS = 25.0f;
    constexpr float MIN_DAMPING = 0.0f;
    constexpr float MAX_DAMPING = 2.0f;
    constexpr float MIN_INITIAL_ANGLE = -179.0f;
    constexpr float MAX_INITIAL_ANGLE = 179.0f;
    constexpr float MIN_INITIAL_ANGULAR_VELOCITY = -20.0f;
    constexpr float MAX_INITIAL_ANGULAR_VELOCITY = 20.0f;
    constexpr float MIN_PERIOD_GRAVITY = 0.01f;

    physim::PendulumSession *boundSession = nullptr;

    std::array<float, physicslab::pendulum::STATE_SLOT_COUNT> stateBuffer{};
    physicslab::bridge::SampleBuffer energyBuffer;
    std::array<physicslab::bridge::SampleBuffer, physicslab::pendulum::PHASE_TRACE_COUNT> phaseBuffers;

    physim::IntegrationMethod activeIntegrationMethod(const physim::PendulumSession &session)
    {
        return session.getSimulation().isSimpleMode()
                   ? session.getSimplePendulum().getIntegrationMethod()
                   : session.getDoublePendulum().getIntegrationMethod();
    }

    void applyIntegrationMethod(physim::PendulumSession &session, physim::IntegrationMethod method)
    {
        if (!physim::isSupportedIntegrationMethod(method))
        {
            return;
        }

        if (session.getSimulation().isSimpleMode())
        {
            session.getSimplePendulum().setIntegrationMethod(method);
            return;
        }

        session.getDoublePendulum().setIntegrationMethod(method);
    }

    // Initial conditions and integration settings are only editable while the
    // simulation is idle, matching the native menus.
    bool requiresIdleSimulation(int parameter)
    {
        switch (parameter)
        {
        case physicslab::pendulum::PARAMETER_TIME_SCALE:
        case physicslab::pendulum::PARAMETER_SHOW_GRID:
        case physicslab::pendulum::PARAMETER_SHOW_AXES:
        case physicslab::pendulum::PARAMETER_SHOW_PRIMARY_TRAIL:
        case physicslab::pendulum::PARAMETER_SHOW_SECONDARY_TRAIL:
            return false;
        default:
            return true;
        }
    }

    void writeParameter(physim::PendulumSession &session, int parameter, float value)
    {
        physim::PendulumSimulation &simulation = session.getSimulation();
        physim::PendulumEnvironment &environment = session.getEnvironment();
        physim::SimplePendulum &simplePendulum = session.getSimplePendulum();
        physim::DoublePendulum &doublePendulum = session.getDoublePendulum();
        physim::PendulumVisualizationSettings &visualization = session.getVisualizationSettings();
        float sanitized = 0.0f;

        switch (parameter)
        {
        case physicslab::pendulum::PARAMETER_MODE:
            simulation.setMode(toBoolean(value) ? physim::PendulumMode::Double : physim::PendulumMode::Simple);
            break;
        case physicslab::pendulum::PARAMETER_GRAVITY:
            if (sanitize(value, MIN_GRAVITY, MAX_GRAVITY, sanitized))
            {
                environment.gravity = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_TIME_SCALE:
            if (sanitize(value, MIN_TIME_SCALE, MAX_TIME_SCALE, sanitized))
            {
                environment.timeScale = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_PHYSICS_TIME_STEP:
            if (sanitize(value, MIN_PHYSICS_TIME_STEP, MAX_PHYSICS_TIME_STEP, sanitized))
            {
                simulation.setPhysicsTimeStep(sanitized);
            }
            break;
        case physicslab::pendulum::PARAMETER_INTEGRATION_METHOD:
            applyIntegrationMethod(session, static_cast<physim::IntegrationMethod>(static_cast<int>(value)));
            break;
        case physicslab::pendulum::PARAMETER_SHOW_GRID:
            visualization.showGrid = toBoolean(value);
            break;
        case physicslab::pendulum::PARAMETER_SHOW_AXES:
            visualization.showAxes = toBoolean(value);
            break;
        case physicslab::pendulum::PARAMETER_SHOW_PRIMARY_TRAIL:
            visualization.showPrimaryTrail = toBoolean(value);
            break;
        case physicslab::pendulum::PARAMETER_SHOW_SECONDARY_TRAIL:
            visualization.showSecondaryTrail = toBoolean(value);
            break;
        case physicslab::pendulum::PARAMETER_SIMPLE_LENGTH:
            if (sanitize(value, MIN_LENGTH, MAX_LENGTH, sanitized))
            {
                simplePendulum.getLength() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_SIMPLE_MASS:
            if (sanitize(value, MIN_MASS, MAX_MASS, sanitized))
            {
                simplePendulum.getMass() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_SIMPLE_DAMPING:
            if (sanitize(value, MIN_DAMPING, MAX_DAMPING, sanitized))
            {
                simplePendulum.getDamping() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_SIMPLE_INITIAL_ANGLE:
            if (sanitize(value, MIN_INITIAL_ANGLE, MAX_INITIAL_ANGLE, sanitized))
            {
                simplePendulum.getInitialAngleDegrees() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_SIMPLE_INITIAL_ANGULAR_VELOCITY:
            if (sanitize(value, MIN_INITIAL_ANGULAR_VELOCITY, MAX_INITIAL_ANGULAR_VELOCITY, sanitized))
            {
                simplePendulum.getInitialAngularVelocity() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_LENGTH_1:
            if (sanitize(value, MIN_LENGTH, MAX_LENGTH, sanitized))
            {
                doublePendulum.getLength1() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_LENGTH_2:
            if (sanitize(value, MIN_LENGTH, MAX_LENGTH, sanitized))
            {
                doublePendulum.getLength2() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_MASS_1:
            if (sanitize(value, MIN_MASS, MAX_MASS, sanitized))
            {
                doublePendulum.getMass1() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_MASS_2:
            if (sanitize(value, MIN_MASS, MAX_MASS, sanitized))
            {
                doublePendulum.getMass2() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_DAMPING_1:
            if (sanitize(value, MIN_DAMPING, MAX_DAMPING, sanitized))
            {
                doublePendulum.getDamping1() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_DAMPING_2:
            if (sanitize(value, MIN_DAMPING, MAX_DAMPING, sanitized))
            {
                doublePendulum.getDamping2() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGLE_1:
            if (sanitize(value, MIN_INITIAL_ANGLE, MAX_INITIAL_ANGLE, sanitized))
            {
                doublePendulum.getInitialAngle1Degrees() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGLE_2:
            if (sanitize(value, MIN_INITIAL_ANGLE, MAX_INITIAL_ANGLE, sanitized))
            {
                doublePendulum.getInitialAngle2Degrees() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGULAR_VELOCITY_1:
            if (sanitize(value, MIN_INITIAL_ANGULAR_VELOCITY, MAX_INITIAL_ANGULAR_VELOCITY, sanitized))
            {
                doublePendulum.getInitialAngularVelocity1() = sanitized;
            }
            break;
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGULAR_VELOCITY_2:
            if (sanitize(value, MIN_INITIAL_ANGULAR_VELOCITY, MAX_INITIAL_ANGULAR_VELOCITY, sanitized))
            {
                doublePendulum.getInitialAngularVelocity2() = sanitized;
            }
            break;
        default:
            break;
        }
    }

    float readParameter(const physim::PendulumSession &session, int parameter)
    {
        const physim::PendulumSimulation &simulation = session.getSimulation();
        const physim::PendulumEnvironment &environment = session.getEnvironment();
        const physim::SimplePendulum &simplePendulum = session.getSimplePendulum();
        const physim::DoublePendulum &doublePendulum = session.getDoublePendulum();
        const physim::PendulumVisualizationSettings &visualization = session.getVisualizationSettings();

        switch (parameter)
        {
        case physicslab::pendulum::PARAMETER_MODE:
            return fromBoolean(simulation.isDoubleMode());
        case physicslab::pendulum::PARAMETER_GRAVITY:
            return environment.gravity;
        case physicslab::pendulum::PARAMETER_TIME_SCALE:
            return environment.timeScale;
        case physicslab::pendulum::PARAMETER_PHYSICS_TIME_STEP:
            return simulation.getPhysicsTimeStep();
        case physicslab::pendulum::PARAMETER_INTEGRATION_METHOD:
            return static_cast<float>(static_cast<int>(activeIntegrationMethod(session)));
        case physicslab::pendulum::PARAMETER_SHOW_GRID:
            return fromBoolean(visualization.showGrid);
        case physicslab::pendulum::PARAMETER_SHOW_AXES:
            return fromBoolean(visualization.showAxes);
        case physicslab::pendulum::PARAMETER_SHOW_PRIMARY_TRAIL:
            return fromBoolean(visualization.showPrimaryTrail);
        case physicslab::pendulum::PARAMETER_SHOW_SECONDARY_TRAIL:
            return fromBoolean(visualization.showSecondaryTrail);
        case physicslab::pendulum::PARAMETER_SIMPLE_LENGTH:
            return simplePendulum.getLength();
        case physicslab::pendulum::PARAMETER_SIMPLE_MASS:
            return simplePendulum.getMass();
        case physicslab::pendulum::PARAMETER_SIMPLE_DAMPING:
            return simplePendulum.getDamping();
        case physicslab::pendulum::PARAMETER_SIMPLE_INITIAL_ANGLE:
            return simplePendulum.getInitialAngleDegrees();
        case physicslab::pendulum::PARAMETER_SIMPLE_INITIAL_ANGULAR_VELOCITY:
            return simplePendulum.getInitialAngularVelocity();
        case physicslab::pendulum::PARAMETER_DOUBLE_LENGTH_1:
            return doublePendulum.getLength1();
        case physicslab::pendulum::PARAMETER_DOUBLE_LENGTH_2:
            return doublePendulum.getLength2();
        case physicslab::pendulum::PARAMETER_DOUBLE_MASS_1:
            return doublePendulum.getMass1();
        case physicslab::pendulum::PARAMETER_DOUBLE_MASS_2:
            return doublePendulum.getMass2();
        case physicslab::pendulum::PARAMETER_DOUBLE_DAMPING_1:
            return doublePendulum.getDamping1();
        case physicslab::pendulum::PARAMETER_DOUBLE_DAMPING_2:
            return doublePendulum.getDamping2();
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGLE_1:
            return doublePendulum.getInitialAngle1Degrees();
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGLE_2:
            return doublePendulum.getInitialAngle2Degrees();
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGULAR_VELOCITY_1:
            return doublePendulum.getInitialAngularVelocity1();
        case physicslab::pendulum::PARAMETER_DOUBLE_INITIAL_ANGULAR_VELOCITY_2:
            return doublePendulum.getInitialAngularVelocity2();
        default:
            return 0.0f;
        }
    }

    float smallAnglePeriod(const physim::PendulumSession &session)
    {
        const float gravity = std::max(session.getEnvironment().gravity, MIN_PERIOD_GRAVITY);
        const float length = session.getSimplePendulum().getLength();

        return 2.0f * std::numbers::pi_v<float> * std::sqrt(length / gravity);
    }

    void publishStateSnapshot(const physim::PendulumSession &session)
    {
        const physim::PendulumSimulation &simulation = session.getSimulation();

        stateBuffer[physicslab::pendulum::STATE_SIMULATION_STATE] =
            static_cast<float>(static_cast<int>(simulation.getState()));
        stateBuffer[physicslab::pendulum::STATE_MODE] = fromBoolean(simulation.isDoubleMode());
        stateBuffer[physicslab::pendulum::STATE_TIME] = simulation.getTimeGlobal();
        stateBuffer[physicslab::pendulum::STATE_TOTAL_ENERGY] = simulation.getCurrentTotalEnergy();
        stateBuffer[physicslab::pendulum::STATE_ENERGY_DELTA] = simulation.getEnergyDelta();
        stateBuffer[physicslab::pendulum::STATE_SMALL_ANGLE_PERIOD] = smallAnglePeriod(session);

        if (simulation.isSimpleMode())
        {
            const physim::SimplePendulum &simplePendulum = session.getSimplePendulum();
            const physim::PendulumBob bob = simplePendulum.getBob();

            stateBuffer[physicslab::pendulum::STATE_PRIMARY_ANGLE_DEGREES] = simplePendulum.getAngleDegrees();
            stateBuffer[physicslab::pendulum::STATE_PRIMARY_ANGULAR_VELOCITY] = simplePendulum.getAngularVelocity();
            stateBuffer[physicslab::pendulum::STATE_PRIMARY_SPEED] = bob.speed;
            stateBuffer[physicslab::pendulum::STATE_SECONDARY_ANGLE_DEGREES] = 0.0f;
            stateBuffer[physicslab::pendulum::STATE_SECONDARY_ANGULAR_VELOCITY] = 0.0f;
            stateBuffer[physicslab::pendulum::STATE_SECONDARY_SPEED] = 0.0f;
            return;
        }

        const physim::DoublePendulum &doublePendulum = session.getDoublePendulum();
        const physim::DoublePendulumBobs bobs = doublePendulum.getBobs();

        stateBuffer[physicslab::pendulum::STATE_PRIMARY_ANGLE_DEGREES] = doublePendulum.getAngle1Degrees();
        stateBuffer[physicslab::pendulum::STATE_PRIMARY_ANGULAR_VELOCITY] = doublePendulum.getAngularVelocity1();
        stateBuffer[physicslab::pendulum::STATE_PRIMARY_SPEED] = bobs.first.speed;
        stateBuffer[physicslab::pendulum::STATE_SECONDARY_ANGLE_DEGREES] = doublePendulum.getAngle2Degrees();
        stateBuffer[physicslab::pendulum::STATE_SECONDARY_ANGULAR_VELOCITY] = doublePendulum.getAngularVelocity2();
        stateBuffer[physicslab::pendulum::STATE_SECONDARY_SPEED] = bobs.second.speed;
    }

    void publishEnergyHistory(const physim::PendulumSimulation &simulation)
    {
        physicslab::bridge::publishSamples(
            energyBuffer,
            simulation.getEnergyHistory(),
            [](const physim::ScalarHistorySample &sample) { return sample.time; },
            [](const physim::ScalarHistorySample &sample) { return sample.value; });
    }

    void publishPhaseHistory(int trace, const std::vector<physim::PhaseSpaceSample> &history)
    {
        physicslab::bridge::publishSamples(
            phaseBuffers[static_cast<std::size_t>(trace)],
            history,
            [](const physim::PhaseSpaceSample &sample) { return sample.angle; },
            [](const physim::PhaseSpaceSample &sample) { return sample.angularVelocity; });
    }

    bool isValidPhaseTrace(int trace)
    {
        return trace >= 0 && trace < physicslab::pendulum::PHASE_TRACE_COUNT;
    }
} // namespace

namespace physicslab::pendulum
{
    void bindSession(physim::PendulumSession *session)
    {
        boundSession = session;
    }

    void publishFrameState()
    {
        if (boundSession == nullptr)
        {
            return;
        }

        const physim::PendulumSimulation &simulation = boundSession->getSimulation();

        publishStateSnapshot(*boundSession);
        publishEnergyHistory(simulation);
        publishPhaseHistory(PHASE_TRACE_PRIMARY, simulation.getPrimaryPhaseSpaceHistory());

        if (simulation.isDoubleMode())
        {
            publishPhaseHistory(PHASE_TRACE_SECONDARY, simulation.getSecondaryPhaseSpaceHistory());
            return;
        }

        phaseBuffers[PHASE_TRACE_SECONDARY].clear();
    }
} // namespace physicslab::pendulum

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
    if (boundSession == nullptr || parameter < 0 || parameter >= physicslab::pendulum::PARAMETER_COUNT)
    {
        return;
    }

    if (requiresIdleSimulation(parameter) && !boundSession->getSimulation().isIdle())
    {
        return;
    }

    writeParameter(*boundSession, parameter, value);
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabGetParameter(int parameter)
{
    if (boundSession == nullptr || parameter < 0 || parameter >= physicslab::pendulum::PARAMETER_COUNT)
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
    return physicslab::pendulum::STATE_SLOT_COUNT;
}

extern "C" EMSCRIPTEN_KEEPALIVE const float *physicsLabSeriesSamples()
{
    return energyBuffer.data();
}

extern "C" EMSCRIPTEN_KEEPALIVE int physicsLabSeriesSampleCount()
{
    return energyBuffer.count();
}

extern "C" EMSCRIPTEN_KEEPALIVE const float *physicsLabPhaseSamples(int trace)
{
    if (!isValidPhaseTrace(trace))
    {
        return nullptr;
    }

    return phaseBuffers[static_cast<std::size_t>(trace)].data();
}

extern "C" EMSCRIPTEN_KEEPALIVE int physicsLabPhaseSampleCount(int trace)
{
    if (!isValidPhaseTrace(trace))
    {
        return 0;
    }

    return phaseBuffers[static_cast<std::size_t>(trace)].count();
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabSetKeyboardCaptured(int captured)
{
    physicslab::keyboardCapturedByDocument() = captured != 0;
}
