#include "main.h"
#include "src/Pattern.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using namespace Ptr89;

static constexpr uint64_t C166_ADDRESS_SPACE_SIZE = 0x1000000;

static const char *getSearchTypeName(ResultType type) {
	return type == RESULT_TYPE_OFFSET || type == RESULT_TYPE_ADDRESS ?
		"address" :
		Pattern::getResultTypeName(type);
}

static std::string getResultBytes(uint32_t offset, size_t resultSize, const Pattern::Memory &memory, bool showBytes) {
	if (resultSize == 0 || offset >= memory.size)
		return "-";

	std::string bytes;
	bool truncated = !showBytes && resultSize > 16;
	size_t size = std::min({ resultSize, memory.size - offset, showBytes ? resultSize : size_t{ 16 } });
	for (size_t i = 0; i < size; i++) {
		if (!bytes.empty())
			bytes += ' ';
		bytes += std::format("{:02X}", memory.data[offset + i]);
	}
	if (truncated)
		bytes += " …";
	return bytes;
}

static std::string getResultBytesJSON(uint32_t offset, size_t resultSize, const Pattern::Memory &memory) {
	std::string bytes;
	size_t size = offset < memory.size ? std::min(resultSize, memory.size - offset) : 0;
	bytes.reserve(size * 2);
	for (size_t i = 0; i < size; i++)
		bytes += std::format("{:02X}", memory.data[offset + i]);
	return bytes;
}

static json searchResultToJSON(const Pattern::SearchResult &result, const Pattern::Memory &memory) {
	json item = { { "address", result.address } };
	if (result.size != 0) {
		item["offset"] = result.offset;
		item["bytes"] = getResultBytesJSON(result.offset, result.size, memory);
	}
	return item;
}

static json xrefToJSON(const Pattern::XRef &xref, const Pattern::Memory &memory) {
	return {
		{ "xref", xref.address },
		{ "offset", xref.offset },
		{ "type", Pattern::getResultTypeName(xref.type) },
		{ "bytes", getResultBytesJSON(xref.offset, xref.size, memory) },
	};
}

static void printResults(const std::vector<Pattern::SearchResult> &results, const Pattern::Memory &memory, bool showBytes) {
	if (results.empty())
		return;

	std::cout << "  ADDRESS   OFFSET    BYTES\n";
	for (const auto &result: results) {
		if (result.size == 0) {
			std::cout << std::format("  {:08X}  {:<9} {}\n", result.address, "-", getResultBytes(result.offset, result.size, memory, showBytes));
		} else {
			std::cout << std::format("  {:08X}  {:08X}  {}\n",
				result.address, result.offset, getResultBytes(result.offset, result.size, memory, showBytes));
		}
	}
}

static void printXRefs(const std::vector<Pattern::XRef> &results, const Pattern::Memory &memory, bool showBytes) {
	if (results.empty())
		return;

	std::cout << "  OFFSET    XREF      KIND       BYTES\n";
	for (const auto &result: results) {
		std::cout << std::format("  {:08X}  {:08X}  {:<10} {}\n",
			result.offset, result.address, Pattern::getResultTypeName(result.type),
			getResultBytes(result.offset, result.size, memory, showBytes));
	}
}

static void printResultCount(size_t count, ResultType type) {
	const char *name;
	const char *plural;
	switch (type) {
		case RESULT_TYPE_OFFSET:
			name = "address";
			plural = "addresses";
			break;
		case RESULT_TYPE_POINTER:
			name = "pointer";
			plural = "pointers";
			break;
		case RESULT_TYPE_REFERENCE:
			name = "reference";
			plural = "references";
			break;
		case RESULT_TYPE_BRANCH:
			name = "branch";
			plural = "branches";
			break;
		case RESULT_TYPE_ADDRESS:
			name = "address";
			plural = "addresses";
			break;
	}
	std::cout << std::format("Found {} {}:\n", count, count == 1 ? name : plural);
}

static void printXRefCount(size_t count) {
	std::cout << std::format("Found {} {}:\n", count, count == 1 ? "xref" : "xrefs");
}

int main(int argc, char *argv[]) {
	spdlog::set_default_logger(spdlog::stderr_color_mt("ptr89"));
	spdlog::set_pattern("%v");
	spdlog::set_level(spdlog::level::warn);

	argparse::ArgumentParser program("ptr89", "2.0.0");

	program.add_argument("-f", "--file")
		.required()
		.nargs(1);
	program.add_argument("-b", "--base")
		.default_value("")
		.nargs(1);
	program.add_argument("-A", "--arch")
		.default_value("arm")
		.nargs(1);
	program.add_argument("-a", "--align")
		.default_value(1)
		.nargs(1)
		.scan<'i', int>();
	program.add_argument("-p", "--pattern")
		.append()
		.default_value("")
		.nargs(1);
	program.add_argument("-x", "--xref", "--xrefs")
		.append()
		.default_value("")
		.nargs(1);
	program.add_argument("-n", "--limit")
		.default_value(100)
		.nargs(1)
		.scan<'i', int>();
	program.add_argument("--from-ini")
		.default_value("")
		.nargs(1);
	program.add_argument("--prettify")
		.default_value("")
		.nargs(1);
	program.add_argument("-V", "--verbose")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);
	program.add_argument("-J", "--json")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);
	program.add_argument("--show-bytes")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);
	program.add_argument("-h", "--help")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);

	auto showHelp = []() {
		std::cerr << "Usage: ptr89 [arguments]\n";
		std::cerr << "\n";
		std::cerr << "Global options:\n";
		std::cerr << "  -h, --help               show this help\n";
		std::cerr << "  -f, --file FILE          fullflash file [required]\n";
		std::cerr << "  -b, --base HEX           fullflash base address [default: A0000000 for arm, auto for c166]\n";
		std::cerr << "  -A, --arch ARCH          architecture: arm or c166 [default: arm]\n";
		std::cerr << "  -a, --align N            search align [default: 1]\n";
		std::cerr << "  -V, --verbose            enable debug logs\n";
		std::cerr << "  -J, --json               output as JSON\n";
		std::cerr << "      --show-bytes         show all bytes for long results\n";
		std::cerr << "\n";
		std::cerr << "Find patterns:\n";
		std::cerr << "  -p, --pattern STRING     pattern to search\n";
		std::cerr << "  -n, --limit NUMBER       limit results count [default 100]\n";
		std::cerr << "\n";
		std::cerr << "Find xrefs:\n";
		std::cerr << "  -x, --xref HEX           address to search\n";
		std::cerr << "  -n, --limit NUMBER       limit results count [default 100]\n";
		std::cerr << "\n";
		std::cerr << "Find patterns from functions.ini:\n";
		std::cerr << "  --from-ini FILE          path to functions.ini\n";
		std::cerr << "\n";
		std::cerr << "Prettify pattern:\n";
		std::cerr << "  --prettify STRING        pattern\n";
		std::cerr << "\n";
	};

	json j;

	try {
		program.parse_args(argc, argv);

		if (program.is_used("--help")) {
			showHelp();
			return 1;
		}

		if (program.get<bool>("--verbose"))
			spdlog::set_level(spdlog::level::debug);

		auto archName = program.get<std::string>("--arch");
		Architecture arch;
		if (archName == "arm") {
			arch = ARCH_ARM;
		} else if (archName == "c166") {
			arch = ARCH_C166;
		} else {
			throw std::runtime_error("Invalid architecture '" + archName + "'. Expected arm or c166.");
		}

		int memoryAlign = program.get<int>("--align");
		if (memoryAlign <= 0)
			throw std::runtime_error("Invalid align value.");

		auto [memory, memorySize] = readBinaryFile(program.get<std::string>("--file"));
		std::unique_ptr<uint8_t[]> memoryHolder(memory);

		uint32_t memoryBase;
		if (program.is_used("--base")) {
			memoryBase = stoll(program.get<std::string>("--base"), NULL, 16);
		} else if (arch == ARCH_C166) {
			if (memorySize > C166_ADDRESS_SPACE_SIZE)
				throw std::runtime_error("C166 fullflash is larger than the 16 MiB address space; specify --base explicitly.");
			memoryBase = static_cast<uint32_t>(C166_ADDRESS_SPACE_SIZE - memorySize);
		} else {
			memoryBase = 0xA0000000;
		}

		Pattern::Memory memoryRegion = { memoryBase, memory, memorySize, memoryAlign, arch };

		auto asJSON = program.get<bool>("--json");
		auto showBytes = program.get<bool>("--show-bytes");
		if (program.is_used("--pattern")) {
			uint32_t limit = program.get<int>("--limit");

			auto patterns = program.get<std::vector<std::string>>("--pattern");
			j["patterns"] = json::array();

			auto start = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			for (auto &patternStr: patterns) {
				auto pattern = Pattern::parse(patternStr);
				auto results = Pattern::find(pattern, memoryRegion, limit);
				if (asJSON) {
					json patternJson;
					patternJson["pattern"] = patternStr;
					patternJson["type"] = getSearchTypeName(pattern->type);
					patternJson["results"] = json::array();
					for (const auto &result: results)
						patternJson["results"].push_back(searchResultToJSON(result, memoryRegion));
					j["patterns"].push_back(patternJson);
				} else {
					std::cout << std::format("Pattern: '{}'\n", patternStr);
					printResultCount(results.size(), pattern->type);
					printResults(results, memoryRegion, showBytes);
					std::cout << '\n';
				}
			}
			auto end = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			if (!asJSON) {
				std::cout << std::format("Search done in {} ms\n", end - start);
			}
		} else if (program.is_used("--xrefs")) {
			uint32_t addr = stoll(program.get<std::string>("--xrefs"), NULL, 16);
			uint32_t limit = program.get<int>("--limit");

			j["target"] = addr;
			j["xrefs"] = json::array();
			auto start = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			auto results = Pattern::finXRefs(addr, memoryRegion, limit);
			if (asJSON) {
				for (const auto &result: results)
					j["xrefs"].push_back(xrefToJSON(result, memoryRegion));
			} else {
				std::cout << std::format("Xrefs to 0x{:08X}\n", addr);
				printXRefCount(results.size());
				printXRefs(results, memoryRegion, showBytes);
				std::cout << '\n';
			}
			auto end = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			if (!asJSON) {
				std::cout << std::format("Search done in {} ms\n", end - start);
			}
		} else if (program.is_used("--from-ini")) {
			auto patternsLib = parsePatternsIni(program.get<std::string>("--from-ini"));

			j["functions"] = json::array();

			for (auto &entry: patternsLib) {
				auto pattern = Pattern::parse(entry.pattern);
				auto results = Pattern::find(pattern, memoryRegion, 1);

				if (asJSON) {
					json functionJson = {
						{ "id", entry.id },
						{ "name", entry.funcName },
						{ "pattern", entry.pattern },
						{ "type", getSearchTypeName(pattern->type) },
					};
					functionJson["result"] = results.empty() || results[0].address == 0xFFFFFFFF ?
						json(nullptr) :
						searchResultToJSON(results[0], memoryRegion);
					j["functions"].push_back(functionJson);
				} else {
					if (entry.id > 0 && (entry.id & 0xF) == 0)
						std::cout << '\n';

					if (results.size() > 0 && results[0].address != 0xFFFFFFFF) {
						auto result = results[0];
						std::cout << std::format("{:04X}: 0x{:08X}   ;{:4X}: {}\n", entry.id * 4, result.address, entry.id, entry.funcName);
					} else {
						std::cout << std::format(";{:03X}:              ;{:4X}: {}\n", entry.id * 4, entry.id, entry.funcName);
					}
				}
			}
		} else if (program.is_used("--prettify")) {
			auto patternStr = program.get<std::string>("--prettify");
			if (asJSON) {
				j["pattern"] = Pattern::stringify(Pattern::parse(patternStr));
			} else {
				std::cout << std::format("Pattern: {}\n", Pattern::stringify(Pattern::parse(patternStr)));
			}
		}

		if (asJSON)
			std::cout << j.dump(2) << '\n';

	} catch (const std::exception &err) {
		if (program.get<bool>("--json")) {
			std::cout << json({ { "error", err.what() } }).dump(2) << '\n';
		} else {
			spdlog::error("ERROR: {}", err.what());
			std::cerr << '\n';
			showHelp();
		}
		return 1;
	}

	return 0;
}

std::vector<PatternsLibraryItem> parsePatternsIni(const std::string &iniFile) {
	auto iniText = readFile(iniFile);
	std::regex exp(R"(^[ \t]*([0-9a-f]+):[ \t]*([^=;\n]+)(?:[ \t]*=[ \t]*([^;:\n]*))?)", std::regex::icase | std::regex::multiline);
	std::smatch m;
	std::vector<PatternsLibraryItem> results;
	auto searchStart = iniText.cbegin();
	while (std::regex_search(searchStart, iniText.cend(), m, exp)) {
		auto id = stoi(m[1].str(), NULL, 16);
		auto funcName = trim(m[2].str());
		auto patternStr = trim(m[3].str());
		results.push_back({ id, funcName, patternStr });
		searchStart = m.suffix().first;
	}
	return results;
}

std::string readFile(const std::string &path) {
	FILE *fp = fopen(path.c_str(), "r");
	if (!fp) {
		throw std::runtime_error("fopen(" + path + ") error: " + strerror(errno));
	}

	char buff[4096];
	std::string result;
	while (!feof(fp)) {
		int readed = fread(buff, 1, sizeof(buff), fp);
		if (readed > 0)
			result.append(buff, readed);
	}
	fclose(fp);

	return result;
}

std::pair<uint8_t *, size_t> readBinaryFile(const std::string &path) {
	FILE *fp = fopen(path.c_str(), "r");
	if (!fp) {
		throw std::runtime_error("fopen(" + path + ") error: " + strerror(errno));
	}

	size_t maxFileSize = std::filesystem::file_size(path);
	uint8_t *bytes = new uint8_t[maxFileSize];

	size_t readed = 0;
	while (!feof(fp) && readed < maxFileSize) {
		int ret = fread(bytes + readed, 1, std::min(static_cast<size_t>(4096), maxFileSize - readed), fp);
		if (ret > 0) {
			readed += ret;
		} else if (ret < 0) {
			throw std::runtime_error("fread(" + path + ") error: " + strerror(errno));
		}
	}
	fclose(fp);

	return { bytes, readed };
}

std::string trim(std::string s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](uint8_t c) {
		return !isspace(c);
	}));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](uint8_t c) {
		return !isspace(c);
	}).base(), s.end());
	return s;
}
