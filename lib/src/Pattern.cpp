#include "Pattern.h"
#include "Parser.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <regex>
#include <stdexcept>
#include <cstdarg>
#include <string>

#include "utils.h"

namespace Ptr89 {

static const char *MNEMONICS[] = { "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "", "??" };
static const char *REGNAMES[] = { "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC" };

Pattern::DebugHandlerFunc Pattern::m_debugHandler = nullptr;
int Pattern::m_debugLevel = 0;

PatternError::PatternError(const Parser *parser, const std::string &msg): std::runtime_error(getErrorMsg(parser, msg)) {

}

std::string PatternError::getErrorMsg(const Parser *parser, const std::string &msg) {
	auto loc = parser->getLocation();
	return msg + " at line " + std::to_string(loc.first) + " column " + std::to_string(loc.second) + ".\n" + parser->getCodeFrame(loc);
}

std::shared_ptr<PtrExp> Pattern::parse(const std::string &pattern) {
	Parser parser;
	return parser.parse(pattern);
}

bool Pattern::fuzzyMatch(const uint8_t *bytes, const uint8_t *masks, int patternSize, const uint8_t *memory) {
	bool found = true;
	for (int j = 0; j < patternSize; j++) {
		uint8_t mask = masks[j];

		if (mask != 0x00) {
			uint8_t byte = bytes[j];
			uint8_t memoryByte = memory[j];

			if (mask == 0xFF) {
				if (byte != memoryByte) {
					found = false;
					break;
				}
			} else {
				if ((byte & mask) != (memoryByte & mask)) {
					found = false;
					break;
				}
			}
		}
	}
	return found;
}

bool Pattern::checkPattern(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory) {
	debugSectionBegin();
	if (m_debugHandler) {
		debug("Checking pattern: '%s' at %08lX\n", stringify(pattern).c_str(), memory.base + offset);
		if (m_debugLevel == 0)
			debug("Memory: %08X %08lX\n", memory.base, memory.size);
	}

	if (pattern->type == PATTERN_TYPE_STATIC_VALUE) {
		debug("Static value: %08X\n", pattern->staticValue);
		return true;
	}

	int patternSize = pattern->bytes.size();
	if (!patternSize) {
		debug("FAIL: empty pattern!\n");
		return false;
	}

	if (offset + patternSize >= memory.size) {
		if (m_debugHandler)
			debug("FAIL: Address %08lX is out of range.\n", memory.base + offset);
		debugSectionEnd();
		return false;
	}
	if (!fuzzyMatch(&pattern->bytes[0], &pattern->masks[0], patternSize, memory.data + offset)) {
		if (m_debugHandler)
			debug("FAIL: bytes not matched.\n");
		debugSectionEnd();
		return false;
	}
	if (!checkSubpatterns(pattern, offset, memory)) {
		if (m_debugHandler)
			debug("FAIL: sub patterns not matched.\n");
		debugSectionEnd();
		return false;
	}
	if (m_debugHandler)
		debug("Pattern matched!\n");
	debugSectionEnd();
	return true;
}

bool Pattern::checkSubpatterns(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory) {
	if (!pattern->subPatterns.size())
		return true;

	if (m_debugHandler)
		debug("Checking sub patterns...\n");

	debugSectionBegin();

	for (auto it: pattern->subPatterns) {
		const SubPtrExp &p = it.second;

		switch (p.type) {
			case SUB_PATTERN_TYPE_BRANCH_2B:
			{
				if (m_debugHandler)
					debug("Decoding THUMB B at %08lX\n", memory.base + offset + p.offset);

				auto [isThumb, thumbAddr] = decodeThumbB(memory.base + offset + p.offset, memory.data + offset + p.offset);
				if (isThumb && inMemory(memory, thumbAddr, 4)) {
					uint32_t fileOffset = thumbAddr - memory.base - p.pattern->inputOffset;
					if (checkPattern(p.pattern, fileOffset, memory)) {
						debugSectionEnd();
						return true;
					}
				} else {
					if (m_debugHandler)
						debug("FAIL: not instruction!\n");
				}
			}
			break;

			case SUB_PATTERN_TYPE_BRANCH_4B:
			{
				if (m_debugHandler)
					debug("Try decoding THUMB BL/BLX at %08lX\n", memory.base + offset + p.offset);

				auto [isThumb, thumbAddr] = decodeThumbBL(memory.base + offset + p.offset, memory.data + offset + p.offset);
				if (isThumb && inMemory(memory, thumbAddr, 4)) {
					thumbAddr = resolveThrunks(thumbAddr, memory);
					uint32_t fileOffset = thumbAddr - memory.base - p.pattern->inputOffset;
					if (checkPattern(p.pattern, fileOffset, memory)) {
						debugSectionEnd();
						return true;
					}
				} else {
					if (m_debugHandler)
						debug("FAIL: not instruction!\n");
				}

				if (m_debugHandler)
					debug("Try decoding ARM B/BL/BLX at %08lX\n", memory.base + offset + p.offset);

				auto [isArm, armAddr] = decodeArmBL(memory.base + offset + p.offset, memory.data + offset + p.offset);
				if (isArm && inMemory(memory, armAddr, 4)) {
					armAddr = resolveThrunks(armAddr, memory);
					uint32_t fileOffset = armAddr - memory.base - p.pattern->inputOffset;
					if (checkPattern(p.pattern, fileOffset, memory)) {
						debugSectionEnd();
						return true;
					}
				} else {
					if (m_debugHandler)
						debug("FAIL: not instruction!\n");
				}

				if (m_debugHandler)
					debug("Try decoding ARM THRUNK at %08lX\n", memory.base + offset + p.offset);

				auto [isArmLdr, armLDR, isThrunk] = decodeArmLDR(memory.base + offset + p.offset, memory.data + offset + p.offset);
				if (isArmLdr && isThrunk) {
					auto [success, ptrAddr] = decodePointer(armLDR, memory);
					if (success) {
						ptrAddr = resolveThrunks(ptrAddr, memory);
						uint32_t fileOffset = ptrAddr - memory.base - p.pattern->inputOffset;
						if (checkPattern(p.pattern, fileOffset, memory)) {
							debugSectionEnd();
							return true;
						}
					} else {
						if (m_debugHandler)
							debug("FAIL: invalid pointer!\n");
					}
				} else {
					if (m_debugHandler)
						debug("FAIL: not instruction!\n");
				}
			}
			break;

			case SUB_PATTERN_TYPE_LDR_2B:
			{
				if (m_debugHandler)
					debug("Try decoding THUMB LDR at %08lX\n", memory.base + offset + p.offset);

				auto [isThumbLdr, thumbLdrAddr] = decodeThumbLDR(memory.base + offset + p.offset, memory.data + offset + p.offset);
				if (isThumbLdr) {
					auto [success, ptrAddr] = decodePointer(thumbLdrAddr, memory);
					if (success) {
						uint32_t fileOffset = ptrAddr - memory.base - p.pattern->inputOffset;
						if (checkPattern(p.pattern, fileOffset, memory)) {
							debugSectionEnd();
							return true;
						}
					} else {
						if (m_debugHandler)
							debug("FAIL: invalid pointer!\n");
					}
				} else {
					if (m_debugHandler)
						debug("FAIL: not instruction!\n");
				}
			}
			break;

			case SUB_PATTERN_TYPE_LDR_4B:
			{
				if (m_debugHandler)
					debug("Try decoding ARM LDR at %08lX\n", memory.base + offset + p.offset);

				auto [isArmLdr, armLdrAddr, isArmThrunk] = decodeArmLDR(memory.base + offset + p.offset, memory.data + offset + p.offset);
				if (isArmLdr) {
					auto [success, ptrAddr] = decodePointer(armLdrAddr, memory);
					if (success) {
						uint32_t fileOffset = ptrAddr - memory.base - p.pattern->inputOffset;
						if (checkPattern(p.pattern, fileOffset, memory)) {
							debugSectionEnd();
							return true;
						}
					} else {
						if (m_debugHandler)
							debug("FAIL: invalid pointer!\n");
					}
				} else {
					if (m_debugHandler)
						debug("FAIL: not instruction!\n");
				}
			}
			break;
		}
	}

	debugSectionEnd();

	return false;
}

std::pair<bool, uint32_t> Pattern::decodeReference(uint32_t offset, const Memory &memory) {
	offset &= ~1;

	debug("Try decoding ARM LDR at %08X\n", memory.base + offset);
	auto [isARM, armLDR, isArmThrunk] = decodeArmLDR(memory.base + offset, memory.data + offset);
	if (isARM) {
		auto [success, addr] = decodePointer(armLDR, memory);
		if (success)
			return { true, addr };
	}
	debug("FAIL: not instruction!\n");

	debug("Try decoding THUMB LDR at %08X\n", memory.base + offset);
	auto [isThumb, thumbLDR] = decodeThumbLDR(memory.base + offset, memory.data + offset);
	if (isThumb) {
		auto [success, addr] = decodePointer(thumbLDR, memory);
		if (success)
			return { true, addr };
	}
	debug("FAIL: not instruction!\n");

	return { false, 0 };
}

std::pair<bool, uint32_t> Pattern::decodePointer(uint32_t addr, const Memory &memory) {
	debug("Try decoding pointer at %08X\n", addr);
	if (inMemory(memory, addr, 4)) {
		uint32_t value = *reinterpret_cast<const uint32_t *>(memory.data + (addr - memory.base));
		debug("Pointer address: %08X\n", value);
		return { true, value };
	} else {
		debug("FAIL: address is out of memory range!\n");
	}
	return { false, 0 };
}

uint32_t Pattern::resolveThrunks(uint32_t addr, const Memory &memory) {
	if (inMemory(memory, addr, 4)) {
		auto [isArmLdr, ldrAddr, isThrunk] = decodeArmLDR(addr, memory.data + (addr - memory.base));
		if (isThrunk && inMemory(memory, ldrAddr)) {
			uint32_t value = *reinterpret_cast<const uint32_t *>(memory.data + (ldrAddr - memory.base));
			if (inMemory(memory, value)) {
				debug("Found thrunk at %08X: PC->%08X\n", addr, value);
				return resolveThrunks(value, memory);
			}
		}
	}
	return addr;
}

std::pair<bool, Pattern::SearchResult> Pattern::decodeResult(const std::shared_ptr<PtrExp> &pattern, uint32_t offset, const Memory &memory) {
	uint32_t address = memory.base + offset;

	switch (pattern->type) {
		case PATTERN_TYPE_OFFSET:
		{
			uint32_t value = address;
			if ((address & 1) == 0 && inMemory(memory, address, 4)) {
				uint16_t instr = *reinterpret_cast<const uint16_t *>(memory.data + offset);
				if ((instr & 0xFE00) == 0xB400) // PUSH
					value |= 1;
			}
			return { true, { address, offset, value } };
		}
		break;

		case PATTERN_TYPE_REFERENCE:
		{
			auto [success, value] = decodeReference(offset, memory);
			if (success)
				return { true, { address, offset, value + pattern->outputOffset } };
		}
		break;

		case PATTERN_TYPE_POINTER:
		{
			auto [success, value] = decodePointer(offset + memory.base, memory);
			if (success)
				return { true, { address, offset, value + pattern->outputOffset } };
		}
		break;

		case PATTERN_TYPE_STATIC_VALUE:
			return { true, { 0, 0, pattern->staticValue } };
		break;
	}
	return { false, { } };
}

int Pattern::findAlignForPattern(const std::shared_ptr<PtrExp> &pattern, int align) {
	for (auto &v: pattern->subPatterns) {
		int offset = v.first;
		auto &sp = v.second;
		switch (sp.type) {
			case SUB_PATTERN_TYPE_BRANCH_2B:
				if ((offset % 2) == 0)
					align = std::max(align, 2);
			break;

			case SUB_PATTERN_TYPE_BRANCH_4B:
				if ((offset % 2) == 0)
					align = std::max(align, 2);
			break;

			case SUB_PATTERN_TYPE_LDR_2B:
				if ((offset % 2) == 0)
					align = std::max(align, 2);
			break;

			case SUB_PATTERN_TYPE_LDR_4B:
				if ((offset % 4) == 0)
					align = std::max(align, 4);
			break;
		}
	}
	return align;
}

std::vector<Pattern::SearchResult> Pattern::find(const std::shared_ptr<PtrExp> &pattern, const Memory &memory, int maxResults) {
	int firstNonWildcardByte = 0;
	bool isTrulyWildcard = true;
	int patternSize = pattern->bytes.size();

	std::vector<SearchResult> searchResults;

	if (m_debugHandler) {
		debug("Searching pattern: %s\n", stringify(pattern).c_str());
		debug("Memory: %08X %08lX\n", memory.base, memory.size);
		debug("\n");
	}

	if (pattern->type == PATTERN_TYPE_STATIC_VALUE) {
		debug("Static value: %08X\n", pattern->staticValue);
		debug("\n");
		searchResults.push_back({ 0, 0, pattern->staticValue });
		return searchResults;
	}

	if (!patternSize) {
		debug("FAIL: empty pattern!\n");
		return searchResults;
	}

	// Wildcard optimization
	for (int i = 0; i < patternSize; i++) {
		if (pattern->masks[i] != 0x00) {
			firstNonWildcardByte = i;
			isTrulyWildcard = false;
			break;
		}
	}

	// Align optimization
	int align = findAlignForPattern(pattern, memory.align);
	debug("Search align: %d\n", align);

	/*
	 * Optimized variant of checkPattern().
	 */
	auto *masks = &pattern->masks[firstNonWildcardByte];
	auto *bytes = &pattern->bytes[firstNonWildcardByte];
	int size = patternSize - firstNonWildcardByte;

	if (size >= 4 && !isTrulyWildcard) { // faster
		debug("Using fast pattern matching algorithm.\n");

		uint32_t mask = *reinterpret_cast<uint32_t *>(masks);
		uint32_t searchValue = *reinterpret_cast<uint32_t *>(bytes) & mask;

		debug("Search prefix: mask=%08X, searchValue=%08X\n", mask, searchValue);
		debug("\n");

		for (size_t i = firstNonWildcardByte; i < memory.size - patternSize + 1; i += align) {
			uint32_t memoryValue = *reinterpret_cast<const uint32_t *>(memory.data + i);
			if ((memoryValue & mask) == searchValue) {
				if (size == 4 || fuzzyMatch(bytes + 4, masks + 4, size -  4, memory.data + i + 4)) {
					size_t foundOffset = i - firstNonWildcardByte;

					if (m_debugHandler)
						debug("Possible result at %08lX\n", memory.base + foundOffset);

					if (checkSubpatterns(pattern, foundOffset, memory)) {
						auto [isDecoded, result] = decodeResult(pattern, foundOffset + pattern->inputOffset, memory);
						if (isDecoded) {
							searchResults.push_back(result);

							if (m_debugHandler) {
								debug("FOUND: address=%08X, offset=%08X, value=%08X\n", result.address, result.offset, result.value);
								debug("\n");
							}

							if (maxResults && searchResults.size() >= maxResults) {
								debug("Maximum search results are reached.\n");
								break;
							}

							if (align == 1) {
								i += size - 1;
							} else {
								i += size;
								if ((i % align) != 0)
									i += align - (i % align);
								i--;
							}
						} else {
							debug("FAIL: can't decode result!\n");
							debug("\n");
						}
					} else {
						if (m_debugHandler) {
							debug("FAIL: sub patterns not matched.\n");
							debug("\n");
						}
					}
				}
			}
		}
	} else {
		debug("Using slow pattern matching algorithm.\n");
		debug("\n");

		for (size_t i = firstNonWildcardByte; i < memory.size - patternSize + 1; i += align) {
			if (fuzzyMatch(bytes, masks, size, memory.data + i)) {
				size_t foundOffset = i - firstNonWildcardByte;
				if (m_debugHandler)
					debug("Possible result at %08lX\n", memory.base + foundOffset);
				if (checkSubpatterns(pattern, foundOffset, memory)) {
					auto [isDecoded, result] = decodeResult(pattern, foundOffset + pattern->inputOffset, memory);
					if (isDecoded) {
						searchResults.push_back(result);

						if (m_debugHandler) {
							debug("FOUND: address=%08X, offset=%08X, value=%08X\n", result.address, result.offset, result.value);
							debug("\n");
						}

						if (maxResults && searchResults.size() >= maxResults) {
							debug("Maximum search results are reached.\n");
							break;
						}

						if (align == 1) {
							i += size - 1;
						} else {
							i += size;
							if ((i % align) != 0)
								i += align - (i % align);
							i--;
						}
					} else {
						debug("FAIL: can't decode result!\n");
						debug("\n");
					}
				} else {
					if (m_debugHandler) {
						debug("FAIL: sub patterns not matched.\n");
						debug("\n");
					}
				}
			}
		}
	}

	return searchResults;
}

std::string Pattern::stringify(const std::shared_ptr<PtrExp> &pattern) {
	std::string patternText;

	if (pattern->type == PATTERN_TYPE_REFERENCE) {
		patternText += "&(";
	} else if (pattern->type == PATTERN_TYPE_POINTER) {
		patternText += "*(";
	}

	int patternSize = pattern->bytes.size();
	std::vector<std::string> tmp;

	for (int i = 0; i < patternSize; i++) {
		uint8_t mask = pattern->masks[i];
		uint8_t byte = pattern->bytes[i];

		if (pattern->subPatterns.find(i) != pattern->subPatterns.end()) {
			auto &p = pattern->subPatterns[i];
			if (p.type == SUB_PATTERN_TYPE_BRANCH_2B) {
				tmp.push_back("[ " + stringify(p.pattern) + " ]");
			} else if (p.type == SUB_PATTERN_TYPE_BRANCH_4B) {
				tmp.push_back("{ " + stringify(p.pattern) + " }");
			} else if (p.type == SUB_PATTERN_TYPE_LDR_2B) {
				tmp.push_back("LDR[ " + stringify(p.pattern) + " ]");
			} else if (p.type == SUB_PATTERN_TYPE_LDR_4B) {
				tmp.push_back("LDR{ " + stringify(p.pattern) + " }");
			}
			i += p.size;
		} else {
			if (mask == 0x00) {
				tmp.push_back("??");
			} else if (mask == 0x0F) {
				tmp.push_back(strprintf("?%X", byte & 0x0F));
			} else if (mask == 0xF0) {
				tmp.push_back(strprintf("%X?", (byte & 0xF0) >> 4));
			} else if (mask == 0xFF) {
				tmp.push_back(strprintf("%02X", byte));
			} else {
				char bin[9] = {};
				for (int i = 0; i < 8; i++) {
					int bit = 1 << (7 - i);
					if ((mask & bit) == 0) {
						bin[i] = '.';
					} else if ((byte & bit)) {
						bin[i] = '1';
					} else {
						bin[i] = '0';
					}
				}
				tmp.push_back(strprintf("[%s]", bin));
			}
		}
	}

	if (pattern->inputOffset != 0)
		tmp.push_back(strprintf("%c 0x%X", pattern->inputOffset < 0 ? '-' : '+', abs(pattern->inputOffset)));

	patternText += strJoin(" ", tmp);

	if (pattern->type == PATTERN_TYPE_REFERENCE || pattern->type == PATTERN_TYPE_POINTER) {
		patternText += ")";
	}

	if (pattern->outputOffset != 0)
		patternText += strprintf(" %c 0x%X", pattern->inputOffset < 0 ? '-' : '+', abs(pattern->outputOffset));

	return patternText;
}

std::pair<bool, uint32_t> Pattern::decodeThumbBL(uint32_t offset, const uint8_t *bytes) {
	uint16_t thumb_instr1 = (bytes[1] << 8) | bytes[0];
	uint16_t thumb_instr2 = (bytes[3] << 8) | bytes[2];

	if ((offset % 2) != 0)
		return { false, 0 };

	if ((thumb_instr1 & 0xF800) == 0xF000 && (thumb_instr2 & 0xF800) == 0xE800) {
		int32_t offset11_a = (int32_t) (signExtend(thumb_instr1 & 0x7FF, 11, 32) << 12);
		uint32_t offset11_b = (thumb_instr2 & 0x7FF) << 1;
		uint32_t addr = (offset + 4 + offset11_a + offset11_b) & 0xFFFFFFFC;
		debug("%08X: %02X %02X %02X %02X  BLX #0x%08X\n", offset, bytes[0], bytes[1], bytes[2], bytes[3], addr);
		return { true, addr };
	} else if ((thumb_instr1 & 0xF800) == 0xF000 && (thumb_instr2 & 0xF800) == 0xF800) {
		int32_t offset11_a = (int32_t) (signExtend(thumb_instr1 & 0x7FF, 11, 32) << 12);
		uint32_t offset11_b = (thumb_instr2 & 0x7FF) << 1;
		uint32_t addr = (offset + 4 + offset11_a + offset11_b);
		debug("%08X: %02X %02X %02X %02X  BL #0x%08X\n", offset, bytes[0], bytes[1], bytes[2], bytes[3], addr);
		return { true, addr };
	}

	return { false, 0 };
}

std::pair<bool, uint32_t> Pattern::decodeArmBL(uint32_t offset, const uint8_t *bytes) {
	uint32_t instr = (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];

	if ((offset % 4) != 0)
		return { false, 0 };

	if (((instr & 0xFE000000) == 0xFA000000)) {
		int32_t offset24 = (int32_t) (signExtend(instr & 0xFFFFFF, 24, 30) << 2U);
		uint32_t H = (instr & 0x01000000) != 0 ? 1 : 0;
		uint32_t addr = (offset + 8 + offset24) + (H << 1);
		debug("%08X: %02X %02X %02X %02X  BLX #0x%08X\n", offset, bytes[0], bytes[1], bytes[2], bytes[3], addr);
		return { true, addr };
	} else if (((instr & 0x0F000000) == 0x0B000000) || ((instr & 0x0F000000) == 0x0A000000)) {
		int32_t offset24 = (int32_t) (signExtend(instr & 0xFFFFFF, 24, 30) << 2U);
		uint32_t addr = (offset + 8 + offset24);
		if (m_debugHandler) {
			uint32_t cond = (instr & 0xF0000000) >> 28;
			uint32_t L = (instr & 0x0F000000) == 0x0B000000;
			debug("%08X: %02X %02X %02X %02X  B%s%s #0x%08X\n", offset, bytes[0], bytes[1], bytes[2], bytes[3], L ? "L" : "", MNEMONICS[cond], addr);
		}
		return { true, addr };
	}

	return { false, 0 };
}

std::pair<bool, uint32_t> Pattern::decodeThumbB(uint32_t offset, const uint8_t *bytes) {
	uint16_t instr = (bytes[1] << 8) | bytes[0];

	if ((offset % 2) != 0)
		return { false, 0 };

	if ((instr & 0xF800) == 0xE000) {
		int32_t offset11 = (int32_t) (signExtend(instr & 0x7FF, 11, 32) << 1);
		uint32_t addr = offset + 4 + offset11;
		debug("%08X: %02X %02X        B #0x%08X\n", offset, bytes[0], bytes[1], addr);
		return { true, addr };
	} else if ((instr & 0xF000) == 0xD000) {
		int32_t offset8 = (int32_t) (signExtend(instr & 0xFF, 8, 32) << 1);
		uint32_t addr = offset + 4 + offset8;
		if (m_debugHandler) {
			uint32_t cond = (instr & 0x0F00) >> 8;
			debug("%08X: %02X %02X        B%s #0x%08X\n", offset, bytes[0], bytes[1], MNEMONICS[cond], addr);
		}
		return { true, addr };
	}

	return { false, 0 };
}

std::pair<bool, uint32_t> Pattern::decodeThumbLDR(uint32_t offset, const uint8_t *bytes) {
	uint16_t instr1 = (bytes[1] << 8) | bytes[0];
	uint16_t instr2 = (bytes[3] << 8) | bytes[2];

	if ((offset % 2) != 0)
		return { false, 0 };

	if ((instr1 & 0xF800) == 0x4800) {
		uint32_t instr1_offset8 = (instr1 & 0xFF) << 2;
		uint32_t instr1_Rd = (instr1 & 0x700) >> 8;
		uint32_t addr = offset + (offset % 4 == 0 ? 4 : 2) + instr1_offset8;
		debug("%08X: %02X %02X        LDR %s, [PC, #0x%X] ; 0x%08X\n", offset, bytes[0], bytes[1], REGNAMES[instr1_Rd], instr1_offset8, addr);
		return { true, addr };
	}

	return { false, 0 };
}

std::tuple<bool, uint32_t, bool> Pattern::decodeArmLDR(uint32_t offset, const uint8_t *bytes) {
	uint32_t instr = (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];

	if ((offset % 4) != 0)
		return { false, 0, false };

	if ((instr & 0xE0F0000) == 0x40F0000) { // Immediate offset/index
		int32_t U = (instr & (1 << 23)) != 0;
		int32_t offset_12 = U ? (instr & 0xFFFU) : -(instr & 0xFFFU);
		uint32_t addr = offset + 8 + offset_12;
		uint32_t Rd = (instr & 0xF000) >> 12;

		if (m_debugHandler) {
			uint32_t cond = (instr & 0xF0000000) >> 28;
			debug("%08X: %02X %02X %02X %02X  LDR%s %s, [PC, #%c0x%X] ; 0x%08X\n", offset, bytes[0], bytes[1], bytes[2], bytes[3],
					MNEMONICS[cond], REGNAMES[Rd], U ? '+' : '-', abs(offset_12), addr);
		}

		return { true, addr, Rd == 0xF };
	}

	return { false, 0, false };
}

void Pattern::debug(const char *format, ...) {
	if (!m_debugHandler)
		return;

	for (int i = 0; i < m_debugLevel; i++)
		_debug("    ");

	va_list v;
	va_start(v, format);
	m_debugHandler(format, v);
	va_end(v);
}

void Pattern::_debug(const char *format, ...) {
	va_list v;
	va_start(v, format);
	m_debugHandler(format, v);
	va_end(v);
}

}; // namespace Ptr89
