import type {
    ControlField,
    PlotDescriptor,
    ReadoutField,
    RecordColumn,
    RecordListDescriptor,
    SimulationControlsDefinition,
} from "../data/controls";

import { drawPhasePlot, drawSeriesPlot } from "./analysis-plot";
import type { SimulationBridge } from "./simulation-bridge";

const SIMULATION_STATE_LABELS = [
    "Idle",
    "Running",
    "Paused",
    "Landed",
] as const;
const NO_RECORD = -1;

interface FieldBinding {
    field: ControlField;
    parameter: number;
    gate: number;
    input: HTMLInputElement | HTMLSelectElement;
    row: HTMLElement;
    lastValue: number;
}

interface ReadoutBinding {
    readout: ReadoutField;
    slot: number;
    gate: number;
    value: HTMLElement;
    row: HTMLElement;
}

interface PlotBinding {
    plot: PlotDescriptor;
    gate: number;
    canvas: HTMLCanvasElement;
    caption: HTMLElement;
    figure: HTMLElement;
}

export interface SimulationControlsHandle {
    dispose: () => void;
}

function simulationStateLabel(state: number) {
    return SIMULATION_STATE_LABELS[state] ?? "Unknown";
}

function formatValue(
    value: number,
    { precision = 3, format = "number", signed = false }: Partial<ReadoutField>,
) {
    if (!Number.isFinite(value)) return "—";

    if (format === "integer") return String(Math.round(value));

    const formatted =
        format === "exponential"
            ? value.toExponential(precision)
            : value.toFixed(precision);

    return signed && value >= 0 ? `+${formatted}` : formatted;
}

function formatRecordValue(value: number, column: RecordColumn) {
    if (!Number.isFinite(value)) return "—";

    if (column.format === "boolean") {
        const [whenFalse, whenTrue] = column.booleanLabels ?? ["no", "yes"];
        return value === 0 ? whenFalse : whenTrue;
    }

    if (column.format === "option") {
        const option = column.options?.find(
            (candidate) => candidate.value === Math.round(value),
        );
        return option?.label ?? String(Math.round(value));
    }

    if (column.format === "integer") return String(Math.round(value));

    const formatted = value.toFixed(column.precision ?? 3);
    return column.unit ? `${formatted} ${column.unit}` : formatted;
}

function writeInput(binding: FieldBinding, value: number) {
    const { field, input } = binding;
    binding.lastValue = value;

    if (field.kind === "checkbox" && input instanceof HTMLInputElement) {
        input.checked = value !== 0;
        return;
    }

    if (field.kind === "select") {
        input.value = String(Math.round(value));
        return;
    }

    input.value = value.toFixed(field.precision ?? 3);
}

function readInput(binding: FieldBinding) {
    const { field, input } = binding;

    if (field.kind === "checkbox" && input instanceof HTMLInputElement) {
        return input.checked ? 1 : 0;
    }

    const parsed = Number(input.value);
    return Number.isFinite(parsed) ? parsed : undefined;
}

export function attachSimulationControls(
    root: HTMLElement,
    bridge: SimulationBridge,
    definition: SimulationControlsDefinition,
): SimulationControlsHandle {
    const parameterIndex = new Map(
        definition.parameterOrder.map((id, index) => [id, index]),
    );
    const stateIndex = new Map(
        definition.stateOrder.map((id, index) => [id, index]),
    );

    const fieldBindings: FieldBinding[] = [];
    for (const group of definition.groups) {
        for (const field of group.fields) {
            const parameter = parameterIndex.get(field.parameter);
            const input = root.querySelector<
                HTMLInputElement | HTMLSelectElement
            >(`[data-control-parameter="${field.parameter}"]`);
            const row = input?.closest<HTMLElement>("[data-control-row]");
            if (parameter === undefined || !input || !row) continue;

            fieldBindings.push({
                field,
                parameter,
                gate:
                    field.requiresParameter === undefined
                        ? -1
                        : (parameterIndex.get(field.requiresParameter) ?? -1),
                input,
                row,
                lastValue: Number.NaN,
            });
        }
    }

    const readoutBindings: ReadoutBinding[] = [];
    definition.readouts.forEach((readout, index) => {
        const slot = stateIndex.get(readout.slot);
        const row = root.querySelector<HTMLElement>(
            `[data-readout-index="${index}"]`,
        );
        const value = row?.querySelector<HTMLElement>("[data-readout-value]");
        if (slot === undefined || !row || !value) return;

        readoutBindings.push({
            readout,
            slot,
            gate:
                readout.requiresSlot === undefined
                    ? -1
                    : (stateIndex.get(readout.requiresSlot) ?? -1),
            value,
            row,
        });
    });

    const plotBindings: PlotBinding[] = [];
    for (const plot of definition.plots) {
        const canvas = root.querySelector<HTMLCanvasElement>(
            `[data-plot-canvas="${plot.id}"]`,
        );
        const caption = root.querySelector<HTMLElement>(
            `[data-plot-caption="${plot.id}"]`,
        );
        const figure = canvas?.closest<HTMLElement>("[data-plot-figure]");
        if (!canvas || !caption || !figure) continue;

        plotBindings.push({
            plot,
            gate:
                plot.requiresParameter === undefined
                    ? -1
                    : (parameterIndex.get(plot.requiresParameter) ?? -1),
            canvas,
            caption,
            figure,
        });
    }

    const startButton = root.querySelector<HTMLButtonElement>(
        '[data-control-action="start"]',
    );
    const stopButton = root.querySelector<HTMLButtonElement>(
        '[data-control-action="stop"]',
    );
    const editHint = root.querySelector<HTMLElement>("[data-control-hint]");
    const recordList = root.querySelector<HTMLElement>("[data-record-list]");
    const variantSlot = stateIndex.get(definition.variantSlot ?? "") ?? -1;
    const simulationStateSlot =
        stateIndex.get(definition.simulationStateSlot) ?? -1;
    let lastRecordSignature: string | undefined;

    function hydrate() {
        for (const binding of fieldBindings) {
            writeInput(binding, bridge.getParameter(binding.parameter));
        }
    }

    function handleChange(event: Event) {
        const binding = fieldBindings.find(
            ({ input }) => input === event.target,
        );
        if (!binding) return;

        const value = readInput(binding);
        if (value !== undefined) {
            bridge.setParameter(binding.parameter, value);
        }

        // The bridge clamps to the physical domain and rejects edits the
        // simulation state does not allow, so the field mirrors what was kept.
        writeInput(binding, bridge.getParameter(binding.parameter));
    }

    function handleAction(event: Event) {
        const { target } = event;
        if (!(target instanceof Element)) return;

        const trigger = target.closest<HTMLElement>("[data-control-action]");
        const action = trigger?.dataset.controlAction;

        if (action === "start") bridge.start();
        if (action === "stop") bridge.stop();
        if (action === "reset") {
            bridge.reset();
            hydrate();
        }
        if (action === "clear-records") bridge.clearRecords();
        if (action === "remove-record") {
            bridge.removeRecord(Number(trigger?.dataset.recordId ?? NO_RECORD));
        }
        if (action === "select-record") {
            bridge.selectRecord(Number(trigger?.dataset.recordId ?? NO_RECORD));
        }
    }

    function handleFocusIn() {
        bridge.setKeyboardCaptured(true);
    }

    function handleFocusOut() {
        bridge.setKeyboardCaptured(false);
    }

    function handlePointerOver(event: Event) {
        const { target } = event;
        if (!(target instanceof Element)) return;

        const row = target.closest<HTMLElement>("[data-record-id]");
        bridge.hoverRecord(Number(row?.dataset.recordId ?? NO_RECORD));
    }

    function handlePointerLeave() {
        bridge.hoverRecord(NO_RECORD);
    }

    function updatePlots(variant: number) {
        for (const binding of plotBindings) {
            const enabled =
                binding.gate < 0 || bridge.getParameter(binding.gate) !== 0;
            binding.figure.hidden = !enabled;
            if (!enabled) continue;

            const summary =
                binding.plot.source === "series"
                    ? drawSeriesPlot(
                          binding.canvas,
                          bridge.readSeriesSamples(),
                          {
                              unit: binding.plot.unit,
                              exponential: binding.plot.exponential,
                          },
                      )
                    : drawPhasePlot(
                          binding.canvas,
                          variant === 1
                              ? [
                                    { samples: bridge.readPhaseSamples(0) },
                                    { samples: bridge.readPhaseSamples(1) },
                                ]
                              : [{ samples: bridge.readPhaseSamples(0) }],
                      );

            binding.caption.textContent = summary ?? binding.plot.emptyMessage;
        }
    }

    function updateFields(variant: number, editable: boolean) {
        for (const binding of fieldBindings) {
            const visible =
                binding.field.variant === undefined ||
                binding.field.variant === variant;
            binding.row.hidden = !visible;
            if (!visible) continue;

            const gateOpen =
                binding.gate < 0 || bridge.getParameter(binding.gate) !== 0;
            const disabled =
                (Boolean(binding.field.editableOnly) && !editable) || !gateOpen;
            binding.input.disabled = disabled;
            binding.row.classList.toggle("is-disabled", disabled);

            if (document.activeElement === binding.input) continue;

            const value = bridge.getParameter(binding.parameter);
            if (value !== binding.lastValue) writeInput(binding, value);
        }
    }

    function updateReadouts(state: Float32Array, variant: number) {
        for (const binding of readoutBindings) {
            const visible =
                (binding.readout.variant === undefined ||
                    binding.readout.variant === variant) &&
                (binding.gate < 0 || state[binding.gate] !== 0);
            binding.row.hidden = !visible;
            if (!visible) continue;

            const raw = state[binding.slot];
            binding.value.textContent =
                binding.readout.format === "state"
                    ? simulationStateLabel(Math.round(raw))
                    : formatValue(raw, binding.readout);
        }
    }

    function renderRecordColumns(
        record: Float32Array,
        columns: readonly RecordColumn[],
    ) {
        return columns
            .map(
                (column) =>
                    `<div><dt>${column.label}</dt><dd>${formatRecordValue(record[column.slot], column)}</dd></div>`,
            )
            .join("");
    }

    function renderRecord(
        record: Float32Array,
        descriptor: RecordListDescriptor,
    ) {
        const id = Math.round(record[descriptor.idSlot]);
        const summary = descriptor.summary
            .map(
                (column) =>
                    `${column.label} ${formatRecordValue(record[column.slot], column)}`,
            )
            .join(" · ");
        const swatch = descriptor.colorSlots
            ? `<span class="simulation-controls__swatch" style="background:rgb(${descriptor.colorSlots
                  .map((slot) => Math.round(record[slot]))
                  .join(" ")})" aria-hidden="true"></span>`
            : "";

        return `<details class="simulation-controls__record" data-record-id="${id}">
            <summary data-control-action="select-record" data-record-id="${id}">${swatch}<span>#${id} · ${summary}</span></summary>
            <dl class="simulation-controls__record-details">${renderRecordColumns(record, descriptor.details)}</dl>
            <button type="button" data-control-action="remove-record" data-record-id="${id}">${descriptor.removeLabel}</button>
        </details>`;
    }

    function updateRecords() {
        const descriptor = definition.records;
        if (!descriptor || !recordList) return;

        const records = bridge.readRecords(descriptor.stride);
        const count = records.length / descriptor.stride;
        const identifiers: string[] = [];
        for (let index = 0; index < count; index += 1) {
            identifiers.push(
                String(
                    Math.round(
                        records[index * descriptor.stride + descriptor.idSlot],
                    ),
                ),
            );
        }

        const signature = identifiers.join(",");
        if (signature === lastRecordSignature) return;
        lastRecordSignature = signature;

        if (count === 0) {
            recordList.innerHTML = `<p class="simulation-controls__help">${descriptor.emptyMessage}</p>`;
            return;
        }

        const markup: string[] = [];
        for (let index = 0; index < count; index += 1) {
            markup.push(
                renderRecord(
                    records.subarray(
                        index * descriptor.stride,
                        (index + 1) * descriptor.stride,
                    ),
                    descriptor,
                ),
            );
        }

        recordList.innerHTML = markup.join("");
    }

    let frameHandle = 0;

    function update() {
        frameHandle = requestAnimationFrame(update);

        const state = bridge.readState();
        if (state.length === 0) return;

        const variant = variantSlot >= 0 ? Math.round(state[variantSlot]) : 0;
        const simulationState =
            simulationStateSlot >= 0
                ? Math.round(state[simulationStateSlot])
                : 0;
        const editable = definition.editableStates.includes(simulationState);
        const running = definition.runningStates.includes(simulationState);

        updateFields(variant, editable);
        updateReadouts(state, variant);
        updatePlots(variant);
        updateRecords();

        if (startButton) {
            startButton.disabled = running;
            startButton.textContent = definition.resumeStates.includes(
                simulationState,
            )
                ? "Resume"
                : "Start";
        }
        if (stopButton) stopButton.disabled = !running;
        if (editHint) editHint.hidden = editable;
    }

    hydrate();
    root.addEventListener("change", handleChange);
    root.addEventListener("click", handleAction);
    root.addEventListener("focusin", handleFocusIn);
    root.addEventListener("focusout", handleFocusOut);
    recordList?.addEventListener("pointerover", handlePointerOver);
    recordList?.addEventListener("pointerleave", handlePointerLeave);
    frameHandle = requestAnimationFrame(update);

    return {
        dispose() {
            cancelAnimationFrame(frameHandle);
            root.removeEventListener("change", handleChange);
            root.removeEventListener("click", handleAction);
            root.removeEventListener("focusin", handleFocusIn);
            root.removeEventListener("focusout", handleFocusOut);
            recordList?.removeEventListener("pointerover", handlePointerOver);
            recordList?.removeEventListener("pointerleave", handlePointerLeave);
            bridge.setKeyboardCaptured(false);
        },
    };
}
