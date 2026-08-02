import { defineCollection } from "astro:content";
import { glob } from "astro/loaders";

const theory = defineCollection({
    loader: glob({
        base: "./vendor/cpp-physics-simulation",
        pattern: "project*/PHYSICS_THEORY.md",
        generateId: ({ entry }) => entry.split("/")[0],
    }),
});

export const collections = { theory };
