#include "Arch.h"

#include <bit>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace Ptr89 {
namespace {

static const char *MNEMONICS[] = { "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "", "??" };
static const char *REGNAMES[] = { "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC" };

static uint32_t signExtend(uint32_t value, int from, int to) {
	if ((value & (1U << (from - 1))) == 0)
		return value;
	for (int i = from; i < to; i++)
		value |= 1U << i;
	return value;
}

static std::tuple<bool, uint32_t, bool> decodeThumbBL(uint32_t address, const uint8_t *bytes) {
	uint16_t first = (bytes[1] << 8) | bytes[0];
	uint16_t second = (bytes[3] << 8) | bytes[2];
	if ((address % 2) != 0)
		return { false, 0, false };

	if ((first & 0xF800) == 0xF000 && (second & 0xF800) == 0xE800) {
		int32_t high = static_cast<int32_t>(signExtend(first & 0x7FF, 11, 32) << 12);
		uint32_t low = (second & 0x7FF) << 1;
		uint32_t target = (address + 4 + high + low) & 0xFFFFFFFC;
		if (spdlog::should_log(spdlog::level::debug)) {
			spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  BLX #0x{:08X}",
				address, bytes[0], bytes[1], bytes[2], bytes[3], target);
		}
		return { true, target, true };
	}
	if ((first & 0xF800) == 0xF000 && (second & 0xF800) == 0xF800) {
		int32_t high = static_cast<int32_t>(signExtend(first & 0x7FF, 11, 32) << 12);
		uint32_t low = (second & 0x7FF) << 1;
		uint32_t target = address + 4 + high + low;
		if (spdlog::should_log(spdlog::level::debug)) {
			spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  BL #0x{:08X}",
				address, bytes[0], bytes[1], bytes[2], bytes[3], target);
		}
		return { true, target, false };
	}
	return { false, 0, false };
}

static std::tuple<bool, uint32_t, bool> decodeArmBL(uint32_t address, const uint8_t *bytes) {
	uint32_t instruction = static_cast<uint32_t>(bytes[0]) |
		(static_cast<uint32_t>(bytes[1]) << 8) |
		(static_cast<uint32_t>(bytes[2]) << 16) |
		(static_cast<uint32_t>(bytes[3]) << 24);
	if ((address % 4) != 0)
		return { false, 0, false };

	if ((instruction & 0xFE000000) == 0xFA000000) {
		int32_t displacement = static_cast<int32_t>(signExtend(instruction & 0xFFFFFF, 24, 30) << 2U);
		uint32_t h = (instruction & 0x01000000) != 0 ? 1 : 0;
		uint32_t target = address + 8 + displacement + (h << 1);
		if (spdlog::should_log(spdlog::level::debug)) {
			spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  BLX #0x{:08X}",
				address, bytes[0], bytes[1], bytes[2], bytes[3], target);
		}
		return { true, target, true };
	}
	if ((instruction & 0x0F000000) == 0x0B000000 || (instruction & 0x0F000000) == 0x0A000000) {
		int32_t displacement = static_cast<int32_t>(signExtend(instruction & 0xFFFFFF, 24, 30) << 2U);
		uint32_t target = address + 8 + displacement;
		uint32_t condition = (instruction & 0xF0000000) >> 28;
		bool link = (instruction & 0x0F000000) == 0x0B000000;
		if (spdlog::should_log(spdlog::level::debug)) {
			spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  B{}{} #0x{:08X}", address,
				bytes[0], bytes[1], bytes[2], bytes[3], link ? "L" : "", MNEMONICS[condition], target);
		}
		return { true, target, false };
	}
	return { false, 0, false };
}

static std::pair<bool, uint32_t> decodeThumbB(uint32_t address, const uint8_t *bytes) {
	uint16_t instruction = (bytes[1] << 8) | bytes[0];
	if ((address % 2) != 0)
		return { false, 0 };

	if ((instruction & 0xF800) == 0xE000) {
		int32_t displacement = static_cast<int32_t>(signExtend(instruction & 0x7FF, 11, 32) << 1);
		uint32_t target = address + 4 + displacement;
		if (spdlog::should_log(spdlog::level::debug)) {
			spdlog::debug("{:08X}: {:02X} {:02X}        B #0x{:08X}", address, bytes[0], bytes[1], target);
		}
		return { true, target };
	}
	if ((instruction & 0xF000) == 0xD000) {
		uint32_t condition = (instruction & 0x0F00) >> 8;
		if (condition >= 0xE)
			return { false, 0 };

		int32_t displacement = static_cast<int32_t>(signExtend(instruction & 0xFF, 8, 32) << 1);
		uint32_t target = address + 4 + displacement;
		if (spdlog::should_log(spdlog::level::debug)) {
			spdlog::debug("{:08X}: {:02X} {:02X}        B{} #0x{:08X}",
				address, bytes[0], bytes[1], MNEMONICS[condition], target);
		}
		return { true, target };
	}
	return { false, 0 };
}

static std::pair<bool, uint32_t> decodeThumbLDR(uint32_t address, const uint8_t *bytes) {
	uint16_t instruction = (bytes[1] << 8) | bytes[0];
	if ((address % 2) != 0 || (instruction & 0xF800) != 0x4800)
		return { false, 0 };

	uint32_t displacement = (instruction & 0xFF) << 2;
	uint32_t reg = (instruction & 0x700) >> 8;
	uint32_t target = address + (address % 4 == 0 ? 4 : 2) + displacement;
	if (spdlog::should_log(spdlog::level::debug)) {
		spdlog::debug("{:08X}: {:02X} {:02X}        LDR {}, [PC, #0x{:X}] ; 0x{:08X}", address,
			bytes[0], bytes[1], REGNAMES[reg], displacement, target);
	}
	return { true, target };
}

static std::pair<bool, uint32_t> decodeThumbADR(uint32_t address, const uint8_t *bytes) {
	uint16_t instruction = (bytes[1] << 8) | bytes[0];
	if ((address % 2) != 0 || (instruction & 0xF800) != 0xA000)
		return { false, 0 };

	uint32_t displacement = (instruction & 0xFF) << 2;
	uint32_t reg = (instruction & 0x700) >> 8;
	uint32_t target = ((address + 4) & ~3U) + displacement;
	if (spdlog::should_log(spdlog::level::debug)) {
		spdlog::debug("{:08X}: {:02X} {:02X}        ADR {}, #0x{:X} ; 0x{:08X}",
			address, bytes[0], bytes[1], REGNAMES[reg], displacement, target);
	}
	return { true, target };
}

static std::tuple<bool, uint32_t, bool> decodeArmLDR(uint32_t address, const uint8_t *bytes) {
	uint32_t instruction = static_cast<uint32_t>(bytes[0]) |
		(static_cast<uint32_t>(bytes[1]) << 8) |
		(static_cast<uint32_t>(bytes[2]) << 16) |
		(static_cast<uint32_t>(bytes[3]) << 24);
	if ((address % 4) != 0 || (instruction & 0x0F7F0000) != 0x051F0000 ||
		(instruction & 0xF0000000) == 0xF0000000)
		return { false, 0, false };

	bool add = (instruction & (1U << 23)) != 0;
	int32_t displacement = add ? static_cast<int32_t>(instruction & 0xFFFU) : -static_cast<int32_t>(instruction & 0xFFFU);
	uint32_t target = address + 8 + displacement;
	uint32_t reg = (instruction & 0xF000) >> 12;
	uint32_t condition = (instruction & 0xF0000000) >> 28;
	if (spdlog::should_log(spdlog::level::debug)) {
		spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  LDR{} {}, [PC, #{}0x{:X}] ; 0x{:08X}", address,
			bytes[0], bytes[1], bytes[2], bytes[3], MNEMONICS[condition], REGNAMES[reg],
			add ? '+' : '-', displacement < 0 ? -displacement : displacement, target);
	}
	return { true, target, reg == 0xF };
}

static std::pair<bool, uint32_t> decodeArmADR(uint32_t address, const uint8_t *bytes) {
	uint32_t instruction = static_cast<uint32_t>(bytes[0]) |
		(static_cast<uint32_t>(bytes[1]) << 8) |
		(static_cast<uint32_t>(bytes[2]) << 16) |
		(static_cast<uint32_t>(bytes[3]) << 24);
	uint32_t opcode = instruction & 0x0FF00000;
	if ((address % 4) != 0 || (opcode != 0x02800000 && opcode != 0x02400000) ||
		(instruction & 0x000F0000) != 0x000F0000 || (instruction & 0x0000F000) == 0x0000F000 ||
		(instruction & 0xF0000000) == 0xF0000000)
		return { false, 0 };

	uint32_t immediate = std::rotr(instruction & 0xFF, ((instruction >> 8) & 0xF) * 2);
	bool add = opcode == 0x02800000;
	uint32_t target = add ? address + 8 + immediate : address + 8 - immediate;
	uint32_t reg = (instruction & 0xF000) >> 12;
	uint32_t condition = instruction >> 28;
	if (spdlog::should_log(spdlog::level::debug)) {
		spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  {}{} {}, PC, #0x{:X} ; 0x{:08X}", address,
			bytes[0], bytes[1], bytes[2], bytes[3], add ? "ADD" : "SUB", MNEMONICS[condition],
			REGNAMES[reg], immediate, target);
	}
	return { true, target };
}

static std::tuple<bool, uint8_t, uint32_t, size_t> decodeC166Mov(const uint8_t *bytes, size_t size) {
	if (size >= 4 && bytes[0] == 0xE6 && bytes[1] >= 0xF0)
		return { true, bytes[1], bytes[2] | (static_cast<uint32_t>(bytes[3]) << 8), 4 };
	if (size >= 2 && bytes[0] == 0xE0)
		return { true, static_cast<uint8_t>(0xF0 | (bytes[1] & 0x0F)), bytes[1] >> 4, 2 };
	return { false, 0, 0, 0 };
}

static std::vector<uint32_t> decodeC166Pair(uint32_t address, const uint8_t *bytes,
	const Pattern::Memory &memory, size_t firstSize) {
	if ((address & 1) != 0 || !Pattern::inMemory(memory, address, firstSize + 2))
		return { };

	size_t available = memory.base + memory.size - address;
	auto [hasLow, lowReg, low, lowSize] = decodeC166Mov(bytes, available);
	if (!hasLow || lowSize != firstSize || (lowReg & 1) != 0)
		return { };

	bool hasHigh;
	uint8_t highReg;
	uint32_t high;
	std::tie(hasHigh, highReg, high, std::ignore) = decodeC166Mov(bytes + lowSize, available - lowSize);
	if (!hasHigh || highReg != lowReg + 1)
		return { };

	std::vector<uint32_t> references;
	// Huge/code pointer: 16-bit segment offset and 8-bit segment number.
	if (high < 0x100)
		references.push_back((high << 16) | low);
	// Far data pointer: 14-bit page offset and 10-bit page number.
	if (low < 0x4000 && high < 0x400) {
		uint32_t target = (high << 14) | low;
		if (references.empty() || references.front() != target)
			references.push_back(target);
	}
	return references;
}

class ArmArch final: public Arch {
	public:
		std::pair<bool, uint32_t> decodeBranch2(uint32_t address, const uint8_t *bytes, const Pattern::Memory &) const override {
			return decodeThumbB(address, bytes);
		}

		std::pair<bool, uint32_t> decodeBranch4(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const override {
			auto thumb = decodeThumbBL(address, bytes);
			if (std::get<0>(thumb) && Pattern::inMemory(memory, std::get<1>(thumb), 4))
				return { true, std::get<1>(thumb) };

			auto arm = decodeArmBL(address, bytes);
			if (std::get<0>(arm) && Pattern::inMemory(memory, std::get<1>(arm), 4))
				return { true, std::get<1>(arm) };

			auto ldr = decodeArmLDR(address, bytes);
			if (std::get<0>(ldr) && std::get<2>(ldr)) {
				auto [success, pointer] = decodePointer(std::get<1>(ldr), memory);
				if (success)
					return { true, pointer };
			}

			return { false, 0 };
		}

		std::vector<uint32_t> decodeReferences2(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const override {
			auto [isLdr, pointerAddress] = decodeThumbLDR(address, bytes);
			if (isLdr) {
				auto [success, pointer] = decodePointer(pointerAddress, memory);
				return success ? std::vector<uint32_t>{ pointer } : std::vector<uint32_t>{ };
			}

			auto [isAdr, target] = decodeThumbADR(address, bytes);
			return isAdr ? std::vector<uint32_t>{ target } : std::vector<uint32_t>{ };
		}

		std::vector<uint32_t> decodeReferences4(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const override {
			auto ldr = decodeArmLDR(address, bytes);
			if (std::get<0>(ldr)) {
				auto [success, pointer] = decodePointer(std::get<1>(ldr), memory);
				return success ? std::vector<uint32_t>{ pointer } : std::vector<uint32_t>{ };
			}

			auto [isAdr, target] = decodeArmADR(address, bytes);
			return isAdr ? std::vector<uint32_t>{ target } : std::vector<uint32_t>{ };
		}

		std::tuple<bool, uint32_t, size_t> decodeReference(uint32_t offset, const Pattern::Memory &memory) const override {
			auto [references, size] = decodeReferences(offset, memory);
			return references.empty() ?
				std::make_tuple(false, 0U, size_t{ 0 }) :
				std::make_tuple(true, references.front(), size);
		}

		std::tuple<bool, uint32_t, size_t> decodeBranchReference(uint32_t offset, const Pattern::Memory &memory) const override {
			if (offset + 2 > memory.size)
				return { false, 0, 0 };

			uint32_t address = memory.base + offset;
			auto [isThumbB, thumbBAddress] = decodeThumbB(address, memory.data + offset);
			if (isThumbB && Pattern::inMemory(memory, thumbBAddress, 2))
				return { true, resolveThunks(thumbBAddress, memory) | 1U, 2 };

			if (offset + 4 > memory.size)
				return { false, 0, 0 };

			auto [isThumb, thumbAddress, isThumbBLX] = decodeThumbBL(address, memory.data + offset);
			if (isThumb && Pattern::inMemory(memory, thumbAddress, 4))
				return { true, resolveThunks(thumbAddress, memory) | (!isThumbBLX ? 1U : 0U), 4 };

			auto [isArm, armAddress, isArmBLX] = decodeArmBL(address, memory.data + offset);
			if (isArm && Pattern::inMemory(memory, armAddress, 4))
				return { true, resolveThunks(armAddress, memory) | (isArmBLX ? 1U : 0U), 4 };

			auto [isLdr, pointerAddress, isThunk] = decodeArmLDR(address, memory.data + offset);
			if (isLdr && isThunk) {
				auto [success, pointer] = decodePointer(pointerAddress, memory);
				if (success)
					return { true, resolveThunks(pointer, memory), 4 };
			}

			return { false, 0, 0 };
		}

		size_t decodePointers(uint32_t address, const Pattern::Memory &memory,
			std::array<uint32_t, 2> &pointers) const override {
			if (!Pattern::inMemory(memory, address, 4))
				return 0;

			const uint8_t *bytes = memory.data + (address - memory.base);
			pointers[0] = bytes[0] |
				(static_cast<uint32_t>(bytes[1]) << 8) |
				(static_cast<uint32_t>(bytes[2]) << 16) |
				(static_cast<uint32_t>(bytes[3]) << 24);
			return 1;
		}

		uint32_t resolveThunks(uint32_t address, const Pattern::Memory &memory) const override {
			for (unsigned depth = 0; depth < 16 && Pattern::inMemory(memory, address, 4); depth++) {
				auto ldr = decodeArmLDR(address, memory.data + (address - memory.base));
				if (!std::get<0>(ldr) || !std::get<2>(ldr))
					break;

				auto [success, pointer] = decodePointer(std::get<1>(ldr), memory);
				if (!success || pointer == address || !Pattern::inMemory(memory, pointer))
					break;

				if (spdlog::should_log(spdlog::level::debug)) {
					spdlog::debug("Found thunk at {:08X}: PC->{:08X}", address, pointer);
				}
				address = pointer;
			}
			return address;
		}

		uint32_t offsetValue(uint32_t address, uint32_t offset, const Pattern::Memory &memory) const override {
			if ((address & 1) == 0 && Pattern::inMemory(memory, address, 2)) {
				const uint8_t *bytes = memory.data + offset;
				uint16_t instruction = bytes[0] | (bytes[1] << 8);
				if ((instruction & 0xFE00) == 0xB400)
					return address | 1;
			}
			return address;
		}

		bool sameAddress(uint32_t first, uint32_t second) const override {
			return (first & ~1U) == (second & ~1U);
		}

		int reference4Align() const override {
			return 4;
		}
};

class C166Arch final: public Arch {
	public:
		std::pair<bool, uint32_t> decodeBranch2(uint32_t address, const uint8_t *bytes, const Pattern::Memory &) const override {
			if ((address % 2) != 0)
				return { false, 0 };

			const uint8_t opcode = bytes[0];
			if ((opcode & 0x0F) != 0x0D && opcode != 0xBB)
				return { false, 0 };

			const int32_t displacement = static_cast<int8_t>(bytes[1]) * 2;
			const uint32_t segment = address & 0xFF0000;
			const uint16_t targetOffset = static_cast<uint16_t>(address + 2 + displacement);
			const uint32_t target = segment | targetOffset;
			if (spdlog::should_log(spdlog::level::debug)) {
				spdlog::debug("{:08X}: {:02X} {:02X}        {} #0x{:08X}", address, bytes[0], bytes[1],
					opcode == 0xBB ? "CALLR" : "JMPR", target);
			}
			return { true, target };
		}

		std::pair<bool, uint32_t> decodeBranch4(uint32_t address, const uint8_t *bytes, const Pattern::Memory &) const override {
			if ((address % 2) != 0)
				return { false, 0 };

			const uint8_t opcode = bytes[0];
			uint32_t target;
			const char *mnemonic;
			if (opcode == 0xDA || opcode == 0xFA) {
				target = (static_cast<uint32_t>(bytes[1]) << 16) | bytes[2] | (static_cast<uint32_t>(bytes[3]) << 8);
				mnemonic = opcode == 0xDA ? "CALLS" : "JMPS";
			} else if ((opcode == 0xCA || opcode == 0xEA) && (bytes[1] & 0x0F) == 0) {
				target = (address & 0xFF0000) | bytes[2] | (static_cast<uint32_t>(bytes[3]) << 8);
				mnemonic = opcode == 0xCA ? "CALLA" : "JMPA";
			} else if (opcode == 0xE2) {
				target = (address & 0xFF0000) | bytes[2] | (static_cast<uint32_t>(bytes[3]) << 8);
				mnemonic = "PCALL";
			} else if ((opcode == 0x8A || opcode == 0x9A || opcode == 0xAA || opcode == 0xBA) && (bytes[3] & 0x0F) == 0) {
				const int32_t displacement = static_cast<int8_t>(bytes[2]) * 2;
				const uint32_t segment = address & 0xFF0000;
				const uint16_t targetOffset = static_cast<uint16_t>(address + 4 + displacement);
				target = segment | targetOffset;
				mnemonic = opcode == 0x8A ? "JB" : opcode == 0x9A ? "JNB" : opcode == 0xAA ? "JBC" : "JNBS";
			} else {
				return { false, 0 };
			}

			if (spdlog::should_log(spdlog::level::debug)) {
				spdlog::debug("{:08X}: {:02X} {:02X} {:02X} {:02X}  {} #0x{:08X}", address,
					bytes[0], bytes[1], bytes[2], bytes[3], mnemonic, target);
			}
			return { true, target };
		}

		std::vector<uint32_t> decodeReferences2(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const override {
			return decodeC166Pair(address, bytes, memory, 2);
		}

		std::vector<uint32_t> decodeReferences4(uint32_t address, const uint8_t *bytes, const Pattern::Memory &memory) const override {
			// TASKING large model: MOV Rn, #POF/SOF(target); MOV Rn+1, #PAG/SEG(target).
			return decodeC166Pair(address, bytes, memory, 4);
		}

		std::tuple<bool, uint32_t, size_t> decodeReference(uint32_t, const Pattern::Memory &) const override {
			return { false, 0, 0 };
		}

		std::tuple<bool, uint32_t, size_t> decodeBranchReference(uint32_t offset, const Pattern::Memory &memory) const override {
			if (offset + 2 > memory.size)
				return { false, 0, 0 };

			uint32_t address = memory.base + offset;
			auto [isBranch2, branchAddress2] = decodeBranch2(address, memory.data + offset, memory);
			if (isBranch2 && Pattern::inMemory(memory, branchAddress2, 2))
				return { true, resolveThunks(branchAddress2, memory), 2 };

			if (offset + 4 <= memory.size) {
				auto [isBranch4, branchAddress4] = decodeBranch4(address, memory.data + offset, memory);
				if (isBranch4 && Pattern::inMemory(memory, branchAddress4, 2))
					return { true, resolveThunks(branchAddress4, memory), 4 };
			}

			return { false, 0, 0 };
		}

		size_t decodePointers(uint32_t address, const Pattern::Memory &memory,
			std::array<uint32_t, 2> &pointers) const override {
			if (!Pattern::inMemory(memory, address, 4))
				return 0;

			const uint8_t *bytes = memory.data + (address - memory.base);
			uint32_t segmentOffset = bytes[0] | (static_cast<uint32_t>(bytes[1]) << 8);
			uint32_t segment = bytes[2];
			pointers[0] = (segment << 16) | segmentOffset;
			size_t count = 1;

			uint32_t page = bytes[2] | (static_cast<uint32_t>(bytes[3]) << 8);
			if (segmentOffset < 0x4000 && page < 0x400) {
				uint32_t pointer = (page << 14) | segmentOffset;
				if (pointers[0] != pointer)
					pointers[count++] = pointer;
			}
			return count;
		}

		uint32_t resolveThunks(uint32_t address, const Pattern::Memory &memory) const override {
			for (unsigned depth = 0; depth < 16 && Pattern::inMemory(memory, address, 2); depth++) {
				const uint8_t *bytes = memory.data + (address - memory.base);
				auto [success, target] = bytes[0] == 0x0D ?
					decodeBranch2(address, bytes, memory) :
					std::make_pair(false, 0U);

				if (!success && Pattern::inMemory(memory, address, 4) &&
					(bytes[0] == 0xFA || (bytes[0] == 0xEA && bytes[1] == 0x00)))
					std::tie(success, target) = decodeBranch4(address, bytes, memory);

				if (!success || target == address || !Pattern::inMemory(memory, target, 2))
					break;

				if (spdlog::should_log(spdlog::level::debug)) {
					spdlog::debug("Found C166 thunk at {:08X}: PC->{:08X}", address, target);
				}
				address = target;
			}
			return address;
		}

		uint32_t offsetValue(uint32_t address, uint32_t, const Pattern::Memory &) const override {
			return address;
		}

		bool sameAddress(uint32_t first, uint32_t second) const override {
			return first == second;
		}

		int reference4Align() const override {
			return 2;
		}
};

}; // namespace

std::pair<std::vector<uint32_t>, size_t> Arch::decodeReferences(uint32_t offset, const Pattern::Memory &memory) const {
	if (offset + 4 <= memory.size) {
		auto references = decodeReferences4(memory.base + offset, memory.data + offset, memory);
		if (!references.empty())
			return { references, 4 };
	}

	if (offset + 2 <= memory.size) {
		auto references = decodeReferences2(memory.base + offset, memory.data + offset, memory);
		if (!references.empty())
			return { references, 2 };
	}
	return { { }, 0 };
}

std::pair<bool, uint32_t> Arch::decodePointer(uint32_t address, const Pattern::Memory &memory) const {
	std::array<uint32_t, 2> pointers;
	size_t count = decodePointers(address, memory, pointers);
	return count == 0 ?
		std::make_pair(false, 0U) :
		std::make_pair(true, pointers[0]);
}

const Arch &getArch(Architecture architecture) {
	static const ArmArch arm;
	static const C166Arch c166;

	switch (architecture) {
		case ARCH_ARM:
			return arm;
		case ARCH_C166:
			return c166;
	}
	throw std::runtime_error("Unsupported architecture.");
}

}; // namespace Ptr89
