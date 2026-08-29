#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <ptr89.h>

namespace Ptr89 {

struct Ptr89SearchResult {
	uint32_t address;
	uint32_t offset;
	std::string bytes;
};

struct Ptr89XRef {
	std::string type;
	uint32_t xref;
	uint32_t offset;
	std::string bytes;
};

class Ptr89Wasm {
	private:
		std::vector<uint8_t> m_data;
		uint32_t m_base = 0;
		int m_align = 1;
		Architecture m_arch = ARCH_ARM;
		bool m_open = false;

		Pattern::Memory getMemory() const;

	public:
		Ptr89Wasm();

		void open(uintptr_t ptr, size_t size, uint32_t base, int align, const std::string &archName);
		void close();
		void setDebug(bool enabled);
		std::vector<Ptr89SearchResult> find(const std::string &patternText, size_t limit) const;
		std::vector<Ptr89XRef> findXRefs(uint32_t address, size_t limit) const;
};

std::string prettify(const std::string &pattern);

}; // namespace Ptr89
