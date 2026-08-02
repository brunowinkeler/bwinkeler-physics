import { unified } from "@astrojs/markdown-remark";
import sitemap from "@astrojs/sitemap";
import { defineConfig } from "astro/config";
import rehypeKatex from "rehype-katex";
import remarkMath from "remark-math";

export default defineConfig({
    site: "https://physics.bwinkeler.com",
    output: "static",
    integrations: [sitemap()],
    markdown: {
        processor: unified({
            remarkPlugins: [remarkMath],
            rehypePlugins: [[rehypeKatex, { output: "htmlAndMathml" }]],
        }),
        shikiConfig: {
            themes: {
                light: "github-light",
                dark: "github-dark",
            },
        },
    },
});
