#include "BallisticsBridge.h"

#include "common/ControlBridge.h"
#include "common/WebRuntime.h"

#include "core/physics/IntegrationMethod.h"
#include "simulation/SimulationSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <emscripten/emscripten.h>
#include <optional>

namespace
{
    using physicslab::bridge::fromBoolean;
    using physicslab::bridge::sanitize;
    using physicslab::bridge::toBoolean;

    // Ranges mirrored from the native Dear ImGui menus, which are not compiled
    // into the browser bundle. The page repeats them as input affordances, but
    // this layer is the authority that keeps the physics in a valid domain.
    constexpr float MIN_INITIAL_SPEED = 0.0f;
    constexpr float MAX_INITIAL_SPEED = 500.0f;
    constexpr float MIN_LAUNCH_ANGLE = 0.0f;
    constexpr float MAX_LAUNCH_ANGLE = 89.9f;
    constexpr float MIN_PROJECTILE_MASS = 0.001f;
    constexpr float MAX_PROJECTILE_MASS = 100.0f;
    constexpr float MIN_PROJECTILE_RADIUS = 0.001f;
    constexpr float MAX_PROJECTILE_RADIUS = 1.0f;
    constexpr float MIN_DRAG_COEFFICIENT = 0.0f;
    constexpr float MAX_DRAG_COEFFICIENT = 3.0f;
    constexpr float MIN_GRAVITY = 0.0f;
    constexpr float MAX_GRAVITY = 50.0f;
    constexpr float MIN_AIR_DENSITY = 0.0f;
    constexpr float MAX_AIR_DENSITY = 5.0f;
    constexpr float MIN_TIME_SCALE = 0.1f;
    constexpr float MAX_TIME_SCALE = 20.0f;
    constexpr float MIN_PHYSICS_TIME_STEP = 0.0001f;
    constexpr float MAX_PHYSICS_TIME_STEP = 0.1f;

    // A visitor cannot meaningfully compare more archived launches than this, and
    // the cap keeps the exported buffer at a fixed size.
    constexpr std::size_t MAX_RECORDS = 64;
    constexpr std::size_t RECORD_STRIDE =
        static_cast<std::size_t>(physicslab::ballistics::RECORD_SLOT_COUNT);
    constexpr int NO_LAUNCH = -1;

    physim::SimulationSession *boundSession = nullptr;

    std::array<float, physicslab::ballistics::STATE_SLOT_COUNT> stateBuffer{};
    std::array<float, MAX_RECORDS * RECORD_STRIDE> recordBuffer{};
    int recordCount = 0;
    int selectedLaunchId = NO_LAUNCH;
    int hoveredLaunchId = NO_LAUNCH;

    // Drag only shapes the flight when air resistance is enabled, so the related
    // parameters stay locked otherwise, exactly as the native menus did.
    bool requiresAirResistance(int parameter)
    {
        switch (parameter)
        {
        case physicslab::ballistics::PARAMETER_MASS:
        case physicslab::ballistics::PARAMETER_DRAG_COEFFICIENT:
        case physicslab::ballistics::PARAMETER_AIR_DENSITY:
            return true;
        default:
            return false;
        }
    }

    bool requiresIdleSimulation(int parameter)
    {
        return parameter != physicslab::ballistics::PARAMETER_TIME_SCALE;
    }

    void writeParameter(physim::SimulationSession &session, int parameter, float value)
    {
        physim::Simulation &simulation = session.getSimulation();
        physim::Environment &environment = session.getEnvironment();
        physim::Projectile &projectile = session.getProjectile();
        float sanitized = 0.0f;

        switch (parameter)
        {
        case physicslab::ballistics::PARAMETER_INITIAL_SPEED:
            if (sanitize(value, MIN_INITIAL_SPEED, MAX_INITIAL_SPEED, sanitized))
            {
                projectile.getInitialSpeed() = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_LAUNCH_ANGLE:
            if (sanitize(value, MIN_LAUNCH_ANGLE, MAX_LAUNCH_ANGLE, sanitized))
            {
                projectile.getLaunchAngle() = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_RADIUS:
            if (sanitize(value, MIN_PROJECTILE_RADIUS, MAX_PROJECTILE_RADIUS, sanitized))
            {
                projectile.getRadius() = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_AIR_RESISTANCE:
            environment.airResistanceEnabled = toBoolean(value);
            break;
        case physicslab::ballistics::PARAMETER_MASS:
            if (sanitize(value, MIN_PROJECTILE_MASS, MAX_PROJECTILE_MASS, sanitized))
            {
                projectile.getMass() = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_DRAG_COEFFICIENT:
            if (sanitize(value, MIN_DRAG_COEFFICIENT, MAX_DRAG_COEFFICIENT, sanitized))
            {
                projectile.getDragCoefficient() = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_AIR_DENSITY:
            if (sanitize(value, MIN_AIR_DENSITY, MAX_AIR_DENSITY, sanitized))
            {
                environment.airDensity = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_GRAVITY:
            if (sanitize(value, MIN_GRAVITY, MAX_GRAVITY, sanitized))
            {
                environment.gravity = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_TIME_SCALE:
            if (sanitize(value, MIN_TIME_SCALE, MAX_TIME_SCALE, sanitized))
            {
                environment.timeScale = sanitized;
            }
            break;
        case physicslab::ballistics::PARAMETER_INTEGRATION_METHOD:
        {
            const auto method = static_cast<physim::IntegrationMethod>(static_cast<int>(value));
            if (physim::isSupportedIntegrationMethod(method))
            {
                projectile.setIntegrationMethod(method);
            }
            break;
        }
        case physicslab::ballistics::PARAMETER_PHYSICS_TIME_STEP:
            if (sanitize(value, MIN_PHYSICS_TIME_STEP, MAX_PHYSICS_TIME_STEP, sanitized))
            {
                simulation.setPhysicsTimeStep(sanitized);
            }
            break;
        default:
            break;
        }
    }

    float readParameter(physim::SimulationSession &session, int parameter)
    {
        const physim::Simulation &simulation = session.getSimulation();
        const physim::Environment &environment = session.getEnvironment();
        physim::Projectile &projectile = session.getProjectile();

        switch (parameter)
        {
        case physicslab::ballistics::PARAMETER_INITIAL_SPEED:
            return projectile.getInitialSpeed();
        case physicslab::ballistics::PARAMETER_LAUNCH_ANGLE:
            return projectile.getLaunchAngle();
        case physicslab::ballistics::PARAMETER_RADIUS:
            return projectile.getRadius();
        case physicslab::ballistics::PARAMETER_AIR_RESISTANCE:
            return fromBoolean(environment.airResistanceEnabled);
        case physicslab::ballistics::PARAMETER_MASS:
            return projectile.getMass();
        case physicslab::ballistics::PARAMETER_DRAG_COEFFICIENT:
            return projectile.getDragCoefficient();
        case physicslab::ballistics::PARAMETER_AIR_DENSITY:
            return environment.airDensity;
        case physicslab::ballistics::PARAMETER_GRAVITY:
            return environment.gravity;
        case physicslab::ballistics::PARAMETER_TIME_SCALE:
            return environment.timeScale;
        case physicslab::ballistics::PARAMETER_INTEGRATION_METHOD:
            return static_cast<float>(static_cast<int>(projectile.getIntegrationMethod()));
        case physicslab::ballistics::PARAMETER_PHYSICS_TIME_STEP:
            return simulation.getPhysicsTimeStep();
        default:
            return 0.0f;
        }
    }

    void publishStateSnapshot(physim::SimulationSession &session)
    {
        const physim::Simulation &simulation = session.getSimulation();
        const physim::Projectile &projectile = session.getProjectile();
        const physim::Position position = projectile.getPosition();
        const std::optional<physim::TrajectoryPoint> apexPoint = simulation.getApexPoint();

        stateBuffer[physicslab::ballistics::STATE_SIMULATION_STATE] =
            static_cast<float>(static_cast<int>(simulation.getState()));
        stateBuffer[physicslab::ballistics::STATE_AIR_RESISTANCE] =
            fromBoolean(session.getEnvironment().airResistanceEnabled);
        stateBuffer[physicslab::ballistics::STATE_TIME] = simulation.getTimeGlobal();
        stateBuffer[physicslab::ballistics::STATE_RANGE] = position.x;
        stateBuffer[physicslab::ballistics::STATE_HEIGHT] = position.y;
        stateBuffer[physicslab::ballistics::STATE_SPEED] = projectile.getCurrentSpeed();
        stateBuffer[physicslab::ballistics::STATE_HAS_APEX] = fromBoolean(apexPoint.has_value());
        stateBuffer[physicslab::ballistics::STATE_APEX_HEIGHT] = apexPoint.has_value() ? apexPoint->y : 0.0f;
        stateBuffer[physicslab::ballistics::STATE_APEX_TIME] = apexPoint.has_value() ? apexPoint->time : 0.0f;
    }

    void publishLaunchRecord(std::size_t index, const physim::LaunchHistoryEntry &entry)
    {
        const std::optional<physim::TrajectoryPoint> apexPoint = entry.trajectory.getApexPoint();
        float *record = recordBuffer.data() + (index * RECORD_STRIDE);

        record[physicslab::ballistics::RECORD_ID] = static_cast<float>(entry.id);
        record[physicslab::ballistics::RECORD_LANDED] = fromBoolean(entry.landed);
        record[physicslab::ballistics::RECORD_FINAL_RANGE] = entry.finalRange;
        record[physicslab::ballistics::RECORD_FLIGHT_TIME] = entry.flightTime;
        record[physicslab::ballistics::RECORD_INITIAL_SPEED] = entry.configuration.projectile.initialSpeed;
        record[physicslab::ballistics::RECORD_LAUNCH_ANGLE] = entry.configuration.projectile.launchAngle;
        record[physicslab::ballistics::RECORD_MASS] = entry.configuration.projectile.mass;
        record[physicslab::ballistics::RECORD_RADIUS] = entry.configuration.projectile.radius;
        record[physicslab::ballistics::RECORD_DRAG_COEFFICIENT] = entry.configuration.projectile.dragCoefficient;
        record[physicslab::ballistics::RECORD_GRAVITY] = entry.configuration.environment.gravity;
        record[physicslab::ballistics::RECORD_AIR_RESISTANCE] =
            fromBoolean(entry.configuration.environment.airResistanceEnabled);
        record[physicslab::ballistics::RECORD_AIR_DENSITY] = entry.configuration.environment.airDensity;
        record[physicslab::ballistics::RECORD_INTEGRATION_METHOD] =
            static_cast<float>(static_cast<int>(entry.configuration.simulation.integrationMethod));
        record[physicslab::ballistics::RECORD_PHYSICS_TIME_STEP] = entry.configuration.simulation.physicsTimeStep;
        record[physicslab::ballistics::RECORD_HAS_APEX] = fromBoolean(apexPoint.has_value());
        record[physicslab::ballistics::RECORD_APEX_HEIGHT] = apexPoint.has_value() ? apexPoint->y : 0.0f;
        record[physicslab::ballistics::RECORD_APEX_TIME] = apexPoint.has_value() ? apexPoint->time : 0.0f;
        record[physicslab::ballistics::RECORD_COLOR_RED] = static_cast<float>(entry.style.r);
        record[physicslab::ballistics::RECORD_COLOR_GREEN] = static_cast<float>(entry.style.g);
        record[physicslab::ballistics::RECORD_COLOR_BLUE] = static_cast<float>(entry.style.b);
    }

    void publishLaunchHistory(const physim::Simulation &simulation)
    {
        const std::vector<physim::LaunchHistoryEntry> &history = simulation.getLaunchHistory();
        const std::size_t publishedCount = std::min(history.size(), MAX_RECORDS);

        for (std::size_t index = 0; index < publishedCount; ++index)
        {
            publishLaunchRecord(index, history[index]);
        }

        recordCount = static_cast<int>(publishedCount);
    }
} // namespace

namespace physicslab::ballistics
{
    void bindSession(physim::SimulationSession *session)
    {
        boundSession = session;
        selectedLaunchId = NO_LAUNCH;
        hoveredLaunchId = NO_LAUNCH;
    }

    HighlightedLaunches highlightedLaunches()
    {
        return {selectedLaunchId, hoveredLaunchId};
    }

    void publishFrameState()
    {
        if (boundSession == nullptr)
        {
            return;
        }

        publishStateSnapshot(*boundSession);
        publishLaunchHistory(boundSession->getSimulation());
    }
} // namespace physicslab::ballistics

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
    if (boundSession == nullptr || parameter < 0 || parameter >= physicslab::ballistics::PARAMETER_COUNT)
    {
        return;
    }

    if (requiresIdleSimulation(parameter) && !boundSession->getSimulation().isIdle())
    {
        return;
    }

    if (requiresAirResistance(parameter) && !boundSession->getEnvironment().airResistanceEnabled)
    {
        return;
    }

    writeParameter(*boundSession, parameter, value);
}

extern "C" EMSCRIPTEN_KEEPALIVE float physicsLabGetParameter(int parameter)
{
    if (boundSession == nullptr || parameter < 0 || parameter >= physicslab::ballistics::PARAMETER_COUNT)
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
    return physicslab::ballistics::STATE_SLOT_COUNT;
}

extern "C" EMSCRIPTEN_KEEPALIVE const float *physicsLabRecords()
{
    return recordBuffer.data();
}

extern "C" EMSCRIPTEN_KEEPALIVE int physicsLabRecordCount()
{
    return recordCount;
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabClearRecords()
{
    if (boundSession == nullptr)
    {
        return;
    }

    boundSession->getSimulation().clearLaunchHistory();
    selectedLaunchId = NO_LAUNCH;
    hoveredLaunchId = NO_LAUNCH;
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabRemoveRecord(int recordId)
{
    if (boundSession == nullptr)
    {
        return;
    }

    boundSession->getSimulation().removeLaunchHistoryEntry(recordId);

    if (selectedLaunchId == recordId)
    {
        selectedLaunchId = NO_LAUNCH;
    }

    if (hoveredLaunchId == recordId)
    {
        hoveredLaunchId = NO_LAUNCH;
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabSelectRecord(int recordId)
{
    selectedLaunchId = recordId;
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabHoverRecord(int recordId)
{
    hoveredLaunchId = recordId;
}

extern "C" EMSCRIPTEN_KEEPALIVE void physicsLabSetKeyboardCaptured(int captured)
{
    physicslab::keyboardCapturedByDocument() = captured != 0;
}
