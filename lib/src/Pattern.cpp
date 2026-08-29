#include "Pattern.h"
#include "Arch.h"
#include "Parser.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <spdlog/spdlog.h>

#include "utils.h"

namespace Ptr89 {

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

const char *Pattern::getResultTypeName(ResultType type) {
	switch (type) {
		case RESULT_TYPE_OFFSET:
			return "offset";
		case RESULT_TYPE_POINTER:
			return "pointer";
		case RESULT_TYPE_REFERENCE:
			return "reference";
		case RESULT_TYPE_BRANCH:
			return "branch";
		case RESULT_TYPE_ADDRESS:
			return "address";
	}
	throw std::invalid_argument("Invalid result type.");
}

const char *Pattern::getSearchTypeName(ResultType type) {
	return type == RESULT_TYPE_OFFSET || type == RESULT_TYPE_ADDRESS ?
		"address" :
		getResultTypeName(type);
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
	bool debug = spdlog::should_log(spdlog::level::debug);
	if (debug)
		spdlog::debug("Checking pattern: '{}' at {:08X}", stringify(pattern), memory.base + offset);

	if (pattern->type == RESULT_TYPE_ADDRESS) {
		if (debug)
			spdlog::debug("Fixed address: {:08X}", pattern->fixedAddress);
		return true;
	}

	int patternSize = pattern->bytes.size();
	if (!patternSize) {
		if (debug)
			spdlog::debug("FAIL: empty pattern!");
		return false;
	}

	if (offset > memory.size || static_cast<size_t>(patternSize) > memory.size - offset) {
		if (debug)
			spdlog::debug("FAIL: Address {:08X} is out of range.", memory.base + offset);
		return false;
	}
	if (!fuzzyMatch(&pattern->bytes[0], &pattern->masks[0], patternSize, memory.data + offset)) {
		if (debug)
			spdlog::debug("FAIL: bytes not matched.");
		return false;
	}
	if (!checkSubpatterns(pattern, offset, memory)) {
		if (debug)
			spdlog::debug("FAIL: sub patterns not matched.");
		return false;
	}
	if (debug)
		spdlog::debug("Pattern matched!");
	return true;
}

bool Pattern::checkSubpatterns(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory) {
	if (!pattern->subPatterns.size())
		return true;

	bool debug = spdlog::should_log(spdlog::level::debug);
	if (debug)
		spdlog::debug("Checking sub patterns...");

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
			if (checkPattern(p.pattern, fileOffset, memory))
				return true;
		}
		if (decoded.empty() && debug)
			spdlog::debug("FAIL: architecture instruction could not be decoded.");
	}

	return false;
}

std::tuple<bool, uint32_t, size_t> Pattern::decodeReference(uint32_t offset, const Memory &memory) {
	return getArch(memory.arch).decodeReference(offset, memory);
}

std::tuple<bool, uint32_t, size_t> Pattern::decodeBranchReference(uint32_t offset, const Memory &memory) {
	return getArch(memory.arch).decodeBranchReference(offset, memory);
}

std::pair<bool, uint32_t> Pattern::decodePointer(uint32_t addr, const Memory &memory) {
	bool debug = spdlog::should_log(spdlog::level::debug);
	if (debug)
		spdlog::debug("Try decoding pointer at {:08X}", addr);
	auto [success, value] = getArch(memory.arch).decodePointer(addr, memory);
	if (success) {
		if (debug)
			spdlog::debug("Pointer address: {:08X}", value);
		return { true, value };
	}
	if (debug)
		spdlog::debug("FAIL: address is out of memory range!");
	return { false, 0 };
}

uint32_t Pattern::resolveThunks(uint32_t addr, const Memory &memory) {
	return getArch(memory.arch).resolveThunks(addr, memory);
}

std::pair<bool, Pattern::SearchResult> Pattern::decodeResult(const std::shared_ptr<PtrExp> &pattern, uint32_t matchOffset, const Memory &memory) {
	uint32_t offset = matchOffset + pattern->inputOffset;
	uint32_t address = memory.base + offset;

	switch (pattern->type) {
		case RESULT_TYPE_OFFSET:
		{
			uint32_t value = getArch(memory.arch).offsetValue(address, offset, memory);
			return { true, { value, matchOffset, pattern->bytes.size() } };
		}
		break;

		case RESULT_TYPE_REFERENCE:
		{
			auto [success, value, size] = decodeReference(offset, memory);
			if (success)
				return { true, { value + pattern->outputOffset, offset, size } };
		}
		break;

		case RESULT_TYPE_BRANCH:
		{
			auto [success, value, size] = decodeBranchReference(offset, memory);
			if (success)
				return { true, { value + pattern->outputOffset, offset, size } };
		}
		break;

		case RESULT_TYPE_POINTER:
		{
			auto [success, value] = decodePointer(offset + memory.base, memory);
			if (success)
				return { true, { value + pattern->outputOffset, offset, 4 } };
		}
		break;

		case RESULT_TYPE_ADDRESS:
			return { true, { pattern->fixedAddress, 0, 0 } };
		break;
	}
	return { false, { } };
}

int Pattern::findAlignForPattern(const std::shared_ptr<PtrExp> &pattern, int align, Architecture architecture) {
	if (pattern->type == RESULT_TYPE_BRANCH) {
		align = std::max(align, 2);
	} else if (pattern->type == RESULT_TYPE_REFERENCE) {
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
	bool debug = spdlog::should_log(spdlog::level::debug);

	if (debug) {
		spdlog::debug("Searching pattern: {}", stringify(pattern));
		spdlog::debug("Memory: {:08X} {:08X}", memory.base, memory.size);
	}

	if (pattern->type == RESULT_TYPE_ADDRESS) {
		if (debug)
			spdlog::debug("Fixed address: {:08X}", pattern->fixedAddress);
		searchResults.push_back({ pattern->fixedAddress, 0, 0 });
		return searchResults;
	}

	if (!patternSize) {
		if (debug)
			spdlog::debug("FAIL: empty pattern!");
		return searchResults;
	}
	if (static_cast<size_t>(patternSize) > memory.size) {
		if (debug)
			spdlog::debug("FAIL: pattern is larger than memory!");
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

	if (debug)
		spdlog::debug("Search align: {}", align);

	/*
	 * Optimized variant of checkPattern().
	 */
	auto *masks = &pattern->masks[firstNonWildcardByte];
	auto *bytes = &pattern->bytes[firstNonWildcardByte];
	int size = patternSize - firstNonWildcardByte;

	if (size >= 4 && !isTrulyWildcard) { // faster
		if (debug)
			spdlog::debug("Using fast pattern matching algorithm.");

		uint32_t mask = *reinterpret_cast<uint32_t *>(masks);
		uint32_t searchValue = *reinterpret_cast<uint32_t *>(bytes) & mask;

		if (debug)
			spdlog::debug("Search prefix: mask={:08X}, searchValue={:08X}", mask, searchValue);

		for (size_t i = firstNonWildcardByte; i < memory.size - patternSize + 1; i += align) {
			uint32_t memoryValue = *reinterpret_cast<const uint32_t *>(memory.data + i);
			if ((memoryValue & mask) == searchValue) {
				if (size == 4 || fuzzyMatch(bytes + 4, masks + 4, size -  4, memory.data + i + 4)) {
					size_t foundOffset = i - firstNonWildcardByte;

					if (debug)
						spdlog::debug("Possible result at {:08X}", memory.base + foundOffset);

					if (checkSubpatterns(pattern, foundOffset, memory)) {
						auto [isDecoded, result] = decodeResult(pattern, foundOffset, memory);
						if (isDecoded) {
							searchResults.push_back(result);

							if (debug)
								spdlog::debug("FOUND: address={:08X}, offset={:08X}", result.address, result.offset);

							if (maxResults && searchResults.size() >= maxResults) {
								if (debug)
									spdlog::debug("Maximum search results are reached.");
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
							if (debug)
								spdlog::debug("FAIL: can't decode result!");
						}
					} else {
						if (debug)
							spdlog::debug("FAIL: sub patterns not matched.");
					}
				}
			}
		}
	} else {
		if (debug)
			spdlog::debug("Using slow pattern matching algorithm.");

		for (size_t i = firstNonWildcardByte; i < memory.size - patternSize + 1; i += align) {
			if (fuzzyMatch(bytes, masks, size, memory.data + i)) {
				size_t foundOffset = i - firstNonWildcardByte;
				if (debug)
					spdlog::debug("Possible result at {:08X}", memory.base + foundOffset);
				if (checkSubpatterns(pattern, foundOffset, memory)) {
					auto [isDecoded, result] = decodeResult(pattern, foundOffset, memory);
					if (isDecoded) {
						searchResults.push_back(result);

						if (debug)
							spdlog::debug("FOUND: address={:08X}, offset={:08X}", result.address, result.offset);

						if (maxResults && searchResults.size() >= maxResults) {
							if (debug)
								spdlog::debug("Maximum search results are reached.");
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
						if (debug)
							spdlog::debug("FAIL: can't decode result!");
					}
				} else {
					if (debug)
						spdlog::debug("FAIL: sub patterns not matched.");
				}
			}
		}
	}

	return searchResults;
}

std::vector<Pattern::XRef> Pattern::finXRefs(uint32_t addr, const Memory &memory, size_t maxResults) {
	bool debug = spdlog::should_log(spdlog::level::debug);
	if (debug)
		spdlog::debug("Searching XRef's for {:08X}", addr);
	std::vector<XRef> searchResults;
	for (size_t i = 0; i < memory.size; i += 2) {
		bool isReference;
		uint32_t refAddr;
		std::tie(isReference, refAddr, std::ignore) = decodeReference(i, memory);
		bool isBranchReference;
		uint32_t branchAddr;
		std::tie(isBranchReference, branchAddr, std::ignore) = decodeBranchReference(i, memory);
		auto [isPointer, ptrAddr] = decodePointer(i + memory.base, memory);
		if (isBranchReference && (branchAddr & ~1) == (addr & ~1)) {
			if (debug)
				spdlog::debug("FOUND: branch at {:08X}", i + memory.base);
			searchResults.push_back({ RESULT_TYPE_BRANCH, static_cast<uint32_t>(memory.base + i), static_cast<uint32_t>(i) });
		} else if (isReference && (refAddr & ~1) == (addr & ~1)) {
			if (debug)
				spdlog::debug("FOUND: reference at {:08X}", i + memory.base);
			searchResults.push_back({ RESULT_TYPE_REFERENCE, static_cast<uint32_t>(memory.base + i), static_cast<uint32_t>(i) });
		} else if (isPointer && (ptrAddr & ~1) == (addr & ~1)) {
			if (debug)
				spdlog::debug("FOUND: pointer at {:08X}", i + memory.base);
			searchResults.push_back({ RESULT_TYPE_POINTER, static_cast<uint32_t>(memory.base + i), static_cast<uint32_t>(i) });
		}

		if (maxResults && searchResults.size() >= maxResults) {
			if (debug)
				spdlog::debug("Maximum search results are reached.");
			break;
		}
	}
	return searchResults;
}

std::string Pattern::stringify(const std::shared_ptr<PtrExp> &pattern) {
	if (pattern->type == RESULT_TYPE_ADDRESS)
		return std::format("< {:08X} >", pattern->fixedAddress);

	std::string patternText;

	if (pattern->type == RESULT_TYPE_REFERENCE) {
		patternText += "&(";
	} else if (pattern->type == RESULT_TYPE_BRANCH) {
		patternText += "&BL(";
	} else if (pattern->type == RESULT_TYPE_POINTER) {
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
			i += p.size - 1;
		} else {
			if (mask == 0x00) {
				tmp.push_back("??");
			} else if (mask == 0x0F) {
				tmp.push_back(std::format("?{:X}", byte & 0x0F));
			} else if (mask == 0xF0) {
				tmp.push_back(std::format("{:X}?", (byte & 0xF0) >> 4));
			} else if (mask == 0xFF) {
				tmp.push_back(std::format("{:02X}", byte));
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
				tmp.push_back(std::format("[{}]", bin));
			}
		}
	}

	if (pattern->inputOffset != 0)
		tmp.push_back(std::format("{} 0x{:X}", pattern->inputOffset < 0 ? '-' : '+', abs(pattern->inputOffset)));

	patternText += strJoin(" ", tmp);

	if (pattern->type == RESULT_TYPE_REFERENCE || pattern->type == RESULT_TYPE_BRANCH || pattern->type == RESULT_TYPE_POINTER) {
		patternText += ")";
	}

	if (pattern->outputOffset != 0)
		patternText += std::format(" {} 0x{:X}", pattern->outputOffset < 0 ? '-' : '+', abs(pattern->outputOffset));

	return patternText;
}

}; // namespace Ptr89
