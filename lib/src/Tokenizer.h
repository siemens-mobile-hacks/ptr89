#pragma once

#include <string>

namespace Ptr89 {

class Tokenizer {
	public:
		enum TokenType {
			TOK_EOF = -1,
			TOK_NULL,
			TOK_INVALID,
			TOK_REFERENCE,
			TOK_POINTER,
			TOK_BLF,
			TOK_4B_BRANCH_OPEN,
			TOK_4B_BRANCH_CLOSE,
			TOK_2B_BRANCH_OPEN,
			TOK_2B_BRANCH_CLOSE,
			TOK_PAREN_OPEN,
			TOK_PAREN_CLOSE,
			TOK_VALUE_OPEN,
			TOK_VALUE_CLOSE,
			TOK_SEPARATOR,
			TOK_WHITESPACE,
			TOK_PLUS,
			TOK_MINUS,
			TOK_HEX,
			TOK_MASK,
			TOK_BIN,
			TOK_ASCII_STRING,
		};

		struct Token {
			TokenType type;
			int start;
			int end;
		};

	protected:
		Token m_nextToken = { TOK_NULL };
		Token m_currentToken = { TOK_NULL };
		int m_offset = 0;
		int m_savedOffset = 0;
		std::string m_input;
		Token parseToken();
		static bool isHex(char c);
		static bool isHexPattern(char c);
		static bool isBinPattern(char c);
		inline int avail() {
			return m_input.size() - m_offset;
		}

	public:
		const Token &next();
		const Token &peek();
		inline void reset(const std::string &value) {
			m_input = value;
			m_offset = 0;
			m_savedOffset = 0;
			m_nextToken = { TOK_NULL };
		}

		inline int offset() const {
			return m_savedOffset;
		}

		static std::string getTokenName(TokenType type);
};

}; // namespace Ptr89
