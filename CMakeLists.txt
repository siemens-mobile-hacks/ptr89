cmake_minimum_required(VERSION 3.10)

project(ptr89)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include_directories("./lib" "./third_party/argparse/include" "./third_party/json/include")

set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
	set(SPDLOG_NO_ATOMIC_LEVELS ON CACHE BOOL "" FORCE)
	set(SPDLOG_NO_THREAD_ID ON CACHE BOOL "" FORCE)
	set(SPDLOG_NO_TLS ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

set(LIB_SRC lib/src/Arch.cpp lib/src/Pattern.cpp lib/src/Tokenizer.cpp lib/src/Parser.cpp lib/src/utils.cpp)

add_library(ptr89-lib STATIC ${LIB_SRC})
set_target_properties(ptr89-lib PROPERTIES OUTPUT_NAME ptr89)
target_link_libraries(ptr89-lib PUBLIC spdlog::spdlog)

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
	add_executable(ptr89_wasm wasm/main.cpp wasm/Ptr89Wasm.cpp)
	set_target_properties(ptr89-lib spdlog PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION TRUE
	)
	set_target_properties(ptr89_wasm PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION TRUE
		SUFFIX ".js"
	)
	target_link_libraries(ptr89_wasm PRIVATE ptr89-lib)
	target_compile_options(ptr89-lib PRIVATE -Oz -fwasm-exceptions)
	target_compile_options(ptr89_wasm PRIVATE -Oz -fwasm-exceptions)
	target_compile_options(spdlog PRIVATE -Oz -fwasm-exceptions)
	target_link_options(ptr89_wasm PRIVATE
		-sWASM=1
		-sSTRICT=1
		-sEXPORT_ES6=1
		-sMODULARIZE
		-sINVOKE_RUN=0
		-sSINGLE_FILE=0
		-sASSERTIONS=0
		-sEXIT_RUNTIME=1
		-sFILESYSTEM=0
		-sDYNAMIC_EXECUTION=0
		-sSTACK_OVERFLOW_CHECK=0
		-sALLOW_MEMORY_GROWTH
		-sENVIRONMENT=node,web,worker
		-sEXPORTED_FUNCTIONS=['_malloc','_free']
		-sEXPORTED_RUNTIME_METHODS=['HEAPU8','getExceptionMessage','decrementExceptionRefcount']
		-sMALLOC=emmalloc
		-fwasm-exceptions
		--bind
		--emit-tsd "$<TARGET_FILE_DIR:ptr89_wasm>/ptr89_wasm.d.ts"
	)

	if (BUILD_TESTS)
		enable_testing()
		find_program(PNPM_EXECUTABLE pnpm REQUIRED)
		add_test(NAME wasm-test COMMAND ${PNPM_EXECUTABLE} test)
		set_tests_properties(wasm-test PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
	endif()
else()
	add_executable(ptr89 src/main.cpp)
	target_link_libraries(ptr89 PRIVATE ptr89-lib)
	target_precompile_headers(ptr89 PRIVATE <argparse/argparse.hpp> <nlohmann/json.hpp> <string> <vector> <memory> <regex> <map> <tuple> <stdexcept>)
	install(TARGETS ptr89)

	if (MSVC)
		add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	else()
		if (BUILD_STATIC)
			target_link_options(ptr89 PUBLIC -static -static-libgcc -static-libstdc++)
		endif()

		if (CMAKE_BUILD_TYPE MATCHES "Release")
			target_link_options(ptr89 PUBLIC -s)
		endif()

		target_compile_options(ptr89 PRIVATE -Wall -Wextra -Werror -O3)
		target_compile_options(ptr89-lib PRIVATE -Wall -Wextra -Werror -O3)
	endif()

	if (BUILD_TESTS)
		enable_testing()
		add_executable(ptr89-tests src/tests.cpp)
		target_link_libraries(ptr89-tests PRIVATE ptr89-lib)
		add_test(NAME test COMMAND ptr89-tests)
	endif()
endif()
