#ifndef PHYSICS_LAB_BALLISTICS_BRIDGE_H
#define PHYSICS_LAB_BALLISTICS_BRIDGE_H

namespace physim
{
    class SimulationSession;
}

// Bridge between the ballistics session and the DOM control panel. The browser
// owns the controls, so this layer replaces the native Dear ImGui menus: it
// validates every parameter write, publishes a packed state snapshot, and flattens
// the launch history into records the page reads from the Wasm heap.
namespace physicslab::ballistics
{
    enum Parameter
    {
        PARAMETER_INITIAL_SPEED = 0,
        PARAMETER_LAUNCH_ANGLE,
        PARAMETER_RADIUS,
        PARAMETER_AIR_RESISTANCE,
        PARAMETER_MASS,
        PARAMETER_DRAG_COEFFICIENT,
        PARAMETER_AIR_DENSITY,
        PARAMETER_GRAVITY,
        PARAMETER_TIME_SCALE,
        PARAMETER_INTEGRATION_METHOD,
        PARAMETER_PHYSICS_TIME_STEP,
        PARAMETER_COUNT
    };

    enum StateSlot
    {
        STATE_SIMULATION_STATE = 0,
        STATE_AIR_RESISTANCE,
        STATE_TIME,
        STATE_RANGE,
        STATE_SPEED,
        STATE_HEIGHT,
        STATE_HAS_APEX,
        STATE_APEX_HEIGHT,
        STATE_APEX_TIME,
        STATE_SLOT_COUNT
    };

    // Layout of one archived launch inside the record buffer.
    enum RecordSlot
    {
        RECORD_ID = 0,
        RECORD_LANDED,
        RECORD_FINAL_RANGE,
        RECORD_FLIGHT_TIME,
        RECORD_INITIAL_SPEED,
        RECORD_LAUNCH_ANGLE,
        RECORD_MASS,
        RECORD_RADIUS,
        RECORD_DRAG_COEFFICIENT,
        RECORD_GRAVITY,
        RECORD_AIR_RESISTANCE,
        RECORD_AIR_DENSITY,
        RECORD_INTEGRATION_METHOD,
        RECORD_PHYSICS_TIME_STEP,
        RECORD_HAS_APEX,
        RECORD_APEX_HEIGHT,
        RECORD_APEX_TIME,
        RECORD_COLOR_RED,
        RECORD_COLOR_GREEN,
        RECORD_COLOR_BLUE,
        RECORD_SLOT_COUNT
    };

    struct HighlightedLaunches
    {
        int selectedId;
        int hoveredId;
    };

    // Binds the session the exported functions operate on. The adapter owns the
    // session lifetime and calls this once during construction.
    void bindSession(physim::SimulationSession *session);

    // Launch rows the page selected or is pointing at, applied by the adapter to
    // the scene renderer every frame.
    HighlightedLaunches highlightedLaunches();

    // Refreshes the state snapshot and the launch records. The adapter calls this
    // once per rendered frame, after the simulation update.
    void publishFrameState();
} // namespace physicslab::ballistics

#endif // PHYSICS_LAB_BALLISTICS_BRIDGE_H
