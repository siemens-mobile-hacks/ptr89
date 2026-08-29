#include <emscripten/bind.h>

#include "Ptr89Wasm.h"

using namespace emscripten;
using namespace Ptr89;

EMSCRIPTEN_BINDINGS(ptr89) {
	value_object<Ptr89Search>("Ptr89Search")
		.field("pattern", &Ptr89Search::pattern)
		.field("type", &Ptr89Search::type)
		.field("results", &Ptr89Search::results);

	value_object<Ptr89SearchResult>("Ptr89SearchResult")
		.field("address", &Ptr89SearchResult::address)
		.field("offset", &Ptr89SearchResult::offset)
		.field("bytes", &Ptr89SearchResult::bytes);

	value_object<Ptr89XRef>("Ptr89XRef")
		.field("type", &Ptr89XRef::type)
		.field("xref", &Ptr89XRef::xref)
		.field("offset", &Ptr89XRef::offset)
		.field("bytes", &Ptr89XRef::bytes);

	register_vector<Ptr89SearchResult>("Ptr89SearchResults");
	register_vector<Ptr89XRef>("Ptr89XRefs");

	class_<Ptr89Wasm>("Ptr89")
		.constructor<>()
		.function("open", &Ptr89Wasm::open)
		.function("close", &Ptr89Wasm::close)
		.function("setDebug", &Ptr89Wasm::setDebug)
		.function("find", &Ptr89Wasm::find)
		.function("findXRefs", &Ptr89Wasm::findXRefs);

	function("prettify", &prettify);
}
