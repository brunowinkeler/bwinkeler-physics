import { ballisticsControls } from "./ballistics";
import { gasControls } from "./gas-brownian";
import { pendulumControls } from "./pendulum";
import type { SimulationControlsDefinition } from "./types";

// Every published simulation renders its controls in the DOM instead of drawing
// a Dear ImGui panel over the canvas.
export const simulationControls: Readonly<
    Record<string, SimulationControlsDefinition>
> = {
    ballistics: ballisticsControls,
    pendulum: pendulumControls,
    "gas-brownian": gasControls,
};

export function getSimulationControls(simulationId: string) {
    return simulationControls[simulationId];
}

export type {
    ControlField,
    ControlGroup,
    PlotDescriptor,
    ReadoutField,
    RecordColumn,
    RecordListDescriptor,
    SimulationControlsDefinition,
} from "./types";
