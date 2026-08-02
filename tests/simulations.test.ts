import { access, readFile } from "node:fs/promises";
import { resolve } from "node:path";

import { describe, expect, it } from "vitest";

import {
    publishedSimulations,
    simulationCategories,
    simulationPath,
    simulations,
    theoryPath,
    visibleSimulations,
} from "../src/data/simulations";

const repositoryRoot = resolve(import.meta.dirname, "..");
const nativeSourceRoot = resolve(
    repositoryRoot,
    "vendor/cpp-physics-simulation",
);

function readCmakeList(document: string, variable: string) {
    const expression = new RegExp(
        `set\\(${variable}\\s+([\\s\\S]*?)\\n\\)`,
        "m",
    );
    const match = document.match(expression);
    if (!match?.[1]) {
        throw new Error(`CMake list '${variable}' was not found.`);
    }

    return match[1]
        .split(/\r?\n/u)
        .map((line) => line.trim())
        .filter((line) => line.length > 0 && !line.startsWith("#"));
}

describe("simulation registry", () => {
    it("uses unique stable identifiers and routes", () => {
        const ids = simulations.map(({ id }) => id);
        const routes = publishedSimulations.flatMap((simulation) => [
            simulationPath(simulation),
            theoryPath(simulation),
        ]);

        expect(new Set(ids).size).toBe(ids.length);
        expect(new Set(routes).size).toBe(routes.length);
        expect(ids).toEqual(
            expect.arrayContaining(["ballistics", "pendulum", "gas-brownian"]),
        );
    });

    it("references declared categories and sorts visible entries", () => {
        const categoryIds = new Set(
            simulationCategories.map((category) => category.id),
        );

        for (const simulation of simulations) {
            expect(categoryIds.has(simulation.category)).toBe(true);
        }

        for (const category of simulationCategories) {
            const orders = visibleSimulations
                .filter((simulation) => simulation.category === category.id)
                .map((simulation) => simulation.order);

            expect(orders).toEqual(
                orders.toSorted((left, right) => left - right),
            );
        }
    });

    it("provides a runtime only for published simulations", () => {
        for (const simulation of simulations) {
            if (simulation.status === "published") {
                expect(simulation.runtimeScript).toMatch(/^\/runtime\/.+\.js$/);
            } else {
                expect(simulation.runtimeScript).toBeUndefined();
            }
        }
    });

    it("resolves every canonical theory file from the pinned submodule", async () => {
        await Promise.all(
            simulations.map((simulation) =>
                access(resolve(nativeSourceRoot, simulation.theoryFile)),
            ),
        );
    });

    it("keeps the web source manifest aligned with the native Project 2 target", async () => {
        const [nativeCmake, webCmake] = await Promise.all([
            readFile(
                resolve(nativeSourceRoot, "project2_pendulum/CMakeLists.txt"),
                "utf8",
            ),
            readFile(resolve(repositoryRoot, "CMakeLists.txt"), "utf8"),
        ]);

        const nativeSources = readCmakeList(
            nativeCmake,
            "PROJECT2_PENDULUM_SOURCES",
        )
            .filter(
                (source) =>
                    source !== "src/main.cpp" &&
                    source !== "src/app/PendulumApplication.cpp",
            )
            .map((source) =>
                source.startsWith("../core/")
                    ? source.slice(3)
                    : `project2_pendulum/${source}`,
            )
            .toSorted();
        const webSources = readCmakeList(
            webCmake,
            "PENDULUM_NATIVE_SOURCES",
        ).toSorted();

        expect(webSources).toEqual(nativeSources);
    });
});
