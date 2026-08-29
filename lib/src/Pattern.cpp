#include "Pattern.h"
#include "Arch.h"
#include "Parser.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <cstdarg>
#include <string>
#include <inttypes.h>

#include "utils.h"

namespace Ptr89 {

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
		debug("Checking pattern: '%s' at %08" PRIu64 "X\n", stringify(pattern).c_str(), memory.base + offset);
		if (m_debugLevel == 0)
			debug("Memory: %08X %08" PRIu64 "X\n", memory.base, memory.size);
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

	if (offset > memory.size || static_cast<size_t>(patternSize) > memory.size - offset) {
		if (m_debugHandler)
			debug("FAIL: Address %08" PRIu64 "X is out of range.\n", memory.base + offset);
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

	const Arch &arch = getArch(memory.arch);
	for (auto it: pattern->subPatterns) {
		const SubPtrExp &p = it.second;
		uint32_t address = memory.base + offset + p.offset;
		const uint8_t *bytes = memory.data + offset + p.offset;
		std::vector<uint32_t> decoded;

		switch (p.type) {
			case SUB_PATTERN_TYPE_BRANCH_2B:
			{
				auto [success, branch] = arch.decodeBranch2(address, bytes, memory);
				if (success)
					decoded.push_back(branch);
			}
				break;
			case SUB_PATTERN_TYPE_BRANCH_4B:
			{
				auto [success, branch] = arch.decodeBranch4(address, bytes, memory);
				if (success)
					decoded.push_back(arch.resolveThunks(branch, memory));
			}
				break;
			case SUB_PATTERN_TYPE_LDR_2B:
				decoded = arch.decodeReferences2(address, bytes, memory);
				break;
			case SUB_PATTERN_TYPE_LDR_4B:
				decoded = arch.decodeReferences4(address, bytes, memory);
				break;
		}

		for (uint32_t target: decoded) {
			if (!inMemory(memory, target))
				continue;
			uint32_t fileOffset = target - memory.base - p.pattern->inputOffset;
			if (checkPattern(p.pattern, fileOffset, memory)) {
				debugSectionEnd();
				return true;
			}
		}
		if (decoded.empty() && m_debugHandler) {
			debug("FAIL: architecture instruction could not be decoded.\n");
		}
	}

	debugSectionEnd();

	return false;
}

std::pair<bool, uint32_t> Pattern::decodeReference(uint32_t offset, const Memory &memory) {
	return getArch(memory.arch).decodeReference(offset, memory);
}

std::pair<bool, uint32_t> Pattern::decodeBranchReference(uint32_t offset, const Memory &memory) {
	return getArch(memory.arch).decodeBranchReference(offset, memory);
}

std::pair<bool, uint32_t> Pattern::decodePointer(uint32_t addr, const Memory &memory) {
	debug("Try decoding pointer at %08X\n", addr);
	auto [success, value] = getArch(memory.arch).decodePointer(addr, memory);
	if (success) {
		debug("Pointer address: %08X\n", value);
		return { true, value };
	}
	debug("FAIL: address is out of memory range!\n");
	return { false, 0 };
}

uint32_t Pattern::resolveThunks(uint32_t addr, const Memory &memory) {
	return getArch(memory.arch).resolveThunks(addr, memory);
}

std::pair<bool, Pattern::SearchResult> Pattern::decodeResult(const std::shared_ptr<PtrExp> &pattern, uint32_t offset, const Memory &memory) {
	uint32_t address = memory.base + offset;

	switch (pattern->type) {
		case PATTERN_TYPE_OFFSET:
		{
			uint32_t value = getArch(memory.arch).offsetValue(address, offset, memory);
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

		case PATTERN_TYPE_BRANCH_REFERENCE:
		{
			auto [success, value] = decodeBranchReference(offset, memory);
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

int Pattern::findAlignForPattern(const std::shared_ptr<PtrExp> &pattern, int align, Architecture architecture) {
	if (pattern->type == PATTERN_TYPE_BRANCH_REFERENCE) {
		align = std::max(align, 2);
	} else if (pattern->type == PATTERN_TYPE_REFERENCE) {
		align = std::max(align, 2);
	}

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
			{
				int instructionAlign = getArch(architecture).reference4Align();
				if ((offset % instructionAlign) == 0)
					align = std::max(align, instructionAlign);
			}
			break;
		}
	}
	return align;
}

std::vector<Pattern::SearchResult> Pattern::find(const std::shared_ptr<PtrExp> &pattern, const Memory &memory, size_t maxResults) {
	int firstNonWildcardByte = 0;
	bool isTrulyWildcard = true;
	int patternSize = pattern->bytes.size();

	std::vector<SearchResult> searchResults;

	if (m_debugHandler) {
		debug("Searching pattern: %s\n", stringify(pattern).c_str());
		debug("Memory: %08X %08" PRIu64 "X\n", memory.base, memory.size);
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
	if (static_cast<size_t>(patternSize) > memory.size) {
		debug("FAIL: pattern is larger than memory!\n");
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
	int align = findAlignForPattern(pattern, memory.align, memory.arch);
	if (align != 1)
		firstNonWildcardByte = 0;

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
						debug("Possible result at %08" PRIu64 "X\n", memory.base + foundOffset);

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
								i -= align;
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
					debug("Possible result at %08" PRIu64 "X\n", memory.base + foundOffset);
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
							i -= align;
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

std::vector<Pattern::XRefSearchResult> Pattern::finXRefs(uint32_t addr, const Memory &memory, size_t maxResults) {
	debug("Searching XRef's for %08X\n", addr);
	std::vector<XRefSearchResult> searchResults;
	for (size_t i = 0; i < memory.size; i += 2) {
		auto [isReference, refAddr] = decodeReference(i, memory);
		auto [isBranchReference, branchAddr] = decodeBranchReference(i, memory);
		auto [isPointer, ptrAddr] = decodePointer(i + memory.base, memory);
		if (isBranchReference && (branchAddr & ~1) == (addr & ~1)) {
			debug("FOUND: branch call at %08" PRIu64 "X\n", i + memory.base);
			searchResults.push_back({ XREF_TYPE_BRANCH_CALL, static_cast<uint32_t>(memory.base + i), static_cast<uint32_t>(i) });
		} else if (isReference && (refAddr & ~1) == (addr & ~1)) {
			debug("FOUND: reference at %08" PRIu64 "X\n", i + memory.base);
			searchResults.push_back({ XREF_TYPE_REFERENCE, static_cast<uint32_t>(memory.base + i), static_cast<uint32_t>(i) });
		} else if (isPointer && (ptrAddr & ~1) == (addr & ~1)) {
			debug("FOUND: pointer at %08" PRIu64 "X\n", i + memory.base);
			searchResults.push_back({ XREF_TYPE_POINTER, static_cast<uint32_t>(memory.base + i), static_cast<uint32_t>(i) });
		}

		if (maxResults && searchResults.size() >= maxResults) {
			debug("Maximum search results are reached.\n");
			break;
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
