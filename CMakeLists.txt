cmake_minimum_required(VERSION 3.20)

project(uap-cpp VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_SHARED "Build shared library" ON)
option(BUILD_STATIC "Build static library" ON)
option(BUILD_BENCHMARKS "Build benchmark executable" OFF)
option(BUILD_TESTS "Build GoogleTest unit-tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

find_package(yaml-cpp REQUIRED)
include(FindPkgConfig)
pkg_check_modules(re2 REQUIRED IMPORTED_TARGET re2)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
endif()

set(UAP_PUBLIC_HEADERS UaParser)
file(GLOB INTERNAL_SRCS CONFIGURE_DEPENDS "internal/*.cpp" "internal/*.h")
set(UAP_SOURCES UaParser.cpp ${INTERNAL_SRCS})

add_library(uap_objects OBJECT ${UAP_SOURCES})
set_target_properties(uap_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(uap_objects
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/uap-cpp>
    PRIVATE internal)

target_link_libraries(uap_objects PUBLIC PkgConfig::re2 yaml-cpp)

set(UAP_BUILT_TARGETS)

if(BUILD_SHARED)
    add_library(uap-cpp-shared SHARED $<TARGET_OBJECTS:uap_objects>)
    set_target_properties(uap-cpp-shared PROPERTIES OUTPUT_NAME uaparser_cpp)
    target_link_libraries(uap-cpp-shared PUBLIC PkgConfig::re2 yaml-cpp)
    list(APPEND UAP_BUILT_TARGETS uap-cpp-shared)
endif()

if(BUILD_STATIC)
    add_library(uap-cpp-static STATIC $<TARGET_OBJECTS:uap_objects>)
    set_target_properties(uap-cpp-static PROPERTIES
        OUTPUT_NAME uaparser_cpp
        POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(uap-cpp-static PUBLIC PkgConfig::re2 yaml-cpp)
    list(APPEND UAP_BUILT_TARGETS uap-cpp-static)
endif()


if(BUILD_BENCHMARKS)
    add_executable(uap-bench benchmarks/UaParserBench.cpp)
    target_link_libraries(uap-bench PRIVATE uap-cpp-shared pthread)
endif()

if(BUILD_TESTS)
    add_executable(uap-cpp-tests UaParserTest.cpp)
    target_link_libraries(uap-cpp-tests PRIVATE uap-cpp-shared GTest::gtest_main pthread)
    add_test(NAME UaParserTests COMMAND uap-cpp-tests)
    install(TARGETS uap-cpp-tests RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(TARGETS ${UAP_BUILT_TARGETS}
    EXPORT  uap_cppTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uap-cpp
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/uap-cppConfigVersion.cmake
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/uap-cppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/uap-cppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uap-cpp
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/uap-cppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/uap-cppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uap-cpp
)

install(EXPORT uap_cppTargets
    FILE uap-cppTargets.cmake
    NAMESPACE uap-cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uap-cpp
)
