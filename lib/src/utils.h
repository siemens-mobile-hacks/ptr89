#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace Ptr89 {

std::pair<int, int> getLocByOffset(const std::string &text, int offset);
std::string codeFrame(const std::string &text, int lineNum, int colNum);
std::string padStart(const std::string &line, int maxLength, char c);
std::string str2spaces(const std::string &line);
std::string tab2spaces(const std::string &line);

std::string strJoin(const std::string &sep, const std::vector<std::string> &lines);
std::vector<std::string> strSplit(const std::string &sep, const std::string &str);
std::string strprintf(const char *format, ...)  __attribute__((format(printf, 1, 2)));

}; // namespace Ptr89
