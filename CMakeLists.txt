cmake_minimum_required(VERSION 3.9)

project(ptr89)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS " -static -O3 ")
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include_directories("./lib" "./third_party/argparse/include" "./third_party/json/include")

add_executable(ptr89 src/main.cpp lib/src/Pattern.cpp lib/src/Tokenizer.cpp lib/src/Parser.cpp lib/src/utils.cpp)
install(TARGETS ptr89 DESTINATION /usr/bin)

# enable_testing()
# add_executable(ptr89-tests src/tests.cpp src/Pattern.cpp src/Tokenizer.cpp src/Parser.cpp src/utils.cpp)
# add_test(NAME test COMMAND ptr89-tests)
