#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <ptr89.h>

namespace Ptr89 {

struct Ptr89SearchResult {
	uint32_t address;
	std::optional<uint32_t> offset;
	std::string bytes;
};

struct Ptr89Search {
	std::string pattern;
	std::string type;
	std::vector<Ptr89SearchResult> results;
};

struct Ptr89XRef {
	std::string type;
	uint32_t xref;
	uint32_t offset;
};

class Ptr89Wasm {
	private:
		std::vector<uint8_t> m_data;
		uint32_t m_base = 0;
		Architecture m_arch = ARCH_ARM;
		bool m_open = false;

		Pattern::Memory getMemory(int align = 1) const;

	public:
		Ptr89Wasm();

		void open(uintptr_t ptr, size_t size, uint32_t base, const std::string &archName);
		void close();
		void setDebug(bool enabled);
		Ptr89Search find(const std::string &patternText, size_t limit, int align) const;
		std::vector<Ptr89XRef> findXRefs(uint32_t address, size_t limit) const;
};

std::string prettify(const std::string &pattern);

}; // namespace Ptr89
