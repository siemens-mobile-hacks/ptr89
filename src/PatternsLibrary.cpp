#include "PatternsLibrary.h"
#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <stdexcept>
#include <toml++/toml.hpp>
#include <utility>

struct PlatformGroup {
	std::string name;
	std::vector<std::string> platforms;
};

static std::vector<PlatformGroup> parsePlatformGroups(const toml::table &root, const std::string &platform) {
	auto groupsTable = root["platforms"].as_table();
	if (!groupsTable)
		throw std::runtime_error("Missing [platforms] table.");

	bool knownPlatform = false;
	std::vector<PlatformGroup> groups;
	for (const auto &[groupName, value]: *groupsTable) {
		auto values = value.as_array();
		if (!values)
			throw std::runtime_error(std::format("platforms.{} must be an array.", groupName.str()));

		PlatformGroup group = { std::string(groupName.str()), { } };
		for (const auto &item: *values) {
			auto name = item.value<std::string>();
			if (!name)
				throw std::runtime_error(std::format("platforms.{} must contain only strings.", groupName.str()));
			group.platforms.push_back(*name);
			if (*name == platform)
				knownPlatform = true;
		}
		groups.push_back(std::move(group));
	}

	if (!knownPlatform)
		throw std::runtime_error(std::format("Unknown platform '{}'.", platform));

	std::stable_sort(groups.begin(), groups.end(), [](const PlatformGroup &a, const PlatformGroup &b) {
		return a.platforms.size() < b.platforms.size();
	});
	return groups;
}

static const toml::node *findPlatformNode(const toml::table &entry, const std::string &prefix,
	const std::string &platform, const std::vector<PlatformGroup> &groups) {
	if (auto value = entry.get(prefix + platform))
		return value;

	for (const auto &group: groups) {
		if (group.name == platform)
			continue;
		if (std::find(group.platforms.begin(), group.platforms.end(), platform) != group.platforms.end()) {
			if (auto value = entry.get(prefix + group.name))
				return value;
		}
	}
	return nullptr;
}

static std::vector<std::string> parsePatterns(const toml::node *node, const std::string &entryName) {
	if (!node)
		return { };
	if (auto pattern = node->value<std::string>())
		return { *pattern };
	if (auto enabled = node->value<bool>()) {
		if (!*enabled)
			return { };
	}
	if (auto values = node->as_array()) {
		std::vector<std::string> patterns;
		for (const auto &value: *values) {
			auto pattern = value.value<std::string>();
			if (!pattern)
				throw std::runtime_error(std::format("Patterns for [{}] must contain only strings.", entryName));
			patterns.push_back(*pattern);
		}
		return patterns;
	}
	throw std::runtime_error(std::format("Pattern for [{}] must be a string, an array of strings, or false.", entryName));
}

static int parseId(std::string_view value) {
	int id = 0;
	auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), id, 16);
	if (error != std::errc() || end != value.data() + value.size() || id < 0)
		throw std::runtime_error(std::format("Invalid function ID '{}'.", value));
	return id;
}

std::vector<PatternsLibraryItem> parsePatternsToml(const std::string &tomlFile, const std::string &platform) {
	auto root = toml::parse_file(tomlFile);
	auto groups = parsePlatformGroups(root, platform);
	std::vector<PatternsLibraryItem> results;

	for (const auto &[name, value]: root) {
		if (name == "platforms")
			continue;
		auto entry = value.as_table();
		if (!entry)
			throw std::runtime_error(std::format("{} must be a table.", name.str()));

		auto declNode = findPlatformNode(*entry, "decl_", platform, groups);
		if (!declNode)
			declNode = entry->get("decl");
		auto funcName = declNode ? declNode->value<std::string>() : std::optional<std::string>{ };
		if (!funcName)
			throw std::runtime_error(std::format("Declaration for [{}] must be a string.", name.str()));

		auto patternNode = findPlatformNode(*entry, "", platform, groups);
		results.push_back({ parseId(name.str()), *funcName, parsePatterns(patternNode, std::string(name.str())) });
	}

	std::sort(results.begin(), results.end(), [](const PatternsLibraryItem &a, const PatternsLibraryItem &b) {
		return a.id < b.id;
	});
	return results;
}
