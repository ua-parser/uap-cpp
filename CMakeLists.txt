cmake_minimum_required(VERSION 3.9)

project(uap-cpp)

IF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Werror -fPIC")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")



set(LIB_SOURCES
        UaParser.cpp
        internal/AlternativeExpander.cpp
        internal/Pattern.cpp
        internal/ReplaceTemplate.cpp
        internal/SnippetIndex.cpp)

find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIR})
#link_directories(${YAML_CPP_LIBRARY})

find_package(re2)

# this is the "object library" target: compiles the sources only once
add_library(objlib OBJECT ${LIB_SOURCES})

# shared libraries need PIC
set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

# shared and static libraries built from the same object files
add_library(uap-cpp-static STATIC $<TARGET_OBJECTS:objlib>)
set_target_properties(uap-cpp-static PROPERTIES OUTPUT_NAME uaparser_cpp)
target_link_libraries(uap-cpp-static)

add_library(uap-cpp-shared SHARED $<TARGET_OBJECTS:objlib>)
set_target_properties(uap-cpp-shared PROPERTIES OUTPUT_NAME uaparser_cpp)
target_link_libraries(uap-cpp-shared)



set(TEST_SOURCES
        UaParserTest.cpp)

find_package(GTest)

add_executable(tests ${TEST_SOURCES} $<TARGET_OBJECTS:objlib>)
set_target_properties(tests PROPERTIES OUTPUT_NAME UaParserTest)

target_link_libraries(tests re2 yaml-cpp gtest pthread)



set(BENCH_SOURCES
        benchmarks/UaParserBench.cpp)

add_executable(bench ${BENCH_SOURCES} $<TARGET_OBJECTS:objlib>)
set_target_properties(bench PROPERTIES OUTPUT_NAME UaParserBench)

target_link_libraries(bench re2 yaml-cpp pthread)
