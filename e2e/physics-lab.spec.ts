import { expect, test } from "@playwright/test";

interface PhysicsLabModule {
    _physicsLabPendulumSimulationTime?: () => number;
    _physicsLabPendulumMouseX?: () => number;
    _physicsLabPendulumMouseY?: () => number;
}

type PhysicsLabWindow = Window & { Module?: PhysicsLabModule };

test("catalog navigation is data-driven and collapsible", async ({ page }) => {
    await page.goto("/");

    await expect(
        page.getByRole("heading", {
            name: "Physics you can explore, not just observe.",
        }),
    ).toBeVisible();
    await expect(
        page.getByRole("heading", { name: "Pendulum Lab" }),
    ).toBeVisible();

    const toggle = page.getByRole("button", {
        name: "Collapse simulation navigation",
    });
    await expect(toggle).toHaveAttribute("aria-expanded", "true");
    await toggle.click();
    await expect(
        page.getByRole("button", { name: "Expand simulation navigation" }),
    ).toHaveAttribute("aria-expanded", "false");
});

test("canonical theory renders math without loading a simulation runtime", async ({
    page,
}) => {
    const runtimeRequests: string[] = [];
    page.on("request", (request) => {
        if (request.url().includes("/runtime/")) {
            runtimeRequests.push(request.url());
        }
    });

    await page.goto("/simulations/pendulum/theory/");

    await expect(
        page.getByRole("heading", { name: "Pendulum Lab theory" }),
    ).toBeVisible();
    await expect(page.locator(".katex").first()).toBeVisible();
    await expect(page.locator("math").first()).toBeAttached();
    expect(runtimeRequests).toEqual([]);
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

test("the real C++ pendulum runtime initializes and advances", async ({
    page,
}) => {
    const pageErrors: string[] = [];
    page.on("pageerror", (error) => pageErrors.push(error.message));

    await page.goto("/simulations/pendulum/");

    const canvas = page.locator(
        'canvas[aria-label="Pendulum Lab interactive simulation canvas"]',
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
                        ?._physicsLabPendulumSimulationTime,
            ),
        )
        .toBe("function");

    await expect
        .poll(() =>
            page.evaluate(
                () =>
                    typeof (window as PhysicsLabWindow).Module
                        ?._physicsLabPendulumMouseX,
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
        Math.abs(canvasDimensions.drawingHeight - canvasDimensions.cssHeight),
    ).toBeLessThanOrEqual(1);

    const canvasBox = await canvas.boundingBox();
    expect(canvasBox).not.toBeNull();
    if (!canvasBox) throw new Error("The simulation canvas has no layout box.");

    const expectedMouse = {
        x: canvasBox.width * 0.72,
        y: canvasBox.height * 0.48,
    };
    await page.mouse.move(
        canvasBox.x + expectedMouse.x,
        canvasBox.y + expectedMouse.y,
    );

    await expect
        .poll(() =>
            page.evaluate(
                ({ expectedX, expectedY }) => {
                    const module = (window as PhysicsLabWindow).Module;
                    const actualX = module?._physicsLabPendulumMouseX?.() ?? -1;
                    const actualY = module?._physicsLabPendulumMouseY?.() ?? -1;
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
                    Math.abs(canvasElement.width - canvasElement.clientWidth),
                    Math.abs(canvasElement.height - canvasElement.clientHeight),
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
            ).Module?._physicsLabPendulumSimulationTime?.() ?? 0,
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
                            ).Module?._physicsLabPendulumSimulationTime?.() ??
                            0,
                    ),
                { timeout: 10_000 },
            )
            .toBeGreaterThan(initialTime);
    } finally {
        await page.keyboard.up("Space");
    }

    expect(pageErrors).toEqual([]);
});
