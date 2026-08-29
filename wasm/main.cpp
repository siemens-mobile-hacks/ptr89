#include <emscripten/bind.h>

#include "Ptr89Wasm.h"

using namespace emscripten;
using namespace Ptr89;

EMSCRIPTEN_BINDINGS(ptr89) {
	value_object<WasmPatternSearchResult>("PatternSearchResult")
		.field("type", &WasmPatternSearchResult::type)
		.field("address", &WasmPatternSearchResult::address)
		.field("offset", &WasmPatternSearchResult::offset)
		.field("value", &WasmPatternSearchResult::value);

	value_object<WasmXRefSearchResult>("XRefSearchResult")
		.field("type", &WasmXRefSearchResult::type)
		.field("address", &WasmXRefSearchResult::address)
		.field("offset", &WasmXRefSearchResult::offset);

	register_vector<WasmPatternSearchResult>("PatternSearchResults");
	register_vector<WasmXRefSearchResult>("XRefSearchResults");

	class_<Ptr89Wasm>("Ptr89")
		.constructor<>()
		.function("open", &Ptr89Wasm::open)
		.function("close", &Ptr89Wasm::close)
		.function("setDebug", &Ptr89Wasm::setDebug)
		.function("find", &Ptr89Wasm::find)
		.function("findXRefs", &Ptr89Wasm::findXRefs);

	function("prettify", &prettify);
}
