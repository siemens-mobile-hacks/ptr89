#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <filesystem>
#include <ptr89.h>
#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

std::pair<uint8_t *, size_t> readBinaryFile(const std::string &path);
