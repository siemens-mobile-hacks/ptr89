#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cassert>
#include <filesystem>
#include <ptr89.h>
#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace Ptr89;

std::pair<uint8_t *, size_t> readBinaryFile(const std::string &path);
int64_t getCurrentTimestamp();

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("ptr89");
	program.add_argument("-f", "--file")
		.help("fullflash dump file")
		.required()
		.nargs(1);
	program.add_argument("-b", "--base")
		.help("fullflash base address (HEX)")
		.default_value("A0000000")
		.nargs(1);
	program.add_argument("-p", "--pattern")
		.help("pattern to search")
		.append()
		.default_value("");
	program.add_argument("-V", "--verbose")
		.help("enable debug information output")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);
	program.add_argument("-J", "--json")
		.help("JSON output")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	if (program.get<bool>("--verbose"))
		Pattern::setDebugHandler(vprintf);

	auto [memory, memorySize] = readBinaryFile(program.get<std::string>("--file"));
	Pattern::Memory memoryRegion = { 0xA0000000, memory, memorySize };

	auto asJSON = program.get<bool>("--json");

	auto patterns = program.get<std::vector<std::string>>("--pattern");
	if (patterns.size() > 0) {
		json j;
		j["patterns"] = json::array();

		auto start = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		for (auto &patternStr: patterns) {
			auto pattern = Pattern::parse(patternStr);
			auto results = Pattern::find(pattern, memoryRegion);
			if (asJSON) {
				json row;
				row["pattern"] = patternStr;
				row["results"] = json::array();
				for (auto &result: results) {
					json item;
					item["address"] = result.address;
					item["offset"] = result.offset;
					item["value"] = result.value;

					if (pattern->type == PATTERN_TYPE_OFFSET) {
						item["type"] = "offset";
					} else if (pattern->type == PATTERN_TYPE_POINTER) {
						item["type"] = "pointer";
					} else if (pattern->type == Ptr89::PATTERN_TYPE_REFERENCE) {
						item["type"] = "reference";
					}

					row["results"].push_back(item);
				}
				j["patterns"].push_back(row);
			} else {
				printf("Pattern: '%s'\n", patternStr.c_str());
				printf("Found %ld matches:\n", results.size());
				for (auto &result: results) {
					if (pattern->type == PATTERN_TYPE_OFFSET) {
						printf("  %08X\n", result.address);
					} else if (pattern->type == PATTERN_TYPE_POINTER) {
						printf("  %08X: %08X\n", result.address, result.value);
					} else if (pattern->type == Ptr89::PATTERN_TYPE_REFERENCE) {
						printf("  %08X: %08X\n", result.address, result.value);
					}
				}
				printf("\n");
			}
		}
		auto end = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		if (asJSON) {
			j["elapsed"] = end - start;
			printf("%s\n", j.dump(2).c_str());
		} else {
			printf("Search done in %ld ms\n", end - start);
		}
	}

	delete[] memory;

	return 0;
}

std::pair<uint8_t *, size_t> readBinaryFile(const std::string &path) {
	FILE *fp = fopen(path.c_str(), "r");
	if (!fp) {
		throw std::runtime_error("fopen(" + path + ") error: " + strerror(errno));
	}

	size_t max_file_size = std::filesystem::file_size(path);
	uint8_t *bytes = new uint8_t[max_file_size];

	char buff[4096];
	size_t readed = 0;
	while (!feof(fp) && readed < max_file_size) {
		int ret = fread(bytes + readed, 1, std::min(4096LU, max_file_size - readed), fp);
		if (ret > 0) {
			readed += ret;
		} else if (ret < 0) {
			throw std::runtime_error("fread(" + path + ") error: " + strerror(errno));
		}
	}
	fclose(fp);

	return { bytes, readed };
}
