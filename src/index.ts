import createModule from "#build/ptr89_wasm.js";

type WasmModule = Awaited<ReturnType<typeof createModule>>;
type WasmPtr89 = InstanceType<WasmModule["Ptr89"]>;

export type Ptr89Arch = "arm" | "c166";
export type Ptr89SearchType = "address" | "pointer" | "reference" | "branch";
export type Ptr89XRefType = "pointer" | "reference" | "branch";

export interface Ptr89OpenOptions {
	arch?: Ptr89Arch;
	base?: number;
}

export interface Ptr89SearchResult {
	address: number;
	offset?: number;
	bytes?: string;
}

export interface Ptr89Search {
	pattern: string;
	type: Ptr89SearchType;
	results: Ptr89SearchResult[];
}

export interface Ptr89XRef {
	type: Ptr89XRefType;
	xref: number;
	offset: number;
}

const C166_ADDRESS_SPACE_SIZE = 0x1000000;
let modulePromise: ReturnType<typeof createModule> | undefined;

function loadModule(): ReturnType<typeof createModule> {
	modulePromise ??= createModule();
	return modulePromise;
}

function getError(module: WasmModule, error: unknown): Error {
	if (error instanceof Error)
		return error;
	try {
		const details = module.getExceptionMessage(error);
		const message = Array.isArray(details) ? details[1] ?? details[0] : details;
		try {
			module.decrementExceptionRefcount(error);
		} catch {
			// The exception may not be owned by the C++ runtime.
		}
		return new Error(String(message));
	} catch {
		return new Error(String(error));
	}
}

function validateLimit(limit: number): void {
	if (!Number.isSafeInteger(limit) || limit < 0)
		throw new RangeError("Result limit must be a non-negative integer.");
}

function validateAlign(align: number): void {
	if (!Number.isSafeInteger(align) || align <= 0)
		throw new RangeError("Search alignment must be a positive integer.");
}

export class Ptr89 {
	private module?: WasmModule;
	private handle?: WasmPtr89;
	private ptr?: number;

	async open(data: Uint8Array, options: Ptr89OpenOptions = {}): Promise<void> {
		await this.openData(data.byteLength, options, (module, ptr) => module.HEAPU8.set(data, ptr));
	}

	async openFile(file: Blob, options: Ptr89OpenOptions = {}): Promise<void> {
		await this.openData(file.size, options, async (module, ptr) => {
			const reader = file.stream().getReader();
			let offset = 0;
			try {
				while (true) {
					const { done, value } = await reader.read();
					if (done)
						break;
					if (value.byteLength > file.size - offset)
						throw new RangeError("Blob stream is larger than the declared size.");
					module.HEAPU8.set(value, ptr + offset);
					offset += value.byteLength;
				}
			} finally {
				reader.releaseLock();
			}
			if (offset !== file.size)
				throw new RangeError("Blob stream is smaller than the declared size.");
		});
	}

	close(): void {
		if (!this.handle)
			return;
		this.handle.close();
		this.handle.delete();
		if (this.module && this.ptr !== undefined)
			this.module._free(this.ptr);
		this.handle = undefined;
		this.module = undefined;
		this.ptr = undefined;
	}

	setDebug(enabled: boolean): void {
		this.getHandle().setDebug(enabled);
	}

	find(pattern: string, limit = 100, align = 1): Ptr89Search {
		validateLimit(limit);
		validateAlign(align);
		const [module, handle] = this.getState();
		try {
			const search = handle.find(pattern, limit, align);
			const matches = search.results;
			try {
				const results = Array.from({ length: matches.size() }, (_, i) => {
					const match = matches.get(i);
					if (!match)
						throw new Error(`Missing search result at index ${i}.`);
					const result: Ptr89SearchResult = { address: match.address };
					if (match.offset !== undefined)
						result.offset = match.offset;
					const bytes = String(match.bytes);
					if (bytes)
						result.bytes = bytes;
					return result;
				});
				return {
					pattern: String(search.pattern),
					type: String(search.type) as Ptr89SearchType,
					results,
				};
			} finally {
				matches.delete();
			}
		} catch (error) {
			throw getError(module, error);
		}
	}

	findXRefs(address: number, limit = 100): Ptr89XRef[] {
		validateLimit(limit);
		if (!Number.isSafeInteger(address) || address < 0 || address > 0xFFFFFFFF)
			throw new RangeError("Address must be a 32-bit unsigned integer.");

		const [module, handle] = this.getState();
		try {
			const matches = handle.findXRefs(address, limit);
			try {
				return Array.from({ length: matches.size() }, (_, i) => {
					const match = matches.get(i);
					if (!match)
						throw new Error(`Missing x-ref result at index ${i}.`);
					return {
						type: String(match.type) as Ptr89XRefType,
						xref: match.xref,
						offset: match.offset,
					};
				});
			} finally {
				matches.delete();
			}
		} catch (error) {
			throw getError(module, error);
		}
	}

	private async openData(size: number, options: Ptr89OpenOptions,
		write: (module: WasmModule, ptr: number) => void | Promise<void>): Promise<void> {
		const arch = options.arch ?? "arm";

		let base = options.base;
		if (base === undefined) {
			if (arch === "c166") {
				if (size > C166_ADDRESS_SPACE_SIZE)
					throw new RangeError("C166 fullflash is larger than the 16 MiB address space; specify base explicitly.");
				base = C166_ADDRESS_SPACE_SIZE - size;
			} else {
				base = 0xA0000000;
			}
		}
		if (!Number.isSafeInteger(base) || base < 0 || base > 0xFFFFFFFF)
			throw new RangeError("Memory base must be a 32-bit unsigned integer.");

		const module = await loadModule();
		const handle = this.handle ?? new module.Ptr89();
		const ptr = module._malloc(size);
		try {
			await write(module, ptr);
			handle.open(ptr, size, base, arch);
			if (this.module && this.ptr !== undefined)
				this.module._free(this.ptr);
			this.module = module;
			this.handle = handle;
			this.ptr = ptr;
		} catch (error) {
			if (!this.handle)
				handle.delete();
			throw getError(module, error);
		} finally {
			if (this.ptr !== ptr)
				module._free(ptr);
		}
	}

	private getHandle(): WasmPtr89 {
		if (!this.handle)
			throw new Error("Ptr89 is not opened.");
		return this.handle;
	}

	private getState(): [WasmModule, WasmPtr89] {
		if (!this.module || !this.handle)
			throw new Error("Ptr89 is not opened.");
		return [this.module, this.handle];
	}
}

export async function prettify(pattern: string): Promise<string> {
	const module = await loadModule();
	try {
		return module.prettify(pattern);
	} catch (error) {
		throw getError(module, error);
	}
}
