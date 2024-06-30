cmake_minimum_required(VERSION 3.9)

project(ptr89)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS " -static -O3 ")
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include_directories("./lib" "./third_party/argparse/include" "./third_party/json/include")

set(LIB_SRC lib/src/Pattern.cpp lib/src/Tokenizer.cpp lib/src/Parser.cpp lib/src/utils.cpp)

add_executable(ptr89 src/main.cpp ${LIB_SRC})
target_precompile_headers(ptr89 PRIVATE <argparse/argparse.hpp> <nlohmann/json.hpp> <string> <vector> <memory> <regex> <map> <tuple> <stdexcept>)
install(TARGETS ptr89 DESTINATION /usr/bin)

enable_testing()
add_executable(ptr89-tests src/tests.cpp ${LIB_SRC})
add_test(NAME test COMMAND ptr89-tests)
