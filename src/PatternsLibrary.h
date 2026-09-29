#pragma once

#include <string>
#include <vector>

struct PatternsLibraryItem {
	int id;
	std::string funcName;
	std::vector<std::string> patterns;
};

std::vector<PatternsLibraryItem> parsePatternsToml(const std::string &tomlFile, const std::string &platform);
