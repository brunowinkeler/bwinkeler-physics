// Typed access to the exported WebAssembly surface of a simulation runtime.
// `ALLOW_MEMORY_GROWTH` can detach the heap views at any allocation, so every
// read resolves `HEAPF32` again instead of caching a view.

export interface PhysicsLabModule {
    HEAPF32?: Float32Array;
    _physicsLabStart?: () => void;
    _physicsLabStop?: () => void;
    _physicsLabReset?: () => void;
    _physicsLabSetParameter?: (parameter: number, value: number) => void;
    _physicsLabGetParameter?: (parameter: number) => number;
    _physicsLabStateBuffer?: () => number;
    _physicsLabStateLength?: () => number;
    _physicsLabSeriesSamples?: () => number;
    _physicsLabSeriesSampleCount?: () => number;
    _physicsLabPhaseSamples?: (trace: number) => number;
    _physicsLabPhaseSampleCount?: (trace: number) => number;
    _physicsLabRecords?: () => number;
    _physicsLabRecordCount?: () => number;
    _physicsLabClearRecords?: () => void;
    _physicsLabRemoveRecord?: (recordId: number) => void;
    _physicsLabSelectRecord?: (recordId: number) => void;
    _physicsLabHoverRecord?: (recordId: number) => void;
    _physicsLabSetKeyboardCaptured?: (captured: number) => void;
}

const EMPTY_SAMPLES = new Float32Array(0);
const FLOAT_SIZE = Float32Array.BYTES_PER_ELEMENT;

function readFloats(
    physicsLab: PhysicsLabModule,
    pointer: number,
    length: number,
) {
    const heap = physicsLab.HEAPF32;
    if (!heap || pointer <= 0 || length <= 0) return EMPTY_SAMPLES;

    const start = pointer / FLOAT_SIZE;
    if (!Number.isInteger(start) || start + length > heap.length) {
        return EMPTY_SAMPLES;
    }

    return heap.subarray(start, start + length);
}

export interface SimulationBridge {
    start: () => void;
    stop: () => void;
    reset: () => void;
    getParameter: (parameter: number) => number;
    setParameter: (parameter: number, value: number) => void;
    setKeyboardCaptured: (captured: boolean) => void;
    readState: () => Float32Array;
    readSeriesSamples: () => Float32Array;
    readPhaseSamples: (trace: number) => Float32Array;
    readRecords: (stride: number) => Float32Array;
    clearRecords: () => void;
    removeRecord: (recordId: number) => void;
    selectRecord: (recordId: number) => void;
    hoverRecord: (recordId: number) => void;
}

export function createSimulationBridge(
    physicsLab: PhysicsLabModule,
): SimulationBridge {
    return {
        start: () => physicsLab._physicsLabStart?.(),
        stop: () => physicsLab._physicsLabStop?.(),
        reset: () => physicsLab._physicsLabReset?.(),
        getParameter: (parameter) =>
            physicsLab._physicsLabGetParameter?.(parameter) ?? 0,
        setParameter: (parameter, value) =>
            physicsLab._physicsLabSetParameter?.(parameter, value),
        setKeyboardCaptured: (captured) =>
            physicsLab._physicsLabSetKeyboardCaptured?.(captured ? 1 : 0),
        readState: () =>
            readFloats(
                physicsLab,
                physicsLab._physicsLabStateBuffer?.() ?? 0,
                physicsLab._physicsLabStateLength?.() ?? 0,
            ),
        readSeriesSamples: () =>
            readFloats(
                physicsLab,
                physicsLab._physicsLabSeriesSamples?.() ?? 0,
                (physicsLab._physicsLabSeriesSampleCount?.() ?? 0) * 2,
            ),
        readPhaseSamples: (trace) =>
            readFloats(
                physicsLab,
                physicsLab._physicsLabPhaseSamples?.(trace) ?? 0,
                (physicsLab._physicsLabPhaseSampleCount?.(trace) ?? 0) * 2,
            ),
        readRecords: (stride) =>
            readFloats(
                physicsLab,
                physicsLab._physicsLabRecords?.() ?? 0,
                (physicsLab._physicsLabRecordCount?.() ?? 0) * stride,
            ),
        clearRecords: () => physicsLab._physicsLabClearRecords?.(),
        removeRecord: (recordId) =>
            physicsLab._physicsLabRemoveRecord?.(recordId),
        selectRecord: (recordId) =>
            physicsLab._physicsLabSelectRecord?.(recordId),
        hoverRecord: (recordId) =>
            physicsLab._physicsLabHoverRecord?.(recordId),
    };
}

export function supportsDomControls(physicsLab: PhysicsLabModule) {
    return (
        typeof physicsLab._physicsLabGetParameter === "function" &&
        typeof physicsLab._physicsLabStateBuffer === "function"
    );
}
