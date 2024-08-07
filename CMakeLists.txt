cmake_minimum_required(VERSION 3.9)

project(ptr89)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if (BUILD_WASM)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include_directories("./lib" "./third_party/argparse/include" "./third_party/json/include")

set(LIB_SRC lib/src/Pattern.cpp lib/src/Tokenizer.cpp lib/src/Parser.cpp lib/src/utils.cpp)

if (BUILD_WASM)
	# NodeJS
	add_executable(ptr89-node src/main.cpp ${LIB_SRC})
	target_precompile_headers(ptr89-node PRIVATE <argparse/argparse.hpp> <nlohmann/json.hpp> <string> <vector> <memory> <regex> <map> <tuple> <stdexcept>)
	target_link_options(ptr89-node PUBLIC
		-sMODULARIZE
		-sINVOKE_RUN=0
		-sNODERAWFS
		-sALLOW_MEMORY_GROWTH
		-sEXPORTED_RUNTIME_METHODS=['ccall','callMain']
		-fwasm-exceptions
	)
	target_compile_options(ptr89-node PUBLIC -O3 -fwasm-exceptions)
else()
	add_executable(ptr89 src/main.cpp ${LIB_SRC})
	target_precompile_headers(ptr89 PRIVATE <argparse/argparse.hpp> <nlohmann/json.hpp> <string> <vector> <memory> <regex> <map> <tuple> <stdexcept>)
#	target_link_options(ptr89 PUBLIC -static)
	target_compile_options(ptr89 PUBLIC -O3)
	install(TARGETS ptr89 DESTINATION /usr/bin)

	if (BUILD_TESTS)
		enable_testing()
		add_executable(ptr89-tests src/tests.cpp ${LIB_SRC})
		add_test(NAME test COMMAND ptr89-tests)
	endif()
endif()
