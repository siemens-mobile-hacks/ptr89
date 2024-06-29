#include <ptr89.h>
#include <cassert>
#include <cstdio>

using namespace Ptr89;

void testArmDecoder() {
	// ARM BL #-offset
	uint8_t instr1[] = { 0xFE, 0xFB, 0xFF, 0x0B };
	assert(Pattern::decodeArmBL(0xA0001000, instr1) == std::make_pair(true, 0xA0000000));

	// ARM BLX #-offset
	uint8_t instr2[] = { 0xFE, 0xFB, 0xFF, 0xFA };
	assert(Pattern::decodeArmBL(0xA0001000, instr2) == std::make_pair(true, 0xA0000000));

	// ARM BL #+offset
	uint8_t instr3[] = { 0xFE, 0x03, 0x00, 0x0B };
	assert(Pattern::decodeArmBL(0xA0000000, instr3) == std::make_pair(true, 0xA0001000));

	// ARM BLX #+offset
	uint8_t instr4[] = { 0xFE, 0x03, 0x00, 0xFA };
	assert(Pattern::decodeArmBL(0xA0000000, instr4) == std::make_pair(true, 0xA0001000));

	// ARM BLX #-offset with H=1
	uint8_t instr5[] = { 0xFE, 0xFB, 0xFF, 0xFB };
	assert(Pattern::decodeArmBL(0xA0001000, instr5) == std::make_pair(true, 0xA0000002));

	// ARM BLX #+offset with H=1
	uint8_t instr6[] = { 0xFE, 0x03, 0x00, 0xFB };
	assert(Pattern::decodeArmBL(0xA0000000, instr6) == std::make_pair(true, 0xA0001002));

	// THUMB BL #-offset
	uint8_t instr7[] = { 0xFE, 0xF7, 0xFE, 0xFF };
	assert(Pattern::decodeThumbBL(0xA0001000, instr7) == std::make_pair(true, 0xA0000000));

	// THUMB BLX #-offset
	uint8_t instr8[] = { 0xFE, 0xF7, 0xFE, 0xEF };
	assert(Pattern::decodeThumbBL(0xA0001000, instr8) == std::make_pair(true, 0xA0000000));

	// THUMB BL #+offset
	uint8_t instr9[] = { 0x00, 0xF0, 0xFE, 0xFF };
	assert(Pattern::decodeThumbBL(0xA0000000, instr9) == std::make_pair(true, 0xA0001000));

	// THUMB BLX #+offset
	uint8_t instr10[] = { 0x00, 0xF0, 0xFE, 0xEF };
	assert(Pattern::decodeThumbBL(0xA0000000, instr10) == std::make_pair(true, 0xA0001000));

	// THUMB B #-offset
	uint8_t instr11[] = { 0x7E, 0xE7 };
	assert(Pattern::decodeThumbB(0xA0000100, instr11) == std::make_pair(true, 0xA0000000));

	// THUMB B #+offset
	uint8_t instr12[] = { 0x7E, 0xE0 };
	assert(Pattern::decodeThumbB(0xA0000000, instr12) == std::make_pair(true, 0xA0000100));

	// THUMB BEQ #-offset
	uint8_t instr13[] = { 0xF6, 0xD0 };
	assert(Pattern::decodeThumbB(0xA0000010, instr13) == std::make_pair(true, 0xA0000000));

	// THUMB BEQ #+offset
	uint8_t instr14[] = { 0x06, 0xD0 };
	assert(Pattern::decodeThumbB(0xA0000000, instr14) == std::make_pair(true, 0xA0000010));

	// THUMB LDR Rd, [PC, #offset]
	uint8_t instr15[] = { 0x16, 0x48 };
	assert(Pattern::decodeThumbLDR(0xA0000000, instr15) == std::make_pair(true, 0xA000005C));

	// ARM LDR Rd, [PC, #+offset]
	uint8_t instr16[] = { 0x00, 0x01, 0x9F, 0xE5 };
	assert(Pattern::decodeArmLDR(0xA0000000, instr16) == std::make_pair(true, 0xA0000108));

	// ARM LDR Rd, [PC, #-offset]
	uint8_t instr17[] = { 0x00, 0x01, 0x1F, 0xE5 };
	assert(Pattern::decodeArmLDR(0xA0000100, instr17) == std::make_pair(true, 0xA0000008));
}

int main() {
	Pattern::setDebugHandler(vprintf);
	testArmDecoder();
	printf("All tests passed.\n");
	return 0;
}
