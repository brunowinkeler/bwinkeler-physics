import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

import { describe, expect, it } from "vitest";

import {
    ballisticsControls,
    ballisticsParameterOrder,
    ballisticsRecordOrder,
    ballisticsStateOrder,
} from "../src/data/controls/ballistics";
import {
    gasControls,
    gasParameterOrder,
    gasStateOrder,
} from "../src/data/controls/gas-brownian";
import {
    pendulumControls,
    pendulumParameterOrder,
    pendulumStateOrder,
} from "../src/data/controls/pendulum";
import { publishedSimulations } from "../src/data/simulations";
import { simulationControls } from "../src/data/controls";
import type { SimulationControlsDefinition } from "../src/data/controls";

const repositoryRoot = resolve(import.meta.dirname, "..");

function readEnumerators(header: string, enumName: string) {
    const expression = new RegExp(
        `enum\\s+${enumName}\\s*\\{([\\s\\S]*?)\\}`,
        "m",
    );
    const body = header.match(expression)?.[1];
    if (!body) {
        throw new Error(
            `Enum '${enumName}' was not found in the bridge header.`,
        );
    }

    return body
        .split(",")
        .map((entry) => entry.trim().split(/\s|=/u)[0])
        .filter((entry): entry is string => Boolean(entry))
        .filter((entry) => !entry.startsWith("//"));
}

interface ControlTarget {
    id: string;
    header: string;
    definition: SimulationControlsDefinition;
    parameterOrder: readonly string[];
    stateOrder: readonly string[];
    recordOrder?: readonly string[];
}

const targets: ControlTarget[] = [
    {
        id: "ballistics",
        header: "web/ballistics/BallisticsBridge.h",
        definition: ballisticsControls,
        parameterOrder: ballisticsParameterOrder,
        stateOrder: ballisticsStateOrder,
        recordOrder: ballisticsRecordOrder,
    },
    {
        id: "pendulum",
        header: "web/pendulum/PendulumBridge.h",
        definition: pendulumControls,
        parameterOrder: pendulumParameterOrder,
        stateOrder: pendulumStateOrder,
    },
    {
        id: "gas-brownian",
        header: "web/gas-brownian/GasBridge.h",
        definition: gasControls,
        parameterOrder: gasParameterOrder,
        stateOrder: gasStateOrder,
    },
];

describe("simulation control panels", () => {
    it("covers every published simulation", () => {
        for (const simulation of publishedSimulations) {
            expect(simulationControls[simulation.id]).toBeDefined();
        }
    });

    for (const target of targets) {
        describe(target.id, () => {
            const header = readFile(
                resolve(repositoryRoot, target.header),
                "utf8",
            );

            it("keeps the parameter table aligned with the C++ enum", async () => {
                const enumerators = readEnumerators(await header, "Parameter");

                expect(enumerators.at(-1)).toBe("PARAMETER_COUNT");
                expect(enumerators.length - 1).toBe(
                    target.parameterOrder.length,
                );
            });

            it("keeps the state table aligned with the C++ enum", async () => {
                const enumerators = readEnumerators(await header, "StateSlot");

                expect(enumerators.at(-1)).toBe("STATE_SLOT_COUNT");
                expect(enumerators.length - 1).toBe(target.stateOrder.length);
            });

            it("keeps any record table aligned with the C++ enum", async () => {
                if (!target.recordOrder) return;

                const enumerators = readEnumerators(await header, "RecordSlot");

                expect(enumerators.at(-1)).toBe("RECORD_SLOT_COUNT");
                expect(enumerators.length - 1).toBe(target.recordOrder.length);
                expect(target.definition.records?.stride).toBe(
                    target.recordOrder.length,
                );
            });

            it("uses unique identifiers", () => {
                expect(new Set(target.parameterOrder).size).toBe(
                    target.parameterOrder.length,
                );
                expect(new Set(target.stateOrder).size).toBe(
                    target.stateOrder.length,
                );
            });

            it("references only declared parameters and state slots", () => {
                const parameters = new Set<string>(target.parameterOrder);
                const slots = new Set<string>(target.stateOrder);
                const { definition } = target;

                for (const group of definition.groups) {
                    for (const field of group.fields) {
                        expect(parameters.has(field.parameter)).toBe(true);
                        if (field.requiresParameter) {
                            expect(
                                parameters.has(field.requiresParameter),
                            ).toBe(true);
                        }
                    }
                }

                for (const readout of definition.readouts) {
                    expect(slots.has(readout.slot)).toBe(true);
                    if (readout.requiresSlot) {
                        expect(slots.has(readout.requiresSlot)).toBe(true);
                    }
                }

                for (const plot of definition.plots) {
                    if (plot.requiresParameter) {
                        expect(parameters.has(plot.requiresParameter)).toBe(
                            true,
                        );
                    }
                }

                if (definition.variantParameter) {
                    expect(parameters.has(definition.variantParameter)).toBe(
                        true,
                    );
                }
                if (definition.variantSlot) {
                    expect(slots.has(definition.variantSlot)).toBe(true);
                }
                expect(slots.has(definition.simulationStateSlot)).toBe(true);
            });

            it("exposes every editable parameter exactly once", () => {
                const rendered = target.definition.groups.flatMap((group) =>
                    group.fields.map((field) => field.parameter),
                );

                expect(new Set(rendered).size).toBe(rendered.length);
                expect(rendered.toSorted()).toEqual(
                    [...target.parameterOrder].toSorted(),
                );
            });

            it("declares usable ranges and options", () => {
                for (const group of target.definition.groups) {
                    for (const field of group.fields) {
                        if (field.kind === "number") {
                            expect(field.min).toBeTypeOf("number");
                            expect(field.max).toBeTypeOf("number");
                            expect(field.step).toBeGreaterThan(0);
                            expect(field.max).toBeGreaterThan(field.min ?? 0);
                        }

                        if (field.kind === "select") {
                            expect(field.options?.length ?? 0).toBeGreaterThan(
                                1,
                            );
                        }
                    }
                }
            });

            it("declares the lifecycle states the panel reacts to", () => {
                const { definition } = target;

                expect(definition.editableStates.length).toBeGreaterThan(0);
                expect(definition.runningStates.length).toBeGreaterThan(0);
                expect(definition.editHint.length).toBeGreaterThan(0);
                expect(definition.shortcuts.length).toBeGreaterThan(0);

                for (const state of definition.runningStates) {
                    expect(definition.editableStates).not.toContain(state);
                }
            });

            it("keeps record columns inside the declared stride", () => {
                const { records } = target.definition;
                if (!records) return;

                const columns = [...records.summary, ...records.details];
                for (const column of columns) {
                    expect(column.slot).toBeGreaterThanOrEqual(0);
                    expect(column.slot).toBeLessThan(records.stride);
                }

                expect(records.idSlot).toBeLessThan(records.stride);
                for (const slot of records.colorSlots ?? []) {
                    expect(slot).toBeLessThan(records.stride);
                }
            });
        });
    }
});
