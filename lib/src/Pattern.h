#pragma once

#include <cstddef>
#include <memory>
#include <map>
#include <tuple>
#include <cstdint>
#include <string>
#include <cstdio>
#include <vector>
#include <cstring>

namespace Ptr89 {

enum PatternType {
	PATTERN_TYPE_OFFSET,			// AB ?? CD ??, return offset of the finded bytes
	PATTERN_TYPE_POINTER,			// *(AB ?? CD ??), use bytes as pointer
	PATTERN_TYPE_REFERENCE,			// &(AB ?? CD ??), decode LDR
	PATTERN_TYPE_BRANCH_REFERENCE,	// &BL(AB ?? CD ??) decode B/BL
	PATTERN_TYPE_STATIC_VALUE,		// < FFFFFFFF >
};

enum SubPatternType {
	SUB_PATTERN_TYPE_BRANCH_4B,		// _blf(AB ?? CD ??) or { AB ?? CD ?? }
	SUB_PATTERN_TYPE_BRANCH_2B,		// [ AB ?? CD ?? ]
	SUB_PATTERN_TYPE_LDR_4B,		// LDR{ AB ?? CD ?? }
	SUB_PATTERN_TYPE_LDR_2B,		// LDR[ AB ?? CD ?? ]
};

enum XRefType {
	XREF_TYPE_REFERENCE,
	XREF_TYPE_BRANCH_CALL,
	XREF_TYPE_POINTER,
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
	PatternType type = PATTERN_TYPE_OFFSET;
	int inputOffset = 0; // &( AB ?? CD ?? + 1 )
	int outputOffset = 0; // &( AB ?? CD ?? ) + 1 or AB ?? AB ?? + 1
	std::vector<uint8_t> masks;
	std::vector<uint8_t> bytes;
	std::map<int, SubPtrExp> subPatterns;
	uint32_t staticValue = 0; // for PATTERN_TYPE_STATIC_VALUE
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
		typedef typeof(vprintf) * DebugHandlerFunc;

		struct Memory {
			uint32_t base;
			const uint8_t *data;
			size_t size;
			int align = 1;
		};

		struct SearchResult {
			uint32_t address;
			uint32_t offset;
			uint32_t value;
		};

		struct XRefSearchResult {
			XRefType type;
			uint32_t address;
			uint32_t offset;
		};

		static std::shared_ptr<PtrExp> parse(const std::string &pattern);
		static std::string stringify(const std::shared_ptr<PtrExp> &pattern);
		static int findAlignForPattern(const std::shared_ptr<PtrExp> &pattern, int align);
		static std::vector<SearchResult> find(const std::shared_ptr<PtrExp> &pattern, const Memory &memory, size_t maxResults = 0);
		static std::vector<XRefSearchResult> finXRefs(uint32_t addr, const Memory &memory, size_t maxResults = 0);
		static bool checkPattern(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory);
		static std::tuple<bool, uint32_t, bool> decodeThumbBL(uint32_t offset, const uint8_t *bytes);
		static std::tuple<bool, uint32_t, bool> decodeArmBL(uint32_t offset, const uint8_t *bytes);
		static std::pair<bool, uint32_t> decodeThumbB(uint32_t offset, const uint8_t *bytes);
		static std::pair<bool, uint32_t> decodeThumbLDR(uint32_t offset, const uint8_t *bytes);
		static std::tuple<bool, uint32_t, bool> decodeArmLDR(uint32_t offset, const uint8_t *bytes);
		static std::pair<bool, uint32_t> decodeArmThrunk(uint32_t offset, const uint8_t *bytes);
		static std::pair<bool, uint32_t> decodeReference(uint32_t offset, const Memory &memory);
		static std::pair<bool, uint32_t> decodeBranchReference(uint32_t offset, const Memory &memory);
		static std::pair<bool, uint32_t> decodePointer(uint32_t addr, const Memory &memory);
		static uint32_t resolveThunks(uint32_t addr, const Memory &memory);

		static inline bool inMemory(const Memory &memory, uint64_t addr, uint64_t size = 1) {
			return addr >= memory.base && addr + size <= memory.base + memory.size;
		}

		static void setDebugHandler(DebugHandlerFunc debugHandler) {
			m_debugHandler = debugHandler;
		}

		static void debugSectionBegin() {
			m_debugLevel++;
		}

		static void debugSectionEnd() {
			m_debugLevel--;
		}

		static void debug(const char *format, ...)  __attribute__((format(printf, 1, 2)));
		static void _debug(const char *format, ...)  __attribute__((format(printf, 1, 2)));
	private:
		static DebugHandlerFunc m_debugHandler;
		static int m_debugLevel;
		static bool checkSubpatterns(const std::shared_ptr<PtrExp> &pattern, size_t offset, const Memory &memory);
		static bool fuzzyMatch(const uint8_t *bytes, const uint8_t *masks, int patternSize, const uint8_t *memory);
		static std::pair<bool, Pattern::SearchResult> decodeResult(const std::shared_ptr<PtrExp> &pattern, uint32_t offset, const Memory &memory);

		static inline uint32_t signExtend(uint32_t value, int from, int to) {
			if ((value & (1 << (from - 1))) != 0) {
				uint32_t mask = 0;
				for (int i = from; i < to; i++)
					mask |= 1 << i;
				return mask | value;
			} else {
				return value;
			}
		}
};

}; // namespace Ptr89
