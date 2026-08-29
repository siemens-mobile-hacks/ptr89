[![NPM Version](https://img.shields.io/npm/v/%40sie-js%2Fptr89)](https://www.npmjs.com/package/@sie-js/ptr89)

# JavaScript module

Install the package with pnpm:

```bash
pnpm add @sie-js/ptr89
```

## Example

```ts
import { readFile } from "node:fs/promises";
import { Ptr89 } from "@sie-js/ptr89";

const fullflash = await readFile("EL71v45.bin");
const ptr89 = new Ptr89();
await ptr89.open(fullflash, { arch: "arm" });

const search = ptr89.find("F0B5061C0C1C151C85B068461122??49", 1);
const xrefs = ptr89.findXRefs(0xA04CA048, 3);

ptr89.close();
```

## API

### `new Ptr89()`

Creates a pattern finder. Call `open()` before searching.

### `open(data, options?)`

```ts
type Ptr89Arch = "arm" | "c166";

interface Ptr89OpenOptions {
	arch?: Ptr89Arch;
	base?: number;
}

open(data: Uint8Array, options?: Ptr89OpenOptions): Promise<void>;
```

Copies a fullflash into WebAssembly memory. The input buffer is no longer
needed after the promise resolves. Calling `open()` again replaces the current
fullflash. Node.js `Buffer` values can be passed directly because `Buffer`
extends `Uint8Array`.

- `arch` selects the instruction decoder. Default: `"arm"`.
- `base` sets the load address. The ARM default is `0xA0000000`. For C166 it is
  calculated as `0x1000000 - data.byteLength`.

### `openFile(file, options?)`

```ts
openFile(file: Blob, options?: Ptr89OpenOptions): Promise<void>;
```

Reads a `File` or `Blob` directly into WebAssembly memory. Options are the same
as for `open()`.

### `find(pattern, limit?, align?)`

```ts
find(pattern: string, limit?: number, align?: number): Ptr89Search;
```

Finds one pattern in the opened fullflash. The default limit is `100`; a limit
of `0` disables it. The default alignment is `1`. The returned object has the
same shape as one entry in the CLI `-J` `patterns` array:

```ts
type Ptr89SearchType = "address" | "pointer" | "reference" | "branch";

interface Ptr89Search {
	pattern: string;
	type: Ptr89SearchType;
	results: Ptr89SearchResult[];
}

interface Ptr89SearchResult {
	address: number;
	offset?: number;
	bytes?: string;
}
```

`address` is the resolved address, pointer value or decoded target. For a
regular pattern, `offset` points to the matched bytes. For other results it is
`address - base` and is omitted when `address` is outside the fullflash. `bytes`
is returned only for a regular pattern result.

### `findXRefs(address, limit?)`

```ts
findXRefs(address: number, limit?: number): Ptr89XRef[];
```

Finds branches, decoded references and stored pointers to a 32-bit address.
The default limit is `100`; a limit of `0` disables it:

```ts
type Ptr89XRefType = "pointer" | "reference" | "branch";

interface Ptr89XRef {
	type: Ptr89XRefType;
	xref: number;
	offset: number;
}
```

### `setDebug(enabled)`

```ts
setDebug(enabled: boolean): void;
```

Enables or disables decoder and pattern-matching logs from the WebAssembly
module.

### `close()`

```ts
close(): void;
```

Releases the copied fullflash and the underlying Embind object. Calling it
more than once is safe.

### `prettify(pattern)`

```ts
prettify(pattern: string): Promise<string>;
```

Parses a pattern and returns its normalized representation. Invalid patterns
reject the promise with a syntax error.

All addresses and offsets are unsigned 32-bit numbers represented as JavaScript
`number` values.

## Building from sources

Build the WebAssembly module from sources:

```bash
pnpm run build:wasm
```

The build produces `ptr89_wasm.js`, `ptr89_wasm.wasm` and `ptr89_wasm.d.ts`
in `build-wasm/`.
