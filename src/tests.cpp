#include <cstdint>
#include <ptr89.h>
#include <src/Arch.h>
#include <cassert>
#include <iostream>

using namespace Ptr89;

static inline uint8_t *I(const std::vector<uint8_t> &value) {
	static std::vector<uint8_t> tmp;
	tmp = value;
	return &tmp[0];
}

static void testArmDecoder() {
	const Arch &arch = getArch(ARCH_ARM);
	std::vector<uint8_t> data(0x2000);
	Pattern::Memory memory = { 0xA0000000, data.data(), data.size(), 1, ARCH_ARM };

	// FE FB FF 0B: BLEQ 0xA0000000
	assert(arch.decodeBranch4(0xA0001000, I({ 0xFE, 0xFB, 0xFF, 0x0B }), memory) == std::make_pair(true, 0xA0000000));

	// FE FB FF FA: BLX 0xA0000000
	assert(arch.decodeBranch4(0xA0001000, I({ 0xFE, 0xFB, 0xFF, 0xFA }), memory) == std::make_pair(true, 0xA0000000));

	// FE 03 00 0B: BLEQ 0xA0001000
	assert(arch.decodeBranch4(0xA0000000, I({ 0xFE, 0x03, 0x00, 0x0B }), memory) == std::make_pair(true, 0xA0001000));

	// FE 03 00 FA: BLX 0xA0001000
	assert(arch.decodeBranch4(0xA0000000, I({ 0xFE, 0x03, 0x00, 0xFA }), memory) == std::make_pair(true, 0xA0001000));

	// FE FB FF FB: BLX 0xA0000002
	assert(arch.decodeBranch4(0xA0001000, I({ 0xFE, 0xFB, 0xFF, 0xFB }), memory) == std::make_pair(true, 0xA0000002));

	// FE 03 00 FB: BLX 0xA0001002
	assert(arch.decodeBranch4(0xA0000000, I({ 0xFE, 0x03, 0x00, 0xFB }), memory) == std::make_pair(true, 0xA0001002));

	// FE F7 FE FF: BL 0xA0000000
	assert(arch.decodeBranch4(0xA0001000, I({ 0xFE, 0xF7, 0xFE, 0xFF }), memory) == std::make_pair(true, 0xA0000000));

	// FE F7 FE EF: BLX 0xA0000000
	assert(arch.decodeBranch4(0xA0001000, I({ 0xFE, 0xF7, 0xFE, 0xEF }), memory) == std::make_pair(true, 0xA0000000));

	// 00 F0 FE FF: BL 0xA0001000
	assert(arch.decodeBranch4(0xA0000000, I({ 0x00, 0xF0, 0xFE, 0xFF }), memory) == std::make_pair(true, 0xA0001000));

	// 00 F0 FE EF: BLX 0xA0001000
	assert(arch.decodeBranch4(0xA0000000, I({ 0x00, 0xF0, 0xFE, 0xEF }), memory) == std::make_pair(true, 0xA0001000));

	// 7E E7: B 0xA0000000
	assert(arch.decodeBranch2(0xA0000100, I({ 0x7E, 0xE7 }), memory) == std::make_pair(true, 0xA0000000));

	// 7E E0: B 0xA0000100
	assert(arch.decodeBranch2(0xA0000000, I({ 0x7E, 0xE0 }), memory) == std::make_pair(true, 0xA0000100));

	// F6 D0: BEQ 0xA0000000
	assert(arch.decodeBranch2(0xA0000010, I({ 0xF6, 0xD0 }), memory) == std::make_pair(true, 0xA0000000));

	// 06 D0: BEQ 0xA0000010
	assert(arch.decodeBranch2(0xA0000000, I({ 0x06, 0xD0 }), memory) == std::make_pair(true, 0xA0000010));

	const uint32_t pointer = 0xA0001800;
	auto writePointer = [&data, pointer](size_t offset) {
		data[offset] = pointer & 0xFF;
		data[offset + 1] = (pointer >> 8) & 0xFF;
		data[offset + 2] = (pointer >> 16) & 0xFF;
		data[offset + 3] = pointer >> 24;
	};
	writePointer(0x08);
	writePointer(0x5C);
	writePointer(0x108);

	// 16 48: LDR R0, [PC, #0x58] -> 0xA000005C
	assert(arch.decodeReferences2(0xA0000000, I({ 0x16, 0x48 }), memory) == std::vector<uint32_t>{ pointer });

	// 16 48: LDR R0, [PC, #0x58] -> 0xA000005C
	assert(arch.decodeReferences2(0xA0000002, I({ 0x16, 0x48 }), memory) == std::vector<uint32_t>{ pointer });

	// 00 01 9F E5: LDR R0, [PC, #0x100] -> 0xA0000108
	assert(arch.decodeReferences4(0xA0000000, I({ 0x00, 0x01, 0x9F, 0xE5 }), memory) == std::vector<uint32_t>{ pointer });

	// 00 01 1F E5: LDR R0, [PC, #-0x100] -> 0xA0000008
	assert(arch.decodeReferences4(0xA0000100, I({ 0x00, 0x01, 0x1F, 0xE5 }), memory) == std::vector<uint32_t>{ pointer });

	// 00 F1 9F E5: LDR PC, [PC, #0x100] -> 0xA0000108
	assert(arch.decodeBranch4(0xA0000000, I({ 0x00, 0xF1, 0x9F, 0xE5 }), memory) == std::make_pair(true, pointer));

	// 00 F1 1F E5: LDR PC, [PC, #-0x100] -> 0xA0000008
	assert(arch.decodeBranch4(0xA0000100, I({ 0x00, 0xF1, 0x1F, 0xE5 }), memory) == std::make_pair(true, pointer));

	// 00 F0 FE FF: BL 0xA0001000; THUMB function pointer is 0xA0001001.
	data[0] = 0x00; data[1] = 0xF0; data[2] = 0xFE; data[3] = 0xFF;
	assert(arch.decodeBranchReference(0, memory) == std::make_tuple(true, 0xA0001001U, 4U));
	// 00 F0 FE EF: BLX 0xA0001000; ARM function pointer is 0xA0001000.
	data[0] = 0x00; data[1] = 0xF0; data[2] = 0xFE; data[3] = 0xEF;
	assert(arch.decodeBranchReference(0, memory) == std::make_tuple(true, 0xA0001000U, 4U));
}

static void testC166Decoder() {
	const Arch &arch = getArch(ARCH_C166);
	Pattern::Memory memory = { 0, nullptr, 0, 1, ARCH_C166 };

	// BB FF: CALLR 0x250100
	assert(arch.decodeBranch2(0x250100, I({ 0xBB, 0xFF }), memory) == std::make_pair(true, 0x250100));
	// 3D 03: JMPR cc_NE, 0x9B5942
	assert(arch.decodeBranch2(0x9B593A, I({ 0x3D, 0x03 }), memory) == std::make_pair(true, 0x9B5942));
	// 0D 10: JMPR cc_UC, 0x9B599A
	assert(arch.decodeBranch2(0x9B5978, I({ 0x0D, 0x10 }), memory) == std::make_pair(true, 0x9B599A));
	// 0D 00: JMPR cc_UC, 0x120000 (16-bit offset wrap)
	assert(arch.decodeBranch2(0x12FFFE, I({ 0x0D, 0x00 }), memory) == std::make_pair(true, 0x120000));
	// CC 00: NOP, not a branch
	assert(arch.decodeBranch2(0x250100, I({ 0xCC, 0x00 }), memory) == std::make_pair(false, 0U));

	// CA 00 56 34: CALLA cc_UC, 0x3456 -> 0x9A3456
	assert(arch.decodeBranch4(0x9A0100, I({ 0xCA, 0x00, 0x56, 0x34 }), memory) == std::make_pair(true, 0x9A3456));
	// EA 30 56 34: JMPA cc_NE, 0x3456 -> 0x9A3456
	assert(arch.decodeBranch4(0x9A0100, I({ 0xEA, 0x30, 0x56, 0x34 }), memory) == std::make_pair(true, 0x9A3456));
	// DA 9A 32 93: CALLS 0x9A, 0x9332 -> 0x9A9332
	assert(arch.decodeBranch4(0x259254, I({ 0xDA, 0x9A, 0x32, 0x93 }), memory) == std::make_pair(true, 0x9A9332));
	// DA A2 54 61: CALLS 0xA2, 0x6154 -> 0xA26154
	assert(arch.decodeBranch4(0x9B592E, I({ 0xDA, 0xA2, 0x54, 0x61 }), memory) == std::make_pair(true, 0xA26154));
	// FA 9A A0 93: JMPS 0x9A, 0x93A0 -> 0x9A93A0
	assert(arch.decodeBranch4(0x37F506, I({ 0xFA, 0x9A, 0xA0, 0x93 }), memory) == std::make_pair(true, 0x9A93A0));
	// E2 FE 56 34: PCALL R14, 0x3456 -> 0x9A3456
	assert(arch.decodeBranch4(0x9A0100, I({ 0xE2, 0xFE, 0x56, 0x34 }), memory) == std::make_pair(true, 0x9A3456));
	// 8A 00 FE F0: JB 0xFD00.15, 0x340100
	assert(arch.decodeBranch4(0x340100, I({ 0x8A, 0x00, 0xFE, 0xF0 }), memory) == std::make_pair(true, 0x340100));
	// 9A 00 FE F0: JNB 0xFD00.15, 0x340100
	assert(arch.decodeBranch4(0x340100, I({ 0x9A, 0x00, 0xFE, 0xF0 }), memory) == std::make_pair(true, 0x340100));
	// AA 00 FE F0: JBC 0xFD00.15, 0x340100
	assert(arch.decodeBranch4(0x340100, I({ 0xAA, 0x00, 0xFE, 0xF0 }), memory) == std::make_pair(true, 0x340100));
	// BA 00 FE F0: JNBS 0xFD00.15, 0x340100
	assert(arch.decodeBranch4(0x340100, I({ 0xBA, 0x00, 0xFE, 0xF0 }), memory) == std::make_pair(true, 0x340100));
	// 8A 00 00 FE: invalid JB encoding (low nibble of q0 must be zero)
	assert(arch.decodeBranch4(0x340100, I({ 0x8A, 0x00, 0x00, 0xFE }), memory) == std::make_pair(false, 0U));

	// 00 F5 37 AB: stored code pointer 0x37:F500; AB is padding.
	uint8_t pointerBytes[] = { 0x00, 0xF5, 0x37, 0xAB };
	Pattern::Memory pointerMemory = { 0x200000, pointerBytes, sizeof(pointerBytes), 1, ARCH_C166 };
	assert(Pattern::decodePointer(0x200000, pointerMemory) == std::make_pair(true, 0x37F500));
	auto pointerResults = Pattern::find(Pattern::parse("*(00F537AB)"), pointerMemory, 1);
	assert(pointerResults.size() == 1 && pointerResults[0].address == 0x37F500 &&
		pointerResults[0].offset == 0 && pointerResults[0].size == 4);

	uint8_t callBytes[] = {
		0xCA, 0x00, 0x08, 0x00,	// CALLA cc_UC, 0x0008 -> 0x900008
		0xCC, 0x00,			// NOP
		0xCC, 0x00,			// NOP
		0xDB, 0x00,			// RETS
		0xCC, 0x00,			// NOP
	};
	Pattern::Memory callMemory = { 0x900000, callBytes, sizeof(callBytes), 1, ARCH_C166 };
	assert(Pattern::decodeBranchReference(0, callMemory) == std::make_tuple(true, 0x900008U, 4U));
	auto branchResults = Pattern::find(Pattern::parse("&BL(CA000800)"), callMemory, 1);
	assert(branchResults.size() == 1 && branchResults[0].address == 0x900008 &&
		branchResults[0].offset == 0 && branchResults[0].size == 4);
	auto nestedBranch4 = Pattern::find(Pattern::parse("{ DB00 }"), callMemory, 1);
	assert(nestedBranch4.size() == 1 && nestedBranch4[0].address == 0x900000);

	uint8_t referenceBytes[] = {
		0xE6, 0xFC, 0x8C, 0x75,	// MOV R12, #0x758C; SOF(0x20758C)
		0xE6, 0xFD, 0x20, 0x00,	// MOV R13, #0x20; SEG(0x20758C)
		0xCC, 0x00,			// NOP
		0xCC, 0x00,			// NOP
		0xDB, 0x00,			// RETS at 0x20758C
	};
	Pattern::Memory referenceMemory = { 0x207580, referenceBytes, sizeof(referenceBytes), 1, ARCH_C166 };
	auto nestedReference = Pattern::find(Pattern::parse("LDR{ DB00 }"), referenceMemory, 1);
	assert(nestedReference.size() == 1 && nestedReference[0].address == 0x207580);

	uint8_t farReferenceBytes[] = {
		0xCC, 0x00,			// NOP; keep the MOV pair 2-byte aligned, not 4-byte aligned
		0xE6, 0xFC, 0x10, 0x00,	// MOV R12, #0x10; POF(0x228010)
		0xE6, 0xFD, 0x8A, 0x00,	// MOV R13, #0x8A; PAG(0x228010)
		0xCC, 0x00,			// NOP
		0xCC, 0x00,			// NOP
		0xCC, 0x00,			// NOP
		0xCC, 0x00,			// NOP
		0xCC, 0x00,			// NOP
		0xDB, 0x00,			// RETS at 0x228010
	};
	Pattern::Memory farReferenceMemory = { 0x227FFC, farReferenceBytes, sizeof(farReferenceBytes), 1, ARCH_C166 };
	auto nestedFarReference = Pattern::find(Pattern::parse("LDR{ DB00 }"), farReferenceMemory, 1);
	assert(nestedFarReference.size() == 1 && nestedFarReference[0].address == 0x227FFE);

	uint8_t shortBranchBytes[] = {
		0x0D, 0x01,	// JMPR cc_UC, 0x900004
		0xCC, 0x00,	// NOP
		0xDB, 0x00,	// RETS
	};
	Pattern::Memory shortBranchMemory = { 0x900000, shortBranchBytes, sizeof(shortBranchBytes), 1, ARCH_C166 };
	auto nestedBranch2 = Pattern::find(Pattern::parse("[ DB00 ]"), shortBranchMemory, 1);
	assert(nestedBranch2.size() == 1 && nestedBranch2[0].address == 0x900000);
	auto branch2Results = Pattern::find(Pattern::parse("&BL(0D01)"), shortBranchMemory, 1);
	assert(branch2Results.size() == 1 && branch2Results[0].address == 0x900004 && branch2Results[0].size == 2);

	// C166 offsets must not receive ARM's Thumb function marker.
	uint8_t patternBytes[] = {
		0x00, 0xB4,	// ADD R11, R4 (same bytes as a THUMB PUSH marker)
		0xCC, 0x00,	// NOP
	};
	auto pattern = Pattern::parse("00 B4");
	Pattern::Memory c166Memory = { 0x200000, patternBytes, sizeof(patternBytes), 1, ARCH_C166 };
	auto c166Results = Pattern::find(pattern, c166Memory, 1);
	assert(c166Results.size() == 1 && c166Results[0].address == 0x200000 && c166Results[0].size == 2);

}

static void testArmPattern() {
	uint8_t bytes[] = {
		0x00, 0x00, 0x9F, 0xE5,	// LDR R0, [PC, #0]
		0x00, 0xF0, 0x20, 0xE3,	// NOP
		0x10, 0x00, 0x00, 0xA0,	// Pointer to 0xA0000010
		0x00, 0xF0, 0x20, 0xE3,	// NOP
		0x00, 0xF0, 0x20, 0xE3,	// NOP at 0xA0000010
	};
	Pattern::Memory memory = { 0xA0000000, bytes, sizeof(bytes), 1, ARCH_ARM };
	auto results = Pattern::find(Pattern::parse("LDR{ 00F020E3 }"), memory, 1);
	assert(results.size() == 1 && results[0].address == 0xA0000000);
	auto referenceResults = Pattern::find(Pattern::parse("&(00009FE5)"), memory, 1);
	assert(referenceResults.size() == 1 && referenceResults[0].address == 0xA0000010 && referenceResults[0].offset == 0);
	auto fixedAddressResults = Pattern::find(Pattern::parse("<A0123456>"), memory, 1);
	assert(fixedAddressResults.size() == 1 && fixedAddressResults[0].address == 0xA0123456 && fixedAddressResults[0].size == 0);
	assert(Pattern::stringify(Pattern::parse("&BL(00F020E3)")) == "&BL(00 F0 20 E3)");
	assert(Pattern::stringify(Pattern::parse("<A0123456>")) == "< A0123456 >");
	assert(Pattern::stringify(Pattern::parse("LDR[ 00BF ] CC")) == "LDR[ 00 BF ] CC");

	// 00 F0 20 E3: NOP; a longer pattern must not overrun the fixture.
	uint8_t shortBytes[] = { 0x00, 0xF0, 0x20, 0xE3 };
	Pattern::Memory shortMemory = { 0xA0000000, shortBytes, sizeof(shortBytes), 1, ARCH_ARM };
	assert(Pattern::find(Pattern::parse("00F020E300F020E3"), shortMemory, 1).empty());
}

static void testThumbPattern() {
	uint8_t referenceBytes[] = {
		0x00, 0x48,			// LDR R0, [PC, #0]
		0x00, 0xBF,			// NOP
		0x08, 0x00, 0x00, 0xA0,	// Pointer to 0xA0000008
		0x00, 0xBF,			// NOP at 0xA0000008
	};
	Pattern::Memory referenceMemory = { 0xA0000000, referenceBytes, sizeof(referenceBytes), 1, ARCH_ARM };
	auto referenceResults = Pattern::find(Pattern::parse("LDR[ 00BF ]"), referenceMemory, 1);
	assert(referenceResults.size() == 1 && referenceResults[0].address == 0xA0000000);

	auto xrefs = Pattern::finXRefs(0xA0000008, referenceMemory, 1);
	assert(xrefs.size() == 1 && xrefs[0].type == RESULT_TYPE_REFERENCE &&
		xrefs[0].address == 0xA0000000 && xrefs[0].offset == 0 && xrefs[0].size == 2);

	uint8_t functionBytes[] = {
		0x10, 0xB5,	// PUSH {R4, LR}
		0x00, 0xBF,	// NOP
	};
	Pattern::Memory functionMemory = { 0xA0000000, functionBytes, sizeof(functionBytes), 1, ARCH_ARM };
	auto functionResults = Pattern::find(Pattern::parse("10 B5"), functionMemory, 1);
	assert(functionResults.size() == 1 && functionResults[0].address == 0xA0000001 && functionResults[0].size == 2);
	auto adjustedResults = Pattern::find(Pattern::parse("00 BF + 1"), functionMemory, 1);
	assert(adjustedResults.size() == 1 && adjustedResults[0].address == 0xA0000003 &&
		adjustedResults[0].offset == 2 && adjustedResults[0].size == 2);
}


int main() {
	testArmDecoder();
	testC166Decoder();
	testArmPattern();
	testThumbPattern();
	std::cout << "All tests passed.\n";
	return 0;
}
