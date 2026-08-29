#pragma once

#include <cstddef>
#include <memory>
#include <map>
#include <tuple>
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

namespace Ptr89 {

enum Architecture {
	ARCH_ARM,
	ARCH_C166,
};

enum ResultType {
	RESULT_TYPE_OFFSET,		// AB ?? CD ??, return address of the matched bytes
	RESULT_TYPE_POINTER,		// *(AB ?? CD ??), use bytes as pointer
	RESULT_TYPE_REFERENCE,		// &(AB ?? CD ??), decode LDR
	RESULT_TYPE_BRANCH,		// &BL(AB ?? CD ??), decode branch
	RESULT_TYPE_ADDRESS,		// < FFFFFFFF >
};

enum SubPatternType {
	SUB_PATTERN_TYPE_BRANCH_4B,		// _blf(AB ?? CD ??) or { AB ?? CD ?? }
	SUB_PATTERN_TYPE_BRANCH_2B,		// [ AB ?? CD ?? ]
	SUB_PATTERN_TYPE_LDR_4B,		// LDR{ AB ?? CD ?? }
	SUB_PATTERN_TYPE_LDR_2B,		// LDR[ AB ?? CD ?? ]
};

struct PtrExp;

struct PtrExpPart {
	SubPatternType type;
	std::vector<uint16_t> bytes;		// for SUB_PATTERN_TYPE_BYTES or SUB_PATTERN_TYPE_STRING
	std::shared_ptr<PtrExp> pattern;	// for SUB_PATTERN_TYPE_BRANCH_4B or SUB_PATTERN_TYPE_BRANCH_2B
};

struct SubPtrExp {
	SubPatternType type;
	std::shared_ptr<PtrExp> pattern;
	int offset;
	int size;
};

struct PtrExp {
	ResultType type = RESULT_TYPE_OFFSET;
	int inputOffset = 0; // &( AB ?? CD ?? + 1 )
	int outputOffset = 0; // &( AB ?? CD ?? ) + 1 or AB ?? AB ?? + 1
	std::vector<uint8_t> masks;
	std::vector<uint8_t> bytes;
	std::map<int, SubPtrExp> subPatterns;
	uint32_t fixedAddress = 0; // for RESULT_TYPE_ADDRESS
};

class Parser;

class PatternError: public std::runtime_error {
	public:
		PatternError(const Parser *parser, const std::string &msg);
	private:
		std::string getErrorMsg(const Parser *parser, const std::string &msg);
};

class Pattern {
	public:
		struct Memory {
			uint32_t base;
			const uint8_t *data;
			size_t size;
			int align = 1;
			Architecture arch = ARCH_ARM;
		};

		struct SearchResult {
			uint32_t address;
			uint32_t offset;
			size_t size;
		};

		struct XRef {
			ResultType type;
			uint32_t address;
			uint32_t offset;
			size_t size;
		};

		static std::shared_ptr<PtrExp> parse(const std::string &pattern);
		static const char *getResultTypeName(ResultType type);
		static std::string stringify(const std::shared_ptr<PtrExp> &pattern);
		static int findAlignForPattern(const std::shared_ptr<PtrExp> &pattern, int align, Architecture architecture = ARCH_ARM);
		static std::vector<SearchResult> find(const std::shared_ptr<PtrExp> &pattern, const Memory &memory, size_t maxResults = 0);
		static std::vector<XRef> finXRefs(uint32_t addr, const Memory &memory, size_t maxResults = 0);
		static bool checkPattern(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory);
		static std::tuple<bool, uint32_t, size_t> decodeReference(uint32_t offset, const Memory &memory);
		static std::tuple<bool, uint32_t, size_t> decodeBranchReference(uint32_t offset, const Memory &memory);
		static std::pair<bool, uint32_t> decodePointer(uint32_t addr, const Memory &memory);
		static uint32_t resolveThunks(uint32_t addr, const Memory &memory);

		static inline bool inMemory(const Memory &memory, uint64_t addr, uint64_t size = 1) {
			return addr >= memory.base && addr + size <= memory.base + memory.size;
		}

	private:
		static bool checkSubpatterns(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory);
		static bool fuzzyMatch(const uint8_t *bytes, const uint8_t *masks, int patternSize, const uint8_t *memory);
		static std::pair<bool, SearchResult> decodeResult(const std::shared_ptr<PtrExp> &pattern, uint32_t matchOffset, const Memory &memory);

};

}; // namespace Ptr89
