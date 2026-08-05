#ifndef PHYSICS_LAB_PENDULUM_BRIDGE_H
#define PHYSICS_LAB_PENDULUM_BRIDGE_H

namespace physim
{
    class PendulumSession;
}

// Bridge between the pendulum session and the DOM control panel. The browser owns
// the controls, so this layer replaces the native Dear ImGui menus: it validates
// every parameter write, publishes a packed state snapshot, and downsamples the
// analysis histories into buffers the page reads directly from the Wasm heap.
namespace physicslab::pendulum
{
    enum Parameter
    {
        PARAMETER_MODE = 0,
        PARAMETER_GRAVITY,
        PARAMETER_TIME_SCALE,
        PARAMETER_PHYSICS_TIME_STEP,
        PARAMETER_INTEGRATION_METHOD,
        PARAMETER_SHOW_GRID,
        PARAMETER_SHOW_AXES,
        PARAMETER_SHOW_PRIMARY_TRAIL,
        PARAMETER_SHOW_SECONDARY_TRAIL,
        PARAMETER_SIMPLE_LENGTH,
        PARAMETER_SIMPLE_MASS,
        PARAMETER_SIMPLE_DAMPING,
        PARAMETER_SIMPLE_INITIAL_ANGLE,
        PARAMETER_SIMPLE_INITIAL_ANGULAR_VELOCITY,
        PARAMETER_DOUBLE_LENGTH_1,
        PARAMETER_DOUBLE_LENGTH_2,
        PARAMETER_DOUBLE_MASS_1,
        PARAMETER_DOUBLE_MASS_2,
        PARAMETER_DOUBLE_DAMPING_1,
        PARAMETER_DOUBLE_DAMPING_2,
        PARAMETER_DOUBLE_INITIAL_ANGLE_1,
        PARAMETER_DOUBLE_INITIAL_ANGLE_2,
        PARAMETER_DOUBLE_INITIAL_ANGULAR_VELOCITY_1,
        PARAMETER_DOUBLE_INITIAL_ANGULAR_VELOCITY_2,
        PARAMETER_COUNT
    };

    enum StateSlot
    {
        STATE_SIMULATION_STATE = 0,
        STATE_MODE,
        STATE_TIME,
        STATE_TOTAL_ENERGY,
        STATE_ENERGY_DELTA,
        STATE_PRIMARY_ANGLE_DEGREES,
        STATE_PRIMARY_ANGULAR_VELOCITY,
        STATE_PRIMARY_SPEED,
        STATE_SECONDARY_ANGLE_DEGREES,
        STATE_SECONDARY_ANGULAR_VELOCITY,
        STATE_SECONDARY_SPEED,
        STATE_SMALL_ANGLE_PERIOD,
        STATE_SLOT_COUNT
    };

    enum PhaseTrace
    {
        PHASE_TRACE_PRIMARY = 0,
        PHASE_TRACE_SECONDARY,
        PHASE_TRACE_COUNT
    };

    // Binds the session the exported functions operate on. The adapter owns the
    // session lifetime and calls this once during construction.
    void bindSession(physim::PendulumSession *session);

    // Refreshes the state snapshot and the analysis buffers. The adapter calls
    // this once per rendered frame, after the simulation update.
    void publishFrameState();
} // namespace physicslab::pendulum

#endif // PHYSICS_LAB_PENDULUM_BRIDGE_H
