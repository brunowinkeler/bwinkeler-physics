#ifndef PHYSICS_LAB_GAS_BRIDGE_H
#define PHYSICS_LAB_GAS_BRIDGE_H

namespace physim
{
    class GasSession;
}

// Bridge between the gas session and the DOM control panel. The browser owns the
// controls, so this layer replaces the native Dear ImGui menus: it validates every
// parameter write, publishes a packed state snapshot, and downsamples the pressure
// history into a buffer the page reads from the Wasm heap.
namespace physicslab::gas
{
    enum Parameter
    {
        PARAMETER_MODE = 0,
        PARAMETER_TIME_SCALE,
        PARAMETER_PARTICLE_COUNT,
        PARAMETER_GAS_PRESET,
        PARAMETER_BOX_WIDTH,
        PARAMETER_BOX_HEIGHT,
        PARAMETER_PHYSICS_TIME_STEP,
        PARAMETER_THERMAL_LEVEL,
        PARAMETER_RANDOM_SEED,
        PARAMETER_SHOW_GRID,
        PARAMETER_SHOW_PRESSURE_PLOT,
        PARAMETER_TRACER_TRAIL,
        PARAMETER_TRACER_MASS_SCALE,
        PARAMETER_TRACER_RADIUS_SCALE,
        PARAMETER_COUNT
    };

    enum StateSlot
    {
        STATE_SIMULATION_STATE = 0,
        STATE_MODE,
        STATE_TIME,
        STATE_PRESSURE,
        STATE_TEMPERATURE,
        STATE_AVERAGE_KINETIC_ENERGY,
        STATE_AVERAGE_SPEED,
        STATE_ACTIVE_PARTICLES,
        STATE_SLOT_COUNT
    };

    // Binds the session the exported functions operate on. The adapter owns the
    // session lifetime and calls this once during construction.
    void bindSession(physim::GasSession *session);

    // Refreshes the state snapshot and the pressure buffer. The adapter calls this
    // once per rendered frame, after the simulation update.
    void publishFrameState();
} // namespace physicslab::gas

#endif // PHYSICS_LAB_GAS_BRIDGE_H
