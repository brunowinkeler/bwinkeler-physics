import { readdir, stat } from "node:fs/promises";
import { relative, resolve } from "node:path";

const outputRoot = resolve("dist");
const maximumFileSize = 25 * 1024 * 1024;
const maximumFileCount = 20_000;
const files = [];

async function collect(directory) {
    for (const entry of await readdir(directory, { withFileTypes: true })) {
        const path = resolve(directory, entry.name);
        if (entry.isDirectory()) {
            await collect(path);
        } else if (entry.isFile()) {
            files.push(path);
        }
    }
}

await collect(outputRoot);

if (files.length > maximumFileCount) {
    throw new Error(
        `Static output contains ${files.length} files, exceeding the Cloudflare Pages limit of ${maximumFileCount}.`,
    );
}

let totalSize = 0;
let largest = { path: "", size: 0 };

for (const path of files) {
    const { size } = await stat(path);
    totalSize += size;
    if (size > largest.size) largest = { path, size };

    if (size > maximumFileSize) {
        throw new Error(
            `${relative(outputRoot, path)} is ${size} bytes, exceeding the Cloudflare Pages 25 MiB per-file limit.`,
        );
    }
}

console.log(
    `Artifact check passed: ${files.length} files, ${totalSize} bytes total, largest ${relative(outputRoot, largest.path)} at ${largest.size} bytes.`,
);
