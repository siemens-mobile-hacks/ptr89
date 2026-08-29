#include "Ptr89Wasm.h"

#include <algorithm>
#include <format>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace Ptr89 {

static std::string getBytes(uint32_t offset, size_t size, const Pattern::Memory &memory) {
	std::string bytes;
	size = offset < memory.size ? std::min(size, memory.size - offset) : 0;
	bytes.reserve(size * 2);
	for (size_t i = 0; i < size; i++)
		bytes += std::format("{:02X}", memory.data[offset + i]);
	return bytes;
}

Ptr89Wasm::Ptr89Wasm() {
	spdlog::set_pattern("%v");
	spdlog::set_level(spdlog::level::warn);
}

void Ptr89Wasm::open(uintptr_t ptr, size_t size, uint32_t base, const std::string &archName) {
	if (ptr == 0 && size != 0)
		throw std::invalid_argument("Memory pointer is null.");

	Architecture arch;
	if (archName == "arm") {
		arch = ARCH_ARM;
	} else if (archName == "c166") {
		arch = ARCH_C166;
	} else {
		throw std::invalid_argument("Invalid architecture '" + archName + "'. Expected arm or c166.");
	}

	m_data = reinterpret_cast<const uint8_t *>(ptr);
	m_size = size;
	m_base = base;
	m_arch = arch;
	m_open = true;
}

void Ptr89Wasm::close() {
	m_data = nullptr;
	m_size = 0;
	m_open = false;
}

void Ptr89Wasm::setDebug(bool enabled) {
	spdlog::set_level(enabled ? spdlog::level::debug : spdlog::level::warn);
}

Ptr89Search Ptr89Wasm::find(const std::string &patternText, size_t limit, int align) const {
	auto memory = getMemory(align);
	auto pattern = Pattern::parse(patternText);
	auto matches = Pattern::find(pattern, memory, limit);

	Ptr89Search search = { patternText, Pattern::getSearchTypeName(pattern->type), { } };
	search.results.reserve(matches.size());
	for (const auto &match: matches) {
		std::optional<uint32_t> offset;
		if (pattern->type == RESULT_TYPE_OFFSET)
			offset = match.offset;
		else if (Pattern::inMemory(memory, match.address))
			offset = match.address - memory.base;
		std::string bytes = pattern->type == RESULT_TYPE_OFFSET ?
			getBytes(match.offset, match.size, memory) :
			"";
		search.results.push_back({ match.address, offset, bytes });
	}
	return search;
}

std::vector<Ptr89XRef> Ptr89Wasm::findXRefs(uint32_t address, size_t limit) const {
	auto memory = getMemory();
	auto matches = Pattern::finXRefs(address, memory, limit);
	std::vector<Ptr89XRef> results;
	results.reserve(matches.size());
	for (const auto &match: matches) {
		results.push_back({ Pattern::getResultTypeName(match.type), match.address, match.offset });
	}
	return results;
}

Pattern::Memory Ptr89Wasm::getMemory(int align) const {
	if (!m_open)
		throw std::runtime_error("Ptr89 is not opened.");
	if (align <= 0)
		throw std::invalid_argument("Search alignment must be greater than zero.");
	return { m_base, m_data, m_size, align, m_arch };
}

std::string prettify(const std::string &pattern) {
	return Pattern::stringify(Pattern::parse(pattern));
}

}; // namespace Ptr89
