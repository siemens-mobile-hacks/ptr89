#include "utils.h"

#include <cstring>
#include <cstdarg>
#include <stdexcept>

namespace Ptr89 {

std::pair<int, int> getLocByOffset(const std::string &text, int offset) {
	int line = 1;
	int col = 1;
	for (int i = 0; i < text.size(); i++) {
		if (text[i] == '\n') {
			line++;
			col = 1;
		}
		if (i == offset)
			return { line, col };
		col++;
	}
	return { line, col };
}

std::string codeFrame(const std::string &text, int lineNum, int colNum) {
	auto lines = strSplit("\n", text);
	int maxLineNumLen = std::to_string(lines.size()).size() + 1;
	std::string out = "";
	int n = 1;
	for (auto &line: lines) {
		if (abs(n - lineNum) > 3) {
			n++;
			continue;
		}
		out += (n == lineNum ? '>' : ' ') +
			padStart(std::to_string(n), maxLineNumLen, ' ') +
			" | " + tab2spaces(line) + "\n";
		if (n == lineNum) {
			for (int j = 0; j < maxLineNumLen + 1; j++)
				out += " ";
			out += " | " + str2spaces(line.substr(0, colNum - 1)) + "^";
		}
		n++;
	}
	return out;
}

std::string padStart(const std::string &line, int maxLength, char c) {
	std::string newStr = line;
	while (newStr.size() < maxLength)
		newStr = c + newStr;
	return newStr;
}

std::string str2spaces(const std::string &line) {
	std::string newStr = "";
	for (int i = 0; i < line.size(); i++) {
		newStr += ' ';
	}
	return newStr;
}

std::string tab2spaces(const std::string &line) {
	std::string newStr = "";
	int virtualSymbols = 0;
	for (int i = 0; i < line.size(); i++) {
		char c = line[i];
		if (c == '\t') {
			int spacesCnt = 4 - virtualSymbols % 4;
			for (int j = 0; j < spacesCnt; j++)
				newStr += " ";
			virtualSymbols += spacesCnt;
		} else {
			virtualSymbols++;
			newStr += c;
		}
	}
	return newStr;
}

std::string strJoin(const std::string &sep, const std::vector<std::string> &lines) {
	std::string out;
	size_t length = lines.size() > 1 ? sep.size() * (lines.size() - 1) : 0;
	for (auto &line: lines)
		length += line.size();

	out.reserve(length);

	bool first = true;
	for (auto &line: lines) {
		if (first) {
			first = false;
			out += line;
		} else {
			out += sep + line;
		}
	}

	return out;
}

std::vector<std::string> strSplit(const std::string &sep, const std::string &str) {
	std::vector<std::string> result;
	size_t last_pos = 0;

	while (true) {
		size_t pos = str.find(sep, last_pos);
		if (pos == std::string::npos) {
			result.push_back(str.substr(last_pos));
			break;
		} else {
			result.push_back(str.substr(last_pos, pos - last_pos));
			last_pos = pos + 1;
		}
	}

	return result;
}

std::string strprintf(const char *format, ...) {
	va_list v;

	std::string out;

	va_start(v, format);
	int n = vsnprintf(nullptr, 0, format, v);
	va_end(v);

	if (n <= 0)
		throw std::runtime_error("vsnprintf error...");

	out.resize(n);

	va_start(v, format);
	vsnprintf(&out[0], out.size() + 1, format, v);
	va_end(v);

	return out;
}

}; // namespace Ptr89
