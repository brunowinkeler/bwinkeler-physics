// Canvas 2D replacements for the analysis plots the native Dear ImGui panel used
// to draw over the simulation. Samples arrive as interleaved pairs read straight
// from the WebAssembly heap.

const PLOT_PADDING = 8;
const GRID_SUBDIVISIONS = 4;
const BACKGROUND_COLOR = "#12121c";
const BORDER_COLOR = "#4a4a5e";
const GRID_COLOR = "rgb(80 82 95 / 70%)";
const AXIS_COLOR = "rgb(210 214 230 / 86%)";
const SERIES_COLORS = ["#89dceb", "#ffb387"] as const;
// Relative, because physical magnitudes range from about 1e-24 for pressure to
// hundreds of joules for energy.
const FLAT_SERIES_RATIO = 1e-9;

export interface PlotTrace {
    samples: Float32Array;
}

interface Bounds {
    minimum: number;
    maximum: number;
}

interface PlotSurface {
    context: CanvasRenderingContext2D;
    width: number;
    height: number;
}

function preparePlotSurface(
    canvas: HTMLCanvasElement,
): PlotSurface | undefined {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    if (width < 1 || height < 1) return undefined;

    const context = canvas.getContext("2d");
    if (!context) return undefined;

    const pixelRatio = Math.min(window.devicePixelRatio || 1, 2);
    const drawingWidth = Math.round(width * pixelRatio);
    const drawingHeight = Math.round(height * pixelRatio);

    if (canvas.width !== drawingWidth || canvas.height !== drawingHeight) {
        canvas.width = drawingWidth;
        canvas.height = drawingHeight;
    }

    context.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0);
    context.clearRect(0, 0, width, height);

    return { context, width, height };
}

function drawFrame({ context, width, height }: PlotSurface) {
    context.fillStyle = BACKGROUND_COLOR;
    context.fillRect(0, 0, width, height);

    context.strokeStyle = GRID_COLOR;
    context.lineWidth = 1;
    context.beginPath();
    for (
        let subdivision = 1;
        subdivision < GRID_SUBDIVISIONS;
        subdivision += 1
    ) {
        const fraction = subdivision / GRID_SUBDIVISIONS;
        const x = Math.round(width * fraction) + 0.5;
        const y = Math.round(height * fraction) + 0.5;
        context.moveTo(x, 0);
        context.lineTo(x, height);
        context.moveTo(0, y);
        context.lineTo(width, y);
    }
    context.stroke();

    context.strokeStyle = BORDER_COLOR;
    context.strokeRect(0.5, 0.5, width - 1, height - 1);
}

function padBounds({ minimum, maximum }: Bounds, padding: number): Bounds {
    const span = maximum - minimum;
    const scale = Math.max(Math.abs(minimum), Math.abs(maximum));

    // A constant series still needs a visible band around the value.
    if (span <= scale * FLAT_SERIES_RATIO) {
        const margin = scale > 0 ? scale * 0.5 : 1;
        return { minimum: minimum - margin, maximum: maximum + margin };
    }

    const margin = padding * span;
    return { minimum: minimum - margin, maximum: maximum + margin };
}

function collectBounds(traces: readonly PlotTrace[], offset: number) {
    let minimum = Number.POSITIVE_INFINITY;
    let maximum = Number.NEGATIVE_INFINITY;

    for (const { samples } of traces) {
        for (let index = offset; index < samples.length; index += 2) {
            const value = samples[index];
            if (!Number.isFinite(value)) continue;
            minimum = Math.min(minimum, value);
            maximum = Math.max(maximum, value);
        }
    }

    if (!Number.isFinite(minimum) || !Number.isFinite(maximum)) {
        return undefined;
    }

    return { minimum, maximum };
}

function projector(bounds: Bounds, length: number, invert: boolean) {
    const span = bounds.maximum - bounds.minimum;
    const usableSpan = span > 0 ? span : 1;
    const usable = length - PLOT_PADDING * 2;

    return (value: number) => {
        const normalized = (value - bounds.minimum) / usableSpan;
        return invert
            ? PLOT_PADDING + usable * (1 - normalized)
            : PLOT_PADDING + usable * normalized;
    };
}

function strokeTrace(
    context: CanvasRenderingContext2D,
    samples: Float32Array,
    color: string,
    toX: (value: number) => number,
    toY: (value: number) => number,
) {
    context.strokeStyle = color;
    context.lineWidth = 1.5;
    context.lineJoin = "round";
    context.beginPath();

    let hasPreviousPoint = false;
    for (let index = 0; index + 1 < samples.length; index += 2) {
        const horizontal = samples[index];
        const vertical = samples[index + 1];
        if (!Number.isFinite(horizontal) || !Number.isFinite(vertical)) {
            hasPreviousPoint = false;
            continue;
        }

        const x = toX(horizontal);
        const y = toY(vertical);
        if (hasPreviousPoint) {
            context.lineTo(x, y);
        } else {
            context.moveTo(x, y);
            hasPreviousPoint = true;
        }
    }

    context.stroke();
}

function drawZeroAxes(
    surface: PlotSurface,
    horizontal: Bounds,
    vertical: Bounds,
    toX: (value: number) => number,
    toY: (value: number) => number,
) {
    const { context, width, height } = surface;
    context.strokeStyle = AXIS_COLOR;
    context.lineWidth = 1;
    context.beginPath();

    if (horizontal.minimum <= 0 && horizontal.maximum >= 0) {
        const x = toX(0);
        context.moveTo(x, 0);
        context.lineTo(x, height);
    }

    if (vertical.minimum <= 0 && vertical.maximum >= 0) {
        const y = toY(0);
        context.moveTo(0, y);
        context.lineTo(width, y);
    }

    context.stroke();
}

/** Plots interleaved `(time, value)` pairs against time. */
export interface SeriesPlotOptions {
    unit?: string;
    exponential?: boolean;
}

export function drawSeriesPlot(
    canvas: HTMLCanvasElement,
    samples: Float32Array,
    options: SeriesPlotOptions = {},
): string | undefined {
    const surface = preparePlotSurface(canvas);
    if (!surface) return undefined;

    drawFrame(surface);
    if (samples.length < 4) return undefined;

    const traces = [{ samples }];
    const timeBounds = collectBounds(traces, 0);
    const valueBounds = collectBounds(traces, 1);
    if (!timeBounds || !valueBounds) return undefined;

    const paddedValues = padBounds(valueBounds, 0.08);
    const toX = projector(timeBounds, surface.width, false);
    const toY = projector(paddedValues, surface.height, true);

    strokeTrace(surface.context, samples, SERIES_COLORS[0], toX, toY);

    const format = (value: number) =>
        options.exponential ? value.toExponential(3) : value.toFixed(4);
    const unit = options.unit ? ` ${options.unit}` : "";
    const span = (timeBounds.maximum - timeBounds.minimum).toFixed(3);

    return `${format(valueBounds.minimum)} to ${format(valueBounds.maximum)}${unit} over ${span} s`;
}

/** Plots interleaved `(angle, angular velocity)` pairs for one or two traces. */
export function drawPhasePlot(
    canvas: HTMLCanvasElement,
    traces: readonly PlotTrace[],
): string | undefined {
    const surface = preparePlotSurface(canvas);
    if (!surface) return undefined;

    drawFrame(surface);

    const drawableTraces = traces.filter(({ samples }) => samples.length >= 4);
    if (drawableTraces.length === 0) return undefined;

    const angleBounds = collectBounds(drawableTraces, 0);
    const velocityBounds = collectBounds(drawableTraces, 1);
    if (!angleBounds || !velocityBounds) return undefined;

    const paddedAngles = padBounds(angleBounds, 0.08);
    const paddedVelocities = padBounds(velocityBounds, 0.08);
    const toX = projector(paddedAngles, surface.width, false);
    const toY = projector(paddedVelocities, surface.height, true);

    drawZeroAxes(surface, paddedAngles, paddedVelocities, toX, toY);

    drawableTraces.forEach(({ samples }, index) => {
        strokeTrace(
            surface.context,
            samples,
            SERIES_COLORS[index % SERIES_COLORS.length],
            toX,
            toY,
        );
    });

    return `theta ${angleBounds.minimum.toFixed(2)} to ${angleBounds.maximum.toFixed(2)} rad · omega ${velocityBounds.minimum.toFixed(2)} to ${velocityBounds.maximum.toFixed(2)} rad/s`;
}
