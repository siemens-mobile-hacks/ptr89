#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cassert>
#include <regex>
#include <filesystem>
#include <ptr89.h>
#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

struct PatternsLibraryItem {
	int id;
	std::string funcName;
	std::string pattern;
};

std::string readFile(const std::string &path);
std::pair<uint8_t *, size_t> readBinaryFile(const std::string &path);
std::vector<PatternsLibraryItem> parsePatternsIni(const std::string &iniFile);
std::string trim(std::string s);
