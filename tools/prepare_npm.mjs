import { cp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import path from "node:path";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const output = path.join(root, "build-npm");

await rm(output, { recursive: true, force: true });
await mkdir(output);

const packageJson = JSON.parse(await readFile(path.join(root, "package.json"), "utf8"));
delete packageJson.scripts.prepack;
delete packageJson.publishConfig.directory;

await writeFile(path.join(output, "package.json"), `${JSON.stringify(packageJson, null, "\t")}\n`);
await cp(path.join(root, "JS.md"), path.join(output, "README.md"));
await cp(path.join(root, "LICENSE"), path.join(output, "LICENSE"));
await cp(path.join(root, "dist"), path.join(output, "dist"), { recursive: true });
await cp(path.join(root, "build-wasm"), path.join(output, "build-wasm"), { recursive: true });
