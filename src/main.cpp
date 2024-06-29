#include <cstdint>
#include <cstdio>
#include <string>
#include <cassert>
#include <ptr89.h>
/*

# HEX
ABCD

# HEX MASK
AB??
A?C?

# Binary MASK
[1111.11.]

# Reference
&( ... )

# Pointer
*( ... )

# Sub-pattern: BL
{ }
_BLF( )

# Sub-pattern: B
[ ]

# Force value
< A0000000 >

# String (decode LDR and check string by addr)
%ascii%

*/

using namespace Ptr89;

int main() {
	Pattern::setDebugHandler(vprintf);

		// parser.parse("&(??4840687047,??4800687047,??B5 + 1) - 4");
		// auto pattern = parser.parse("??,1C,??,48,??,B5,??,68, {??,1C,??,68,??,68,??,2B,??,D0,??,68, [??,23, [??B5??1C??6E] ] ,??,47} ,??,BD + 1");
///		auto pattern = parser.parse("??,1C,??,48,??,B5,??,68, { ??,1C,??,68,??,68,??,2B,??,D0,??,68, [??,23, [??B5??1C??6E] ] ,??,47 },??,BD + 1");

//		auto pattern = parser.parse("&(??4840687047,??4800687047,??B5 + 1) - 4"); // THUMB LDR+LDR

//		auto pattern = parser.parse("&(70402DE9??669FE50050A0E10140A0E1+4)"); // ARM

//		auto pattern = parser.parse("&( 98 02 9f e5 00 00 90 e5 08 80 bd e8 )"); // ARM

//		auto pattern = parser.parse("&(04D000220021??48 + 6)"); // THUMB LDR

//		auto pattern = parser.parse("&(??,48,??,B5,??,68, {??,63,??,47}, ??,BD + 1)"); // THUMB LDR
		auto pattern = Pattern::parse("*(010080E2020051E15C008415AC008405????????0400A0E11040BDE8+20)"); // THUMB LDR


		//auto pattern = parser.parse("70 40 [0010....] e9 [0000....] [....0000] a0 e1 [0000....] [....0000] a0 e1 [........] [........] [0100....] e2 [........] [........] [1000....] e2 [........] [........] a0 e3 [........] [........] a0 e3 [........] [........] [........] eb [........] [0000....] [0101....] e3 [........] [........] [........] 0a [........] [........] a0 e3 [........] [........] [1000....] e2 [........] [........] [........] [1111101.] [0000....] [....0000] a0 e1 [........] [........] [1000....] e2 [........] [........] a0 e3 [........] [........] [........] [1111101.] [........] [0000....] [0101....] e3 [........] [0000....] [0101....] 13 [........] [........] [........] 0a [0000....] [....0000] [0100....] e0 [........] [0000....] [0101....] e3 [........] [........] [........] ba [........] [........] a0 e3 [........] [........] [1100....] e5 [........] [........] [1000....] e2 [0000....] [....0000] a0 e1 [........] [........] [........] [1111101.] [.000....] [........] a0 e1 [.010....] [........] a0 e1 [........] [........] [1000....] e2 ");

		//auto pattern = parser.parse("{ AA } [........] [........] [1001....] e5 [........] [0000....] [0101....] e3 [........] [0000....] [0101....] 13 [........] [........] [1001....] 15 [........] [0000....] [0101....] 13 [........] [........] [1001....] 15 [........] [0000....] [0101....] 13 [........] [0000....] [0101....] 13 [........] [0000....] [0101....] 13 [........] [0000....] [0101....] 13 [........] [0000....] [0101....] 13 [........] [0000....] [0101....] 13 [........] [........] e0 03 [........] [........] [........] 0a e0 0d [1000....] e8 [........] [........] [1000....] e2 0f 00 [1001....] e8 [........] [........] [........] eb [0000....] [....0000] a0 e1 ");

//		auto [memory, memorySize] = readBinaryFile("/home/azq2/dev/sie/elfloader3/ff/EL71sw45.bin");

	//	Pattern::Memory memoryRegion = { 0xA0000000, memory, memorySize };
	//	Pattern::find(pattern, memoryRegion);

		printf("parsed pattern: '%s'\n", Pattern::stringify(pattern).c_str());

	/*
	while (true) {
		auto token = m_tok.next();

		printf("TOK_%d %.*s\n", token.type, token.end - token.start, &input[token.start]);

		if (token.type == Tokenizer::TOK_INVALID || token.type == Tokenizer::TOK_EOF)
			break;
	}
	*/

	return 0;
}
