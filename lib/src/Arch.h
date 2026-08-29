#pragma once

#include "Pattern.h"

namespace Ptr89 {

class Arch {
	public:
		virtual ~Arch() = default;

		virtual std::pair<bool, uint32_t> decodeBranch2(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const = 0;
		virtual std::pair<bool, uint32_t> decodeBranch4(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const = 0;
		virtual std::vector<uint32_t> decodeReferences2(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const = 0;
		virtual std::vector<uint32_t> decodeReferences4(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const = 0;
		virtual std::tuple<bool, uint32_t, size_t> decodeReference(uint32_t offset, const Pattern::Memory &memory) const = 0;
		virtual std::tuple<bool, uint32_t, size_t> decodeBranchReference(uint32_t offset, const Pattern::Memory &memory) const = 0;
		virtual std::pair<bool, uint32_t> decodePointer(uint32_t address, const Pattern::Memory &memory) const = 0;
		virtual uint32_t resolveThunks(uint32_t address, const Pattern::Memory &memory) const = 0;
		virtual uint32_t offsetValue(uint32_t address, uint32_t offset, const Pattern::Memory &memory) const = 0;
		virtual int reference4Align() const = 0;
};

const Arch &getArch(Architecture architecture);

}; // namespace Ptr89
