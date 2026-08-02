import { expect, test } from "@playwright/test";

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
                    typeof (
                        window as Window & {
                            Module?: {
                                _physicsLabPendulumSimulationTime?: () => number;
                            };
                        }
                    ).Module?._physicsLabPendulumSimulationTime,
            ),
        )
        .toBe("function");

    const initialTime = await page.evaluate(
        () =>
            (
                window as Window & {
                    Module?: {
                        _physicsLabPendulumSimulationTime?: () => number;
                    };
                }
            ).Module?._physicsLabPendulumSimulationTime?.() ?? 0,
    );

    await page.keyboard.down("Space");

    try {
        await expect
            .poll(
                () =>
                    page.evaluate(
                        () =>
                            (
                                window as Window & {
                                    Module?: {
                                        _physicsLabPendulumSimulationTime?: () => number;
                                    };
                                }
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
