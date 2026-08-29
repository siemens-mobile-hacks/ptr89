import createModule from "#build/ptr89_wasm.js";

type WasmModule = Awaited<ReturnType<typeof createModule>>;
type WasmPtr89 = InstanceType<WasmModule["Ptr89"]>;

export type Architecture = "arm" | "c166";
export type PatternSearchResultType = "offset" | "pointer" | "reference" | "branch" | "static_value";
export type XRefSearchResultType = "pointer" | "reference" | "branch";

export interface OpenOptions {
	arch?: Architecture;
	base?: number;
	align?: number;
}

export interface PatternSearchResult {
	type: PatternSearchResultType;
	address: number;
	offset: number;
	value: number;
}

export interface XRefSearchResult {
	type: XRefSearchResultType;
	address: number;
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

export class Ptr89 {
	private module?: WasmModule;
	private handle?: WasmPtr89;

	async open(data: Uint8Array, options: OpenOptions = {}): Promise<void> {
		const arch = options.arch ?? "arm";
		const align = options.align ?? 1;
		if (!Number.isSafeInteger(align) || align <= 0)
			throw new RangeError("Memory alignment must be a positive integer.");

		let base = options.base;
		if (base === undefined) {
			if (arch === "c166") {
				if (data.byteLength > C166_ADDRESS_SPACE_SIZE)
					throw new RangeError("C166 fullflash is larger than the 16 MiB address space; specify base explicitly.");
				base = C166_ADDRESS_SPACE_SIZE - data.byteLength;
			} else {
				base = 0xA0000000;
			}
		}
		if (!Number.isSafeInteger(base) || base < 0 || base > 0xFFFFFFFF)
			throw new RangeError("Memory base must be a 32-bit unsigned integer.");

		const module = await loadModule();
		const handle = this.handle ?? new module.Ptr89();
		const ptr = module._malloc(data.byteLength);
		module.HEAPU8.set(data, ptr);
		try {
			handle.open(ptr, data.byteLength, base, align, arch);
			this.module = module;
			this.handle = handle;
		} catch (error) {
			if (!this.handle)
				handle.delete();
			throw getError(module, error);
		} finally {
			module._free(ptr);
		}
	}

	close(): void {
		if (!this.handle)
			return;
		this.handle.close();
		this.handle.delete();
		this.handle = undefined;
		this.module = undefined;
	}

	setDebug(enabled: boolean): void {
		this.getHandle().setDebug(enabled);
	}

	find(pattern: string, limit = 100): PatternSearchResult[] {
		validateLimit(limit);
		const [module, handle] = this.getState();
		try {
			const matches = handle.find(pattern, limit);
			try {
				return Array.from({ length: matches.size() }, (_, i) => {
					const match = matches.get(i);
					if (!match)
						throw new Error(`Missing search result at index ${i}.`);
					return match as PatternSearchResult;
				});
			} finally {
				matches.delete();
			}
		} catch (error) {
			throw getError(module, error);
		}
	}

	findXRefs(address: number, limit = 100): XRefSearchResult[] {
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
					return match as XRefSearchResult;
				});
			} finally {
				matches.delete();
			}
		} catch (error) {
			throw getError(module, error);
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
