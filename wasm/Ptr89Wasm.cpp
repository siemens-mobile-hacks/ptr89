#include "Ptr89Wasm.h"

#include <stdexcept>

#include <spdlog/spdlog.h>

namespace Ptr89 {

static const char *getPatternTypeName(PatternType type) {
	switch (type) {
		case PATTERN_TYPE_OFFSET:
			return "offset";
		case PATTERN_TYPE_POINTER:
			return "pointer";
		case PATTERN_TYPE_REFERENCE:
			return "reference";
		case PATTERN_TYPE_BRANCH_REFERENCE:
			return "branch";
		case PATTERN_TYPE_STATIC_VALUE:
			return "static_value";
	}
	throw std::invalid_argument("Invalid pattern type.");
}

static const char *getXRefTypeName(XRefType type) {
	switch (type) {
		case XREF_TYPE_REFERENCE:
			return "reference";
		case XREF_TYPE_BRANCH_CALL:
			return "branch";
		case XREF_TYPE_POINTER:
			return "pointer";
	}
	throw std::invalid_argument("Invalid x-ref type.");
}

Ptr89Wasm::Ptr89Wasm() {
	spdlog::set_pattern("%v");
	spdlog::set_level(spdlog::level::warn);
}

void Ptr89Wasm::open(uintptr_t ptr, size_t size, uint32_t base, int align, const std::string &archName) {
	if (ptr == 0 && size != 0)
		throw std::invalid_argument("Memory pointer is null.");
	if (align <= 0)
		throw std::invalid_argument("Memory alignment must be greater than zero.");

	Architecture arch;
	if (archName == "arm") {
		arch = ARCH_ARM;
	} else if (archName == "c166") {
		arch = ARCH_C166;
	} else {
		throw std::invalid_argument("Invalid architecture '" + archName + "'. Expected arm or c166.");
	}

	const auto *data = reinterpret_cast<const uint8_t *>(ptr);
	m_data.clear();
	if (size != 0)
		m_data.assign(data, data + size);
	m_base = base;
	m_align = align;
	m_arch = arch;
	m_open = true;
}

void Ptr89Wasm::close() {
	m_data.clear();
	m_data.shrink_to_fit();
	m_open = false;
}

void Ptr89Wasm::setDebug(bool enabled) {
	spdlog::set_level(enabled ? spdlog::level::debug : spdlog::level::warn);
}

std::vector<WasmPatternSearchResult> Ptr89Wasm::find(const std::string &patternText, size_t limit) const {
	auto memory = getMemory();
	auto pattern = Pattern::parse(patternText);
	auto matches = Pattern::find(pattern, memory, limit);

	std::vector<WasmPatternSearchResult> results;
	results.reserve(matches.size());
	for (const auto &match: matches)
		results.push_back({ getPatternTypeName(pattern->type), match.address, match.offset, match.value });
	return results;
}

std::vector<WasmXRefSearchResult> Ptr89Wasm::findXRefs(uint32_t address, size_t limit) const {
	auto matches = Pattern::finXRefs(address, getMemory(), limit);
	std::vector<WasmXRefSearchResult> results;
	results.reserve(matches.size());
	for (const auto &match: matches)
		results.push_back({ getXRefTypeName(match.type), match.address, match.offset });
	return results;
}

Pattern::Memory Ptr89Wasm::getMemory() const {
	if (!m_open)
		throw std::runtime_error("Ptr89 is not opened.");
	return { m_base, m_data.data(), m_data.size(), m_align, m_arch };
}

std::string prettify(const std::string &pattern) {
	return Pattern::stringify(Pattern::parse(pattern));
}

}; // namespace Ptr89
