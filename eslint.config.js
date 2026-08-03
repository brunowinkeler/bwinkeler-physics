import eslint from "@eslint/js";
import astro from "eslint-plugin-astro";
import globals from "globals";
import tseslint from "typescript-eslint";

export default [
    {
        ignores: [
            ".astro/**",
            "build/**",
            "dist/**",
            "emsdk-cache/**",
            "node_modules/**",
            "public/runtime/**",
            "vendor/**",
        ],
    },
    eslint.configs.recommended,
    ...tseslint.configs.recommended,
    ...astro.configs.recommended,
    {
        languageOptions: {
            globals: {
                ...globals.browser,
                ...globals.node,
            },
        },
    },
];
