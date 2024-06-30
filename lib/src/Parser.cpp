#include "Parser.h"
#include "Pattern.h"
#include "Tokenizer.h"
#include "src/utils.h"
#include <cstdint>
#include <stdexcept>

namespace Ptr89 {

std::shared_ptr<PtrExp> Parser::parse(const std::string &input) {
	m_input = input;
	m_pattern = {};
	m_tok.reset(m_input);

	skipWhitespaces();

	switch (m_tok.peek().type) {
		case Tokenizer::TOK_POINTER:	// *( ... )
		case Tokenizer::TOK_REFERENCE:	// &( ... )
			parseReferenceOrPointer();
		break;

		case Tokenizer::TOK_VALUE_OPEN:
			parseStaticValue();
		break;

		case Tokenizer::TOK_PAREN_OPEN:	// (...)
		default:
			parseOffsetPattern();
		break;

		case Tokenizer::TOK_EOF:
			// Empty pattern
		break;

		case Tokenizer::TOK_INVALID:
			throw PatternError(this, "Syntax error");
		break;
	}

	skipWhitespaces();

	if (m_tok.peek().type != Tokenizer::TOK_EOF)
		throw PatternError(this, "Unexpected tokens after end of pattern");

	return std::make_shared<PtrExp>(m_pattern);
}

void Parser::parseStaticValue() {
	expectToken(Tokenizer::TOK_VALUE_OPEN);
	m_tok.next();

	skipWhitespaces();

	expectToken(Tokenizer::TOK_HEX);

	m_pattern.type = PATTERN_TYPE_STATIC_VALUE;
	m_pattern.staticValue = getTokenUInt(m_tok.peek());

	m_tok.next();

	skipWhitespaces();

	expectToken(Tokenizer::TOK_VALUE_CLOSE);
	m_tok.next();
}

void Parser::parseReferenceOrPointer() {
	if (m_tok.peek().type == Tokenizer::TOK_POINTER) {
		m_pattern.type = PATTERN_TYPE_POINTER;
	} else {
		m_pattern.type = PATTERN_TYPE_REFERENCE;
	}

	m_tok.next();

	expectToken(Tokenizer::TOK_PAREN_OPEN);
	m_tok.next();

	parsePatternBody();

	expectToken(Tokenizer::TOK_PAREN_CLOSE);
	m_tok.next();

	m_pattern.outputOffset = parseOffset();
}

void Parser::parseOffsetPattern() {
	m_pattern.type = PATTERN_TYPE_OFFSET;
	parsePatternBody();
}

void Parser::parsePatternBody() {
	skipWhitespaces();

	if (m_tok.peek().type == Tokenizer::TOK_PAREN_OPEN) {
		expectToken(Tokenizer::TOK_PAREN_OPEN);
		m_tok.next();

		while (parsePatternData());

		expectToken(Tokenizer::TOK_PAREN_CLOSE);
		m_tok.next();

		m_pattern.inputOffset = parseOffset();
	} else {
		while (parsePatternData());
		m_pattern.inputOffset = parseOffset();
	}
}

int Parser::parseOffset() {
	skipWhitespaces();
	if (m_tok.peek().type != Tokenizer::TOK_MINUS && m_tok.peek().type != Tokenizer::TOK_PLUS)
		return 0;

	bool isNegative = (m_tok.peek().type == Tokenizer::TOK_MINUS);
	m_tok.next();

	skipWhitespaces();
	expectToken(Tokenizer::TOK_HEX);
	return getTokenInt(m_tok.next()) * (isNegative ? -1 : 1);
}

void Parser::parseHexMask() {
	auto value = getTokenStr(m_tok.peek());
	if (value.size() % 2 != 0)
		throw PatternError(this, "The hex number length must be even");

	for (int i = 0; i < value.size(); i += 2) {
		uint16_t mask = 0;
		uint16_t byte = 0;

		if (value[i] != '?') {
			mask |= 0xF0;
			byte |= hex2byte(value[i]) << 4;
		}

		if (value[i + 1] != '?') {
			mask |= 0x0F;
			byte |= hex2byte(value[i + 1]);
		}

		m_pattern.bytes.push_back(byte);
		m_pattern.masks.push_back(mask);
	}

	m_tok.next();
}

void Parser::parseBinMask() {
	auto value = getTokenStr(m_tok.peek());
	uint16_t mask = 0;
	uint16_t byte = 0;
	for (int i = 1; i <= 8; i++) {
		int16_t bit = 1 << (8 - i);
		if (value[i] == '1') {
			mask |= bit;
			byte |= bit;
		} else if (value[i] == '0') {
			mask |= bit;
		}
	}
	m_pattern.bytes.push_back(byte);
	m_pattern.masks.push_back(mask);
	m_tok.next();
}

void Parser::parseSubPattern(SubPatternType type, Tokenizer::TokenType openTag, Tokenizer::TokenType closeTag) {
	PtrExp mainPattern = m_pattern;

	expectToken(openTag);
	m_tok.next();

	m_pattern = {};
	while (parsePatternData());

	PtrExp subPattern = m_pattern;
	m_pattern = mainPattern;

	int offset = m_pattern.bytes.size();
	m_pattern.subPatterns[offset] = {
		.type = type,
		.pattern = std::make_shared<PtrExp>(subPattern),
		.offset = offset,
		.size = 0
	};

	if (type == SUB_PATTERN_TYPE_BRANCH_2B) {
		for (int i = 0; i < 2; i++) {
			m_pattern.bytes.push_back(0);
			m_pattern.masks.push_back(0);
		}
		m_pattern.subPatterns[offset].size = 2;
	} else if (type == SUB_PATTERN_TYPE_BRANCH_4B) {
		for (int i = 0; i < 4; i++) {
			m_pattern.bytes.push_back(0);
			m_pattern.masks.push_back(0);
		}
		m_pattern.subPatterns[offset].size = 4;
	}

	expectToken(closeTag);
	m_tok.next();
}

void Parser::parseAsciiString() {
	auto value = getTokenStr(m_tok.peek());
	if (value.size() <= 2)
		throw PatternError(this, "Empty string not allowed");

	auto stringValue = value.substr(1, value.size() - 2);

	SubPtrExp subPattern = { };
	subPattern.type = SUB_PATTERN_TYPE_STRING;
	subPattern.pattern = std::make_shared<PtrExp>(PtrExp());

	for (int i = 1; i < value.size() - 2; i++) {
		uint8_t byte = (uint8_t) value[i];
		subPattern.pattern->bytes.push_back(byte);
		subPattern.pattern->masks.push_back(0xFF);
	}

	int offset = m_pattern.bytes.size();
	subPattern.offset = offset;
	subPattern.size = 4;
	m_pattern.subPatterns[offset] = subPattern;

	for (int i = 0; i < 4; i++) {
		m_pattern.bytes.push_back(0);
		m_pattern.masks.push_back(0);
	}

	m_tok.next();
}

bool Parser::parsePatternData() {
	switch (m_tok.peek().type) {
		case Tokenizer::TOK_HEX:
		case Tokenizer::TOK_MASK:
			parseHexMask();
		break;

		case Tokenizer::TOK_BIN:
			parseBinMask();
		break;

		case Tokenizer::TOK_2B_BRANCH_OPEN:	// [ ... ]
			parseSubPattern(SUB_PATTERN_TYPE_BRANCH_2B, Tokenizer::TOK_2B_BRANCH_OPEN, Tokenizer::TOK_2B_BRANCH_CLOSE);
		break;

		case Tokenizer::TOK_4B_BRANCH_OPEN: // { ... }
			parseSubPattern(SUB_PATTERN_TYPE_BRANCH_4B, Tokenizer::TOK_4B_BRANCH_OPEN, Tokenizer::TOK_4B_BRANCH_CLOSE);
		break;

		case Tokenizer::TOK_BLF: // _BLF(...)
			m_tok.next();
			parseSubPattern(SUB_PATTERN_TYPE_BRANCH_4B, Tokenizer::TOK_PAREN_OPEN, Tokenizer::TOK_PAREN_CLOSE);
		break;

		case Tokenizer::TOK_ASCII_STRING:
			parseAsciiString();
		break;

		case Tokenizer::TOK_SEPARATOR:
		case Tokenizer::TOK_WHITESPACE:
			m_tok.next();
		break;

		case Tokenizer::TOK_INVALID:
			throw PatternError(this, "Syntax error");
		break;

		default:
			// Data is end
			return false;
		break;
	}
	return true;
}

Tokenizer::TokenType Parser::getClosingToken(Tokenizer::TokenType tokenType) {
	switch (tokenType) {
		case Tokenizer::TOK_2B_BRANCH_OPEN:		return Tokenizer::TOK_2B_BRANCH_CLOSE;
		case Tokenizer::TOK_4B_BRANCH_OPEN:		return Tokenizer::TOK_4B_BRANCH_CLOSE;
		case Tokenizer::TOK_PAREN_OPEN:			return Tokenizer::TOK_PAREN_CLOSE;
		default:								return Tokenizer::TOK_INVALID;
	}
}

void Parser::expectTokens(const std::vector<Tokenizer::TokenType> tokenTypes) {
	for (const auto tokenType: tokenTypes) {
		if (m_tok.peek().type == tokenType)
			return;
	}
	if (m_tok.peek().type == Tokenizer::TOK_EOF) {
		throw PatternError(this, "Unexpected EOF");
	} else if (m_tok.peek().type == Tokenizer::TOK_INVALID) {
		throw PatternError(this, "Syntax error");
	} else {
		throw PatternError(this, "Unexpected token " + Tokenizer::getTokenName(m_tok.peek().type));
	}
}

void Parser::expectToken(Tokenizer::TokenType tokenType) {
	if (m_tok.peek().type == tokenType)
		return;
	if (m_tok.peek().type == Tokenizer::TOK_EOF) {
		throw PatternError(this, "Unexpected EOF");
	} else if (m_tok.peek().type == Tokenizer::TOK_INVALID) {
		throw PatternError(this, "Syntax error");
	} else {
		throw PatternError(this, "Unexpected token " + Tokenizer::getTokenName(m_tok.peek().type));
	}
}

void Parser::skipWhitespaces() {
	while (m_tok.peek().type == Tokenizer::TOK_WHITESPACE)
		m_tok.next();
}

int Parser::getTokenInt(const Tokenizer::Token &token) {
	return stoi(getTokenStr(token), nullptr, 16);
}

uint32_t Parser::getTokenUInt(const Tokenizer::Token &token) {
	return stol(getTokenStr(token), nullptr, 16);
}

std::string Parser::getTokenStr(const Tokenizer::Token &token) {
	return m_input.substr(token.start, token.end - token.start);
}

std::pair<int, int> Parser::getLocation() const {
	return getLocByOffset(m_input, m_tok.offset());
}

std::string Parser::getCodeFrame(const std::pair<int, int> &loc) const {
	return codeFrame(m_input, loc.first, loc.second);
}

}; // namespace Ptr89
