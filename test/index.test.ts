import { Buffer } from "node:buffer";
import { describe, expect, it } from "vitest";

import { Ptr89, prettify } from "../src/index.js";

describe("Ptr89 WASM", () => {
	it("finds ARM patterns and x-refs", async () => {
		const memory = Buffer.from([
			0x00, 0x48,			// LDR R0, [PC, #0]
			0x00, 0xBF,			// NOP
			0x08, 0x00, 0x00, 0xA0,	// Pointer to 0xA0000008
			0x00, 0xBF,			// NOP at 0xA0000008
			0x10, 0xB5,			// PUSH {R4, LR} at 0xA000000A
		]);
		const ptr89 = new Ptr89();
		await ptr89.open(memory, { arch: "arm" });

		expect(ptr89.find("LDR[ 00BF ]", 1)).toEqual({
			pattern: "LDR[ 00BF ]",
			type: "address",
			results: [{
				address: 0xA0000000,
				offset: 0,
				bytes: "0048",
			}],
		});
		expect(ptr89.find("&(00 48)", 1)).toEqual({
			pattern: "&(00 48)",
			type: "reference",
			results: [{
				address: 0xA0000008,
				offset: 8,
			}],
		});
		expect(ptr89.find("*(08 00 00 A0)", 1)).toEqual({
			pattern: "*(08 00 00 A0)",
			type: "pointer",
			results: [{
				address: 0xA0000008,
				offset: 8,
			}],
		});
		expect(ptr89.findXRefs(0xA0000008, 10)).toContainEqual({
			type: "reference",
			xref: 0xA0000000,
			offset: 0,
		});
		expect(ptr89.find("10 B5", 1)).toEqual({
			pattern: "10 B5",
			type: "address",
			results: [{
				address: 0xA000000B,
				offset: 10,
				bytes: "10B5",
			}],
		});
		expect(ptr89.find("00 BF + 1", 1)).toEqual({
			pattern: "00 BF + 1",
			type: "address",
			results: [{
				address: 0xA0000003,
				offset: 2,
				bytes: "00BF",
			}],
		});
		expect(ptr89.find("48", 1, 2)).toEqual({
			pattern: "48",
			type: "address",
			results: [],
		});

		ptr89.close();
	});

	it("accepts the C166 architecture as a string", async () => {
		const memory = Uint8Array.from([
			0x0D, 0x01,	// JMPR cc_UC, 0xFFFFFE
			0xCC, 0x00,	// NOP
			0xDB, 0x00,	// RETS
		]);
		const ptr89 = new Ptr89();
		await ptr89.open(memory, { arch: "c166" });

		expect(ptr89.find("[ DB00 ]", 1)).toEqual({
			pattern: "[ DB00 ]",
			type: "address",
			results: [{
				address: 0xFFFFFA,
				offset: 0,
				bytes: "0D01",
			}],
		});
		expect(ptr89.find("&BL(0D01)", 1)).toEqual({
			pattern: "&BL(0D01)",
			type: "branch",
			results: [{
				address: 0xFFFFFE,
				offset: 4,
			}],
		});
		expect(ptr89.find("<123456>", 1)).toEqual({
			pattern: "<123456>",
			type: "address",
			results: [{
				address: 0x123456,
			}],
		});

		ptr89.close();
	});

	it("reopens memory", async () => {
		const ptr89 = new Ptr89();
		await ptr89.open(Uint8Array.from([
			0x10, 0xB5,	// PUSH {R4, LR}
		]));
		await ptr89.open(Uint8Array.from([
			0x00, 0xBF,	// NOP
		]));

		expect(ptr89.find("10 B5", 1).results).toEqual([]);
		expect(ptr89.find("00 BF", 1).results).toEqual([{
			address: 0xA0000000,
			offset: 0,
			bytes: "00BF",
		}]);

		ptr89.close();
	});

	it("opens a Blob directly", async () => {
		const memory = Uint8Array.from([
			0x00, 0xBF,	// NOP
			0x10, 0xB5,	// PUSH {R4, LR}
		]);
		const blob = new Blob([memory.subarray(0, 2), memory.subarray(2)]);
		blob.arrayBuffer = () => {
			throw new Error("arrayBuffer() must not be used");
		};

		const ptr89 = new Ptr89();
		await ptr89.openFile(blob, { arch: "arm" });

		expect(ptr89.find("10 B5", 1)).toEqual({
			pattern: "10 B5",
			type: "address",
			results: [{
				address: 0xA0000003,
				offset: 2,
				bytes: "10B5",
			}],
		});

		ptr89.close();
	});

	it("prettifies patterns", async () => {
		await expect(prettify("00bf")).resolves.toBe("00 BF");
		await expect(prettify("&BL(00bf) ")).resolves.toBe("&BL(00 BF)");
		await expect(prettify("<123456>")).resolves.toBe("< 00123456 >");
		await expect(prettify("LDR[ 00bf ] cc")).resolves.toBe("LDR[ 00 BF ] CC");
		await expect(prettify("GG")).rejects.toThrow("Syntax error");
	});
});
