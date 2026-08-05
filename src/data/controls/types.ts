// Declarative description of a simulation control panel. The page renders the
// panel and the runtime wiring from these tables, and the numeric parameter and
// state indexes are the contract shared with the C++ bridge of each simulation.

export type ControlKind = "number" | "select" | "checkbox";

export interface ControlOption {
    label: string;
    value: number;
}

export interface ControlField {
    parameter: string;
    label: string;
    kind: ControlKind;
    unit?: string;
    min?: number;
    max?: number;
    step?: number;
    precision?: number;
    options?: readonly ControlOption[];
    /** Rendered only while the variant parameter holds this value. */
    variant?: number;
    /** Editable only while the simulation is in one of `editableStates`. */
    editableOnly?: boolean;
    /** Editable only while this other parameter is enabled. */
    requiresParameter?: string;
    help?: string;
}

export interface ControlGroup {
    id: string;
    title: string;
    fields: readonly ControlField[];
}

export interface ReadoutField {
    slot: string;
    label: string;
    unit?: string;
    precision?: number;
    /** Renders the raw value as the simulation state label instead of a number. */
    format?: "number" | "integer" | "exponential" | "state";
    signed?: boolean;
    variant?: number;
    /** Rendered only while this state slot is non-zero. */
    requiresSlot?: string;
}

export interface PlotDescriptor {
    id: string;
    title: string;
    source: "series" | "phase";
    caption: string;
    emptyMessage: string;
    unit?: string;
    exponential?: boolean;
    /** Rendered only while this parameter is enabled. */
    requiresParameter?: string;
}

export interface RecordColumn {
    label: string;
    slot: number;
    unit?: string;
    precision?: number;
    format?: "number" | "integer" | "boolean" | "option";
    options?: readonly ControlOption[];
    booleanLabels?: readonly [string, string];
}

/** A dynamic list of packed records the bridge republishes every frame. */
export interface RecordListDescriptor {
    id: string;
    title: string;
    emptyMessage: string;
    stride: number;
    idSlot: number;
    colorSlots?: readonly [number, number, number];
    summary: readonly RecordColumn[];
    details: readonly RecordColumn[];
    clearLabel: string;
    removeLabel: string;
}

export interface SimulationControlsDefinition {
    /** Parameter identifiers in the exact order of the C++ parameter enum. */
    parameterOrder: readonly string[];
    /** State identifiers in the exact order of the C++ state slot enum. */
    stateOrder: readonly string[];
    /** Simulation state values in which `editableOnly` fields accept edits. */
    editableStates: readonly number[];
    /** Simulation state values that count as running. */
    runningStates: readonly number[];
    /** Simulation state values where the start button offers to resume. */
    resumeStates: readonly number[];
    /** Parameter whose value selects which variant-specific rows are visible. */
    variantParameter?: string;
    /** State slot mirroring the variant parameter, used by the update loop. */
    variantSlot?: string;
    /** State slot holding the simulation state, used to gate editable fields. */
    simulationStateSlot: string;
    /** Shown while the simulation state locks the configuration. */
    editHint: string;
    groups: readonly ControlGroup[];
    readouts: readonly ReadoutField[];
    plots: readonly PlotDescriptor[];
    records?: RecordListDescriptor;
    shortcuts: readonly string[];
}
