#include "main.h"
#include <cstdint>

using json = nlohmann::json;
using namespace Ptr89;

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("ptr89");

	program.add_argument("-f", "--file")
		.required()
		.nargs(1);
	program.add_argument("-b", "--base")
		.default_value("A0000000")
		.nargs(1);
	program.add_argument("-a", "--align")
		.default_value(1)
		.nargs(1);
	program.add_argument("-p", "--pattern")
		.append()
		.default_value("")
		.nargs(1);
	program.add_argument("--from-ini")
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
		std::cerr << "  -b, --base HEX           fullflash base address [default: A0000000]\n";
		std::cerr << "  -a, --align N            search align [default: 1]\n";
		std::cerr << "  -V, --verbose            enable debug\n";
		std::cerr << "  -J, --json               output as JSON\n";
		std::cerr << "\n";
		std::cerr << "Find patterns:\n";
		std::cerr << "  -p, --pattern STRING     pattern to search\n";
		std::cerr << "\n";
		std::cerr << "Find patterns from functions.ini:\n";
		std::cerr << "  --from-ini FILE          path to functions.ini\n";
	};

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "ERROR: " << err.what() << "\n\n";
		showHelp();
		return 1;
	}

	if (program.is_used("--help")) {
		showHelp();
		return 1;
	}

	if (program.get<bool>("--verbose"))
		Pattern::setDebugHandler(vprintf);

	uint32_t memoryBase = stol(program.get<std::string>("--base"), NULL, 16);
	int memoryAlign = program.get<int>("--align");
	if (memoryAlign <= 0)
		throw std::runtime_error("Invalid align value.");

	auto [memory, memorySize] = readBinaryFile(program.get<std::string>("--file"));
	Pattern::Memory memoryRegion = { memoryBase, memory, memorySize, memoryAlign };

	auto asJSON = program.get<bool>("--json");
	json j;

	if (program.is_used("--pattern")) {
		auto patterns = program.get<std::vector<std::string>>("--pattern");
		j["patterns"] = json::array();

		auto start = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		for (auto &patternStr: patterns) {
			auto pattern = Pattern::parse(patternStr);
			auto results = Pattern::find(pattern, memoryRegion);
			if (asJSON) {
				json patternJson;
				patternJson["pattern"] = patternStr;
				patternJson["results"] = json::array();
				for (auto &result: results) {
					json item;
					item["address"] = result.address;
					item["offset"] = result.offset;
					item["value"] = result.value;

					if (pattern->type == PATTERN_TYPE_OFFSET) {
						item["type"] = "offset";
					} else if (pattern->type == PATTERN_TYPE_POINTER) {
						item["type"] = "pointer";
					} else if (pattern->type == PATTERN_TYPE_REFERENCE) {
						item["type"] = "reference";
					} else if (pattern->type == PATTERN_TYPE_STATIC_VALUE) {
						item["type"] = "static_value";
					}

					patternJson["results"].push_back(item);
				}
				j["patterns"].push_back(patternJson);
			} else {
				printf("Pattern: '%s'\n", patternStr.c_str());
				printf("Found %ld matches:\n", results.size());
				for (auto &result: results) {
					if (pattern->type == PATTERN_TYPE_OFFSET) {
						printf("  %08X: %08X (offset)\n", result.address, result.value);
					} else if (pattern->type == PATTERN_TYPE_POINTER) {
						printf("  %08X: %08X (pointer)\n", result.address, result.value);
					} else if (pattern->type == PATTERN_TYPE_REFERENCE) {
						printf("  %08X: %08X (reference)\n", result.address, result.value);
					} else if (pattern->type == PATTERN_TYPE_STATIC_VALUE) {
						printf("  %08X (static value)\n", result.value);
					}
				}
				printf("\n");
			}
		}
		auto end = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		j["elapsed"] = end - start;

		if (!asJSON) {
			printf("Search done in %ld ms\n", end - start);
		}
	} else if (program.is_used("--from-ini")) {
		auto start = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		auto patternsLib = parsePatternsIni(program.get<std::string>("--from-ini"));

		j["patterns"] = json::array();

		for (auto &entry: patternsLib) {
			auto pattern = Pattern::parse(entry.pattern);
			auto results = Pattern::find(pattern, memoryRegion, 1);

			if (asJSON) {
				json patternJson;
				patternJson["pattern"] = entry.pattern;
				patternJson["id"] = entry.id;
				patternJson["function"] = entry.funcName;
				patternJson["results"] = json::array();
				for (auto &result: results) {
					json item;
					item["address"] = result.address;
					item["offset"] = result.offset;
					item["value"] = result.value;

					if (pattern->type == PATTERN_TYPE_OFFSET) {
						item["type"] = "offset";
					} else if (pattern->type == PATTERN_TYPE_POINTER) {
						item["type"] = "pointer";
					} else if (pattern->type == PATTERN_TYPE_REFERENCE) {
						item["type"] = "reference";
					} else if (pattern->type == PATTERN_TYPE_STATIC_VALUE) {
						item["type"] = "static_value";
					}

					patternJson["results"].push_back(item);
				}
				j["patterns"].push_back(patternJson);
			} else {
				if (entry.id > 0 && (entry.id & 0xF) == 0)
					printf("\n");

				if (results.size() > 0 && results[0].value != 0xFFFFFFFF) {
					auto result = results[0];
					printf("%04X: 0x%08X   ;%4X: %s\n", entry.id * 4, result.value, entry.id, entry.funcName.c_str());
				} else {
					printf(";%03X:              ;%4X: %s\n", entry.id * 4, entry.id, entry.funcName.c_str());
				}
			}
		}
		auto end = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		j["elapsed"] = end - start;
	}

	if (asJSON) {
		printf("%s\n", j.dump(2).c_str());
	}

	delete[] memory;

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

	char buff[4096];
	size_t readed = 0;
	while (!feof(fp) && readed < maxFileSize) {
		int ret = fread(bytes + readed, 1, std::min(4096LU, maxFileSize - readed), fp);
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
