#include <cstdint>
#include <ptr89.h>
#include <cassert>
#include <cstdio>

using namespace Ptr89;

static inline uint8_t *I(const std::vector<uint8_t> &value) {
	static std::vector<uint8_t> tmp;
	tmp = value;
	return &tmp[0];
}

static void testArmDecoder() {
	// ARM BL #-offset
	assert(Pattern::decodeArmBL(0xA0001000, I({ 0xFE, 0xFB, 0xFF, 0x0B })) == std::make_pair(true, 0xA0000000));

	// ARM BLX #-offset
	assert(Pattern::decodeArmBL(0xA0001000, I({ 0xFE, 0xFB, 0xFF, 0xFA })) == std::make_pair(true, 0xA0000000));

	// ARM BL #+offset
	assert(Pattern::decodeArmBL(0xA0000000, I({ 0xFE, 0x03, 0x00, 0x0B })) == std::make_pair(true, 0xA0001000));

	// ARM BLX #+offset
	assert(Pattern::decodeArmBL(0xA0000000, I({ 0xFE, 0x03, 0x00, 0xFA })) == std::make_pair(true, 0xA0001000));

	// ARM BLX #-offset with H=1
	assert(Pattern::decodeArmBL(0xA0001000, I({ 0xFE, 0xFB, 0xFF, 0xFB })) == std::make_pair(true, 0xA0000002));

	// ARM BLX #+offset with H=1
	assert(Pattern::decodeArmBL(0xA0000000, I({ 0xFE, 0x03, 0x00, 0xFB })) == std::make_pair(true, 0xA0001002));

	// THUMB BL #-offset
	assert(Pattern::decodeThumbBL(0xA0001000, I({ 0xFE, 0xF7, 0xFE, 0xFF })) == std::make_pair(true, 0xA0000000));

	// THUMB BLX #-offset
	assert(Pattern::decodeThumbBL(0xA0001000, I({ 0xFE, 0xF7, 0xFE, 0xEF })) == std::make_pair(true, 0xA0000000));

	// THUMB BL #+offset
	assert(Pattern::decodeThumbBL(0xA0000000, I({ 0x00, 0xF0, 0xFE, 0xFF })) == std::make_pair(true, 0xA0001000));

	// THUMB BLX #+offset
	assert(Pattern::decodeThumbBL(0xA0000000, I({ 0x00, 0xF0, 0xFE, 0xEF })) == std::make_pair(true, 0xA0001000));

	// THUMB B #-offset
	assert(Pattern::decodeThumbB(0xA0000100, I({ 0x7E, 0xE7 })) == std::make_pair(true, 0xA0000000));

	// THUMB B #+offset
	assert(Pattern::decodeThumbB(0xA0000000, I({ 0x7E, 0xE0 })) == std::make_pair(true, 0xA0000100));

	// THUMB BEQ #-offset
	assert(Pattern::decodeThumbB(0xA0000010, I({ 0xF6, 0xD0 })) == std::make_pair(true, 0xA0000000));

	// THUMB BEQ #+offset
	assert(Pattern::decodeThumbB(0xA0000000, I({ 0x06, 0xD0 })) == std::make_pair(true, 0xA0000010));

	// THUMB LDR Rd, [PC, #offset]
	assert(Pattern::decodeThumbLDR(0xA0000000, I({ 0x16, 0x48 })) == std::make_pair(true, 0xA000005C));

	// THUMB LDR Rd, [PC, #offset]
	assert(Pattern::decodeThumbLDR(0xA0000002, I({ 0x16, 0x48 })) == std::make_pair(true, 0xA000005C));

	// ARM LDR Rd, [PC, #+offset]
	assert(Pattern::decodeArmLDR(0xA0000000, I({ 0x00, 0x01, 0x9F, 0xE5 })) == std::tuple(true, 0xA0000108, false));

	// ARM LDR Rd, [PC, #-offset]
	assert(Pattern::decodeArmLDR(0xA0000100, I({ 0x00, 0x01, 0x1F, 0xE5 })) == std::tuple(true, 0xA0000008, false));

	// ARM LDR PC, [PC, #+offset]
	assert(Pattern::decodeArmLDR(0xA0000000, I({ 0x00, 0xF1, 0x9F, 0xE5 })) == std::tuple(true, 0xA0000108, true));

	// ARM LDR PC, [PC, #-offset]
	assert(Pattern::decodeArmLDR(0xA0000100, I({ 0x00, 0xF1, 0x1F, 0xE5 })) == std::tuple(true, 0xA0000008, true));
}



int main() {
	Pattern::setDebugHandler(vprintf);
	testArmDecoder();
	printf("All tests passed.\n");
	return 0;
}
