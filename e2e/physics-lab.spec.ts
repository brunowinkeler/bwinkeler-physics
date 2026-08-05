import { expect, test } from "@playwright/test";

import { publishedSimulations } from "../src/data/simulations";

interface PhysicsLabModule {
    _physicsLabSimulationTime?: () => number;
    _physicsLabMouseX?: () => number;
    _physicsLabMouseY?: () => number;
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
