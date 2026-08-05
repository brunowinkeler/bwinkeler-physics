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
            expect(simulation.shortcuts.length).toBeGreaterThan(0);

            if (simulation.status === "published") {
                expect(simulation.runtimeScript).toBe(
                    `/runtime/${simulation.id}/${simulation.id}.js`,
                );
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

    it("keeps the web source manifest aligned with every native target", async () => {
        const targets = [
            {
                projectDirectory: "project1_balistica",
                nativeList: "PROJECT1_BALISTICA_SOURCES",
                webList: "BALLISTICS_NATIVE_SOURCES",
                excluded: ["src/main.cpp", "src/app/Application.cpp"],
            },
            {
                projectDirectory: "project2_pendulum",
                nativeList: "PROJECT2_PENDULUM_SOURCES",
                webList: "PENDULUM_NATIVE_SOURCES",
                excluded: ["src/main.cpp", "src/app/PendulumApplication.cpp"],
            },
            {
                projectDirectory: "project3_gas_brownian",
                nativeList: "PROJECT3_GAS_BROWNIAN_SOURCES",
                webList: "GAS_BROWNIAN_NATIVE_SOURCES",
                excluded: [
                    "src/main.cpp",
                    "src/app/GasBrownianApplication.cpp",
                ],
            },
        ];

        const webCmake = await readFile(
            resolve(repositoryRoot, "CMakeLists.txt"),
            "utf8",
        );

        for (const target of targets) {
            const nativeCmake = await readFile(
                resolve(
                    nativeSourceRoot,
                    `${target.projectDirectory}/CMakeLists.txt`,
                ),
                "utf8",
            );

            const nativeSources = readCmakeList(nativeCmake, target.nativeList)
                .filter((source) => !target.excluded.includes(source))
                .map((source) =>
                    source.startsWith("../core/")
                        ? source.slice(3)
                        : `${target.projectDirectory}/${source}`,
                )
                .toSorted();
            const webSources = readCmakeList(
                webCmake,
                target.webList,
            ).toSorted();

            expect(webSources).toEqual(nativeSources);
        }
    });
});
