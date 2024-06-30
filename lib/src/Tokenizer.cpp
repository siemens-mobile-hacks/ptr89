#include "Tokenizer.h"
#include <cctype>
#include <strings.h>

namespace Ptr89 {

const Tokenizer::Token &Tokenizer::next() {
	if (m_nextToken.type == TOK_NULL)
		m_nextToken = parseToken();
	m_currentToken = m_nextToken;
	m_savedOffset = m_offset;
	m_nextToken = parseToken();
	return m_currentToken;
}

const Tokenizer::Token &Tokenizer::peek() {
	if (m_nextToken.type == TOK_NULL)
		m_nextToken = parseToken();
	return m_nextToken;
}

bool Tokenizer::isHex(char c) {
	return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9');
}

bool Tokenizer::isHexPattern(char c) {
	return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9') || c == '?';
}

bool Tokenizer::isBinPattern(char c) {
	return c == '1' || c == '0' || c == '.';
}

Tokenizer::Token Tokenizer::parseToken() {
	auto start = m_offset;

	if (m_offset == m_input.size()) {
		return { TOK_EOF, m_offset, m_offset };
	}

	while (isspace(m_input[m_offset])) {
		m_offset++;
		while (avail() > 0 && isspace(m_input[m_offset]))
			m_offset++;
		return { TOK_WHITESPACE, start, m_offset };
	}

	if (m_input[m_offset] == '[' && avail() >= 10 && m_input[m_offset + 9] == ']') {
		bool isBinaryPtr = false;
		for (int i = 1; i <= 8; i++) {
			if (!isBinPattern(m_input[m_offset + i])) {
				isBinaryPtr = false;
				break;
			}
			if (m_input[m_offset + i] == '.')
				isBinaryPtr = true;
		}
		if (isBinaryPtr) {
			m_offset += 10;
			return { TOK_BIN, start, start + 10  };
		}
	}

	switch (m_input[m_offset]) {
		case '&':
			m_offset++;
			return { TOK_REFERENCE, start, m_offset };
		break;
		case '*':
			m_offset++;
			return { TOK_POINTER, start, m_offset };
		break;
		case '(':
			m_offset++;
			return { TOK_PAREN_OPEN, start, m_offset };
		break;
		case ')':
			m_offset++;
			return { TOK_PAREN_CLOSE, start, m_offset };
		break;
		case '[':
			m_offset++;
			return { TOK_2B_BRANCH_OPEN, start, m_offset };
		break;
		case ']':
			m_offset++;
			return { TOK_2B_BRANCH_CLOSE, start, m_offset };
		break;
		case '{':
			m_offset++;
			return { TOK_4B_BRANCH_OPEN, start, m_offset };
		break;
		case '}':
			m_offset++;
			return { TOK_4B_BRANCH_CLOSE, start, m_offset };
		break;
		case '+':
			m_offset++;
			return { TOK_PLUS, start, m_offset };
		break;
		case '-':
			m_offset++;
			return { TOK_MINUS, start, m_offset };
		break;
		case ',':
			m_offset++;
			return { TOK_SEPARATOR, start, m_offset };
		break;
		case '<':
			m_offset++;
			return { TOK_VALUE_OPEN, start, m_offset };
		break;
		case '>':
			m_offset++;
			return { TOK_VALUE_CLOSE, start, m_offset };
		break;
	}

	if (m_input[m_offset] == '0' && avail() > 2 && tolower(m_input[m_offset + 1]) == 'x' && isHex(m_input[m_offset + 2])) {
		m_offset += 3;
		while (avail() > 0 && isHex(m_input[m_offset])) {
			m_offset++;
		}
		return { TOK_HEX, start, m_offset };
	}

	if (isHexPattern(m_input[m_offset])) {
		bool isMask = false;
		m_offset++;

		if (m_input[m_offset] == '?')
			isMask = true;

		while (avail() > 0 && isHexPattern(m_input[m_offset])) {
			if (m_input[m_offset] == '?')
				isMask = true;
			m_offset++;
		}

		return { (isMask ? TOK_MASK : TOK_HEX), start, m_offset };
	}

	if (m_input[m_offset] == '_' && strcasecmp(m_input.substr(m_offset, 4).c_str(), "_blf") == 0) {
		m_offset += 4;
		return { TOK_BLF, start, m_offset };
	}

	if (tolower(m_input[m_offset]) == 'l' && strcasecmp(m_input.substr(m_offset, 3).c_str(), "ldr") == 0) {
		m_offset += 3;
		return { TOK_LDR, start, m_offset };
	}

	if (m_input[m_offset] == '%') {
		m_offset++;
		while (avail() > 0) {
			if (m_input[m_offset] == '%') {
				m_offset++;
				return { TOK_NAMED_BRANCH, start, m_offset };
			}
			m_offset++;
		}
	}

	return { TOK_INVALID, start, m_offset };
}

std::string Tokenizer::getTokenName(TokenType type) {
	switch (type) {
		case TOK_EOF:				return "EOF";
		case TOK_NULL:				return "NULL";
		case TOK_INVALID:			return "INVALID";
		case TOK_REFERENCE:			return "REFERENCE";
		case TOK_POINTER:			return "POINTER";
		case TOK_BLF:				return "BLF";
		case TOK_LDR:				return "LDR";
		case TOK_4B_BRANCH_OPEN:	return "4B_BRANCH_OPEN";
		case TOK_4B_BRANCH_CLOSE:	return "4B_BRANCH_CLOSE";
		case TOK_2B_BRANCH_OPEN:	return "2B_BRANCH_OPEN";
		case TOK_2B_BRANCH_CLOSE:	return "2B_BRANCH_CLOSE";
		case TOK_PAREN_OPEN:		return "PAREN_OPEN";
		case TOK_PAREN_CLOSE:		return "PAREN_CLOSE";
		case TOK_VALUE_OPEN:		return "VALUE_OPEN";
		case TOK_VALUE_CLOSE:		return "VALUE_CLOSE";
		case TOK_SEPARATOR:			return "SEPARATOR";
		case TOK_WHITESPACE:		return "WHITESPACE";
		case TOK_PLUS:				return "PLUS";
		case TOK_MINUS:				return "MINUS";
		case TOK_HEX:				return "HEX";
		case TOK_MASK:				return "MASK";
		case TOK_BIN:				return "BIN";
		case TOK_NAMED_BRANCH:		return "NAMED_BRANCH";
	}
	return "UNKNOWN";
}

}; // namespace Ptr89
