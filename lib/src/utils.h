#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <concepts>

namespace Ptr89 {

std::pair<int, int> getLocByOffset(const std::string &text, int offset);
std::string codeFrame(const std::string &text, int lineNum, int colNum);
std::string padStart(const std::string &line, int maxLength, char c);
std::string str2spaces(const std::string &line);
std::string tab2spaces(const std::string &line);

std::string strJoin(const std::string &sep, const std::vector<std::string> &lines);
std::vector<std::string> strSplit(const std::string &sep, const std::string &str);

#if defined(_MSC_VER)
std::string strprintf(const char *format, ...);
#else
std::string strprintf(const char *format, ...)  __attribute__((format(printf, 1, 2)));
#endif

template<std::signed_integral T>
constexpr auto toUnsigned(T const value) {
	return static_cast<std::make_unsigned_t<T>>(value);
}

template<std::unsigned_integral T>
constexpr auto toSigned(T const value) {
	return static_cast<std::make_signed_t<T>>(value);
}

}; // namespace Ptr89
