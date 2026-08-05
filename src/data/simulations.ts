export const simulationCategories = [
    {
        id: "mechanics",
        title: "Mechanics",
        description: "Motion, forces, energy, and numerical integration.",
    },
    {
        id: "statistical-physics",
        title: "Statistical Physics",
        description: "Collective behavior emerging from many particles.",
    },
] as const;

export type SimulationCategoryId = (typeof simulationCategories)[number]["id"];
export type SimulationStatus = "published" | "coming-soon" | "draft";

export interface SimulationDefinition {
    id: string;
    title: string;
    shortTitle: string;
    description: string;
    category: SimulationCategoryId;
    order: number;
    status: SimulationStatus;
    engine: "Raylib" | "SDL3";
    sourceDirectory: string;
    theoryFile: string;
    runtimeScript?: string;
    shortcuts: string;
    minimumViewport: {
        width: number;
        height: number;
    };
}

export const simulations: readonly SimulationDefinition[] = [
    {
        id: "ballistics",
        title: "Ballistics Lab",
        shortTitle: "Ballistics",
        description:
            "Explore two-dimensional projectile motion, drag, impact interpolation, and numerical integration.",
        category: "mechanics",
        order: 10,
        status: "published",
        engine: "Raylib",
        sourceDirectory: "project1_balistica",
        theoryFile: "project1_balistica/PHYSICS_THEORY.md",
        runtimeScript: "/runtime/ballistics/ballistics.js",
        shortcuts: "Space: launch or pause · R: reset",
        minimumViewport: { width: 1024, height: 640 },
    },
    {
        id: "pendulum",
        title: "Pendulum Lab",
        shortTitle: "Pendulums",
        description:
            "Compare simple and double pendulums, integrators, energy behavior, phase space, and chaotic motion.",
        category: "mechanics",
        order: 20,
        status: "published",
        engine: "Raylib",
        sourceDirectory: "project2_pendulum",
        theoryFile: "project2_pendulum/PHYSICS_THEORY.md",
        runtimeScript: "/runtime/pendulum/pendulum.js",
        shortcuts: "Space: start or pause · R: reset · 1/2: change model",
        minimumViewport: { width: 1024, height: 640 },
    },
    {
        id: "gas-brownian",
        title: "Gas and Brownian Motion Lab",
        shortTitle: "Gas and Brownian Motion",
        description:
            "Study elastic collisions, effective temperature, pressure, and Brownian trajectories in a particle bath.",
        category: "statistical-physics",
        order: 10,
        status: "published",
        engine: "Raylib",
        sourceDirectory: "project3_gas_brownian",
        theoryFile: "project3_gas_brownian/PHYSICS_THEORY.md",
        runtimeScript: "/runtime/gas-brownian/gas-brownian.js",
        shortcuts:
            "Space: start or pause · R: reset · 1/2: change mode · T: tracer trail",
        minimumViewport: { width: 1024, height: 640 },
    },
] as const;

export type Simulation = SimulationDefinition;

export const visibleSimulations = simulations
    .filter((simulation) => simulation.status !== "draft")
    .toSorted((left, right) => left.order - right.order);

export const publishedSimulations = visibleSimulations.filter(
    (simulation) => simulation.status === "published",
);

export function simulationPath(simulation: Pick<SimulationDefinition, "id">) {
    return `/simulations/${simulation.id}/`;
}

export function theoryPath(simulation: Pick<SimulationDefinition, "id">) {
    return `${simulationPath(simulation)}theory/`;
}

export function getSimulation(id: string) {
    return simulations.find((simulation) => simulation.id === id);
}

export function getCategory(id: SimulationCategoryId) {
    return simulationCategories.find((category) => category.id === id);
}
