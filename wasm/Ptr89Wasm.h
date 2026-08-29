#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <ptr89.h>

namespace Ptr89 {

struct WasmPatternSearchResult {
	std::string type;
	uint32_t address;
	uint32_t offset;
	uint32_t value;
};

struct WasmXRefSearchResult {
	std::string type;
	uint32_t address;
	uint32_t offset;
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
		std::vector<WasmPatternSearchResult> find(const std::string &patternText, size_t limit) const;
		std::vector<WasmXRefSearchResult> findXRefs(uint32_t address, size_t limit) const;
};

std::string prettify(const std::string &pattern);

}; // namespace Ptr89
