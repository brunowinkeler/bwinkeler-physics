import { expect, test } from "@playwright/test";

import { simulationControls } from "../src/data/controls";
import { pendulumParameterOrder } from "../src/data/controls/pendulum";
import { publishedSimulations } from "../src/data/simulations";

interface PhysicsLabModule {
    _physicsLabSimulationTime?: () => number;
    _physicsLabMouseX?: () => number;
    _physicsLabMouseY?: () => number;
    _physicsLabGetParameter?: (parameter: number) => number;
}

type PhysicsLabWindow = Window & { Module?: PhysicsLabModule };

test("catalog navigation is data-driven and collapsible", async ({ page }) => {
    await page.goto("/");

    await expect(
        page.getByRole("heading", {
            name: "Physics you can explore, not just observe.",
        }),
    ).toBeVisible();

    for (const simulation of publishedSimulations) {
        await expect(
            page.getByRole("heading", { name: simulation.title }),
        ).toBeVisible();
    }

    const toggle = page.getByRole("button", {
        name: "Collapse simulation navigation",
    });
    await expect(toggle).toHaveAttribute("aria-expanded", "true");
    await toggle.click();
    await expect(
        page.getByRole("button", { name: "Expand simulation navigation" }),
    ).toHaveAttribute("aria-expanded", "false");
});

test("an undersized viewport receives guidance without downloading the runtime", async ({
    page,
}) => {
    const runtimeRequests: string[] = [];
    page.on("request", (request) => {
        if (request.url().includes("/runtime/")) {
            runtimeRequests.push(request.url());
        }
    });
    await page.setViewportSize({ width: 800, height: 600 });

    await page.goto("/simulations/pendulum/");

    await expect(page.locator("[data-runtime-status-text]")).toContainText(
        "requires a viewport of at least 1024 × 640 pixels",
    );
    expect(runtimeRequests).toEqual([]);
});

for (const simulation of publishedSimulations) {
    test(`${simulation.id} theory renders math without loading a runtime`, async ({
        page,
    }) => {
        const runtimeRequests: string[] = [];
        page.on("request", (request) => {
            if (request.url().includes("/runtime/")) {
                runtimeRequests.push(request.url());
            }
        });

        await page.goto(`/simulations/${simulation.id}/theory/`);

        await expect(
            page.getByRole("heading", { name: `${simulation.title} theory` }),
        ).toBeVisible();
        await expect(page.locator(".katex").first()).toBeVisible();
        await expect(page.locator("math").first()).toBeAttached();
        expect(runtimeRequests).toEqual([]);
    });

    test(`the real C++ ${simulation.id} runtime initializes and advances`, async ({
        page,
    }) => {
        const pageErrors: string[] = [];
        page.on("pageerror", (error) => pageErrors.push(error.message));

        await page.goto(`/simulations/${simulation.id}/`);

        const canvas = page.locator(
            `canvas[aria-label="${simulation.title} interactive simulation canvas"]`,
        );
        await expect(canvas).toBeVisible();
        await expect(page.locator("[data-runtime-status]")).toBeHidden({
            timeout: 30_000,
        });

        await expect
            .poll(() =>
                page.evaluate(
                    () =>
                        typeof (window as PhysicsLabWindow).Module
                            ?._physicsLabSimulationTime,
                ),
            )
            .toBe("function");

        const canvasDimensions = await canvas.evaluate((element) => {
            const canvasElement = element as HTMLCanvasElement;
            return {
                drawingWidth: canvasElement.width,
                drawingHeight: canvasElement.height,
                cssWidth: canvasElement.clientWidth,
                cssHeight: canvasElement.clientHeight,
            };
        });
        expect(
            Math.abs(canvasDimensions.drawingWidth - canvasDimensions.cssWidth),
        ).toBeLessThanOrEqual(1);
        expect(
            Math.abs(
                canvasDimensions.drawingHeight - canvasDimensions.cssHeight,
            ),
        ).toBeLessThanOrEqual(1);

        // Headers vary in height per simulation, so the canvas must be scrolled
        // into view before the pointer target is computed.
        await canvas.scrollIntoViewIfNeeded();
        const canvasBox = await canvas.boundingBox();
        expect(canvasBox).not.toBeNull();
        if (!canvasBox) {
            throw new Error("The simulation canvas has no layout box.");
        }

        const viewport = page.viewportSize();
        expect(viewport).not.toBeNull();
        if (!viewport) {
            throw new Error("The page has no viewport size.");
        }

        const expectedMouse = {
            x: canvasBox.width * 0.72,
            y: canvasBox.height * 0.48,
        };
        expect(canvasBox.y + expectedMouse.y).toBeLessThan(viewport.height);
        await page.mouse.move(
            canvasBox.x + expectedMouse.x,
            canvasBox.y + expectedMouse.y,
        );

        await expect
            .poll(() =>
                page.evaluate(
                    ({ expectedX, expectedY }) => {
                        const physicsLab = (window as PhysicsLabWindow).Module;
                        const actualX = physicsLab?._physicsLabMouseX?.() ?? -1;
                        const actualY = physicsLab?._physicsLabMouseY?.() ?? -1;
                        return Math.max(
                            Math.abs(actualX - expectedX),
                            Math.abs(actualY - expectedY),
                        );
                    },
                    { expectedX: expectedMouse.x, expectedY: expectedMouse.y },
                ),
            )
            .toBeLessThanOrEqual(2);

        const enterFullscreen = page.getByRole("button", {
            name: "Enter full screen",
        });
        await expect(enterFullscreen).toBeVisible();
        await enterFullscreen.click();
        const exitFullscreen = page.getByRole("button", {
            name: "Exit full screen",
        });
        await expect(exitFullscreen).toBeVisible();
        await expect
            .poll(() =>
                canvas.evaluate((element) => {
                    const canvasElement = element as HTMLCanvasElement;
                    return Math.max(
                        Math.abs(
                            canvasElement.width - canvasElement.clientWidth,
                        ),
                        Math.abs(
                            canvasElement.height - canvasElement.clientHeight,
                        ),
                    );
                }),
            )
            .toBeLessThanOrEqual(1);
        await exitFullscreen.click();
        await expect(enterFullscreen).toBeVisible();

        const initialTime = await page.evaluate(
            () =>
                (
                    window as PhysicsLabWindow
                ).Module?._physicsLabSimulationTime?.() ?? 0,
        );

        await canvas.focus();
        await page.keyboard.down("Space");

        try {
            await expect
                .poll(
                    () =>
                        page.evaluate(
                            () =>
                                (
                                    window as PhysicsLabWindow
                                ).Module?._physicsLabSimulationTime?.() ?? 0,
                        ),
                    { timeout: 10_000 },
                )
                .toBeGreaterThan(initialTime);
        } finally {
            await page.keyboard.up("Space");
        }

        expect(pageErrors).toEqual([]);
    });
}

for (const simulation of publishedSimulations) {
    const definition = simulationControls[simulation.id];
    if (!definition) continue;

    const field = definition.groups
        .flatMap((group) => group.fields)
        .find(
            (candidate) =>
                candidate.kind === "number" && candidate.variant === undefined,
        );

    test(`the ${simulation.id} control panel edits and clamps parameters`, async ({
        page,
    }) => {
        expect(field).toBeDefined();
        if (!field) return;

        const pageErrors: string[] = [];
        page.on("pageerror", (error) => pageErrors.push(error.message));

        await page.goto(`/simulations/${simulation.id}/`);
        await expect(page.locator("[data-runtime-status]")).toBeHidden({
            timeout: 30_000,
        });

        const panel = page.locator("[data-simulation-controls]");
        await expect(panel).toBeVisible();

        const parameter = definition.parameterOrder.indexOf(field.parameter);
        const input = panel.locator(
            `[data-control-parameter="${field.parameter}"]`,
        );
        const precision = field.precision ?? 3;
        const maximum = field.max ?? 1;
        const target = Number(
            (((field.min ?? 0) + maximum) / 2).toFixed(precision),
        );

        await input.fill(String(target));
        await input.blur();
        await expect
            .poll(() =>
                page.evaluate(
                    (index) =>
                        (
                            window as PhysicsLabWindow
                        ).Module?._physicsLabGetParameter?.(index) ?? 0,
                    parameter,
                ),
            )
            .toBeCloseTo(target, Math.min(precision, 3));

        // A value beyond the physical domain is clamped by the bridge and
        // mirrored back into the field.
        await input.fill(String(maximum * 2));
        await input.blur();
        await expect(input).toHaveValue(maximum.toFixed(precision));

        // Typing in the panel must not reach the simulation shortcuts.
        const idleTime = await page.evaluate(
            () =>
                (
                    window as PhysicsLabWindow
                ).Module?._physicsLabSimulationTime?.() ?? 0,
        );
        await input.focus();
        await page.keyboard.down("Space");
        await page.waitForTimeout(500);
        await page.keyboard.up("Space");
        expect(
            await page.evaluate(
                () =>
                    (
                        window as PhysicsLabWindow
                    ).Module?._physicsLabSimulationTime?.() ?? 0,
            ),
        ).toBe(idleTime);

        await page.getByRole("button", { name: "Start", exact: true }).click();
        await expect
            .poll(
                () =>
                    page.evaluate(
                        () =>
                            (
                                window as PhysicsLabWindow
                            ).Module?._physicsLabSimulationTime?.() ?? 0,
                    ),
                { timeout: 10_000 },
            )
            .toBeGreaterThan(idleTime);

        const state = panel.locator(
            "[data-readout-index='0'] [data-readout-value]",
        );
        await expect(state).toHaveText("Running");

        await page.getByRole("button", { name: "Reset" }).click();
        await expect(state).toHaveText("Idle");

        expect(pageErrors).toEqual([]);
    });
}

test("the ballistics panel archives and clears launches", async ({ page }) => {
    await page.goto("/simulations/ballistics/");
    await expect(page.locator("[data-runtime-status]")).toBeHidden({
        timeout: 30_000,
    });

    const panel = page.locator("[data-simulation-controls]");
    const records = panel.locator("[data-record-list]");
    await expect(records).toContainText("No archived launches yet.");

    await page.getByRole("button", { name: "Start", exact: true }).click();
    await page.waitForTimeout(1500);
    await page.getByRole("button", { name: "Reset" }).click();

    const firstRecord = records.locator("[data-record-id]").first();
    await expect(firstRecord).toBeVisible();
    await firstRecord.click();
    await expect(firstRecord).toContainText("Drag coefficient");

    await page.getByRole("button", { name: "Clear history" }).click();
    await expect(records).toContainText("No archived launches yet.");
});

test("the gas panel reveals the tracer controls in Brownian mode", async ({
    page,
}) => {
    await page.goto("/simulations/gas-brownian/");
    await expect(page.locator("[data-runtime-status]")).toBeHidden({
        timeout: 30_000,
    });

    const panel = page.locator("[data-simulation-controls]");
    const tracerMass = panel.locator(
        '[data-control-parameter="tracerMassScale"]',
    );
    await expect(tracerMass).toBeHidden();

    await panel.locator('[data-control-parameter="mode"]').selectOption("1");
    await expect(tracerMass).toBeVisible();

    // The pressure plot is optional, so clearing its toggle hides the figure.
    const pressurePlot = panel.locator('[data-plot-canvas="pressure"]');
    await expect(pressurePlot).toBeVisible();
    await panel
        .locator('[data-control-parameter="showPressurePlot"]')
        .uncheck();
    await expect(pressurePlot).toBeHidden();
});

test("the pendulum control panel drives the runtime from the DOM", async ({
    page,
}) => {
    const pageErrors: string[] = [];
    page.on("pageerror", (error) => pageErrors.push(error.message));

    await page.goto("/simulations/pendulum/");
    await expect(page.locator("[data-runtime-status]")).toBeHidden({
        timeout: 30_000,
    });

    const panel = page.locator("[data-simulation-controls]");
    await expect(panel).toBeVisible();

    const lengthParameter = pendulumParameterOrder.indexOf("simpleLength");
    const length = panel.locator('[data-control-parameter="simpleLength"]');
    await length.fill("3.25");
    await length.blur();

    await expect
        .poll(() =>
            page.evaluate(
                (parameter) =>
                    (
                        window as PhysicsLabWindow
                    ).Module?._physicsLabGetParameter?.(parameter) ?? 0,
                lengthParameter,
            ),
        )
        .toBeCloseTo(3.25, 3);

    // A value beyond the physical domain is clamped by the bridge and mirrored
    // back into the field.
    await length.fill("99");
    await length.blur();
    await expect(length).toHaveValue("5.000");

    const idleTime = await page.evaluate(
        () =>
            (
                window as PhysicsLabWindow
            ).Module?._physicsLabSimulationTime?.() ?? 0,
    );

    await page.getByRole("button", { name: "Start", exact: true }).click();
    await expect
        .poll(
            () =>
                page.evaluate(
                    () =>
                        (
                            window as PhysicsLabWindow
                        ).Module?._physicsLabSimulationTime?.() ?? 0,
                ),
            { timeout: 10_000 },
        )
        .toBeGreaterThan(idleTime);

    await expect(page.getByText("State", { exact: true })).toBeVisible();
    await expect(
        panel.locator("[data-readout-index='0'] [data-readout-value]"),
    ).toHaveText("Running");

    await page.getByRole("button", { name: "Reset" }).click();
    await expect(
        panel.locator("[data-readout-index='0'] [data-readout-value]"),
    ).toHaveText("Idle");

    // The double-pendulum rows replace the simple ones when the model changes.
    await panel.locator('[data-control-parameter="mode"]').selectOption("1");
    await expect(
        panel.locator('[data-control-parameter="doubleLength1"]'),
    ).toBeVisible();
    await expect(length).toBeHidden();

    expect(pageErrors).toEqual([]);
});
