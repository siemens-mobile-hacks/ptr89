#pragma once

#include <string>
#include <vector>
#include <stack>
#include "Pattern.h"
#include "Tokenizer.h"

namespace Ptr89 {

class Parser {
	protected:
		PtrExp m_pattern;
		std::string m_input;
		Tokenizer m_tok;
		void parseReferenceOrPointer();
		void parseOffsetPattern();
		void parseSubPattern(SubPatternType type, Tokenizer::TokenType openTag, Tokenizer::TokenType closeTag);
		void parseHexMask();
		void parseBinMask();
		bool parsePatternData();
		void parsePatternBody();
		int parseOffset();
		void skipWhitespaces();
		void expectToken(Tokenizer::TokenType tokenType);
		void expectTokens(const std::vector<Tokenizer::TokenType> tokenTypes);

		Tokenizer::TokenType getClosingToken(Tokenizer::TokenType tokenType);
		int getTokenInt(const Tokenizer::Token &token);
		std::string getTokenStr(const Tokenizer::Token &token);

		static inline int hex2byte(char c) {
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'A' && c <= 'F')
				return (c - 'A') + 10;
			if (c >= 'a' && c <= 'f')
				return (c - 'a') + 10;
			return -1;
		}
	public:
		std::shared_ptr<PtrExp> parse(const std::string &value);

		std::pair<int, int> getLocation() const;
		std::string getCodeFrame(const std::pair<int, int> &loc) const;
};

}; // namespace Ptr89
