ua_parser C++ Library
=====================

Usage
-----

### Linux

To build the (static) library:

    make uaparser_cpp

To build and run the tests:

    make test

A recent (GCC >= 4.8 or Clang >= 3.9 both work) C++11 compiler is required.

#### CMake build

##### libraries

    cd uap-cpp
    mkdir build
    cd build
    cmake ..
    
    make uap-cpp-static
    
    make uap-cpp-shared

##### tests
Prepare gtest:

    apt-get install libgtest-dev
    apt-get install cmake
    cd /usr/src/gtest
    cmake CMakeLists.txt
    make
    cp *.a /usr/lib

Run from build directory:

    make tests

Run from uap-cpp directory:

    ./build/UaParserTest

##### benchmark
Run from build directory:

    make bench

Run from uap-cpp directory:

    time ./build/UaParserBench uap-core/regexes.yaml benchmarks/useragents.txt 1000

### Windows

First, open ``uap-cpp.sln`` with MSVC 15 (Visual Studio 2017).

To build the (static) library:

    build the "UaParser" project

To build and run the tests:

    build the "UaParserTest" project

The MSVC projects assume boost to be installed at: ``C:\boost_1_69_0`` and yaml to be installed at: ``C:\yaml-cpp``. Change these paths if needed. ``boost_regex`` needs to be built from source in advance. For yaml-cpp, you can built it from source, or use a prebuilt [here](https://github.com/hsluoyz/yaml-cpp-prebuilt-win32).

Dependencies
------------

* re2, yaml-cpp (0.5 API)
* gtest (for testing)
* [uap-core](https://github.com/ua-parser/uap-core), same directory level as uap-cpp. You can clone this repo with --recurse-submodules to get it. Alternatively, run `git submodule update --init`.

Contributing
------------

Pull requests are welcome. Use `clang-format -i *.cpp *.h` to format the sources before sending the patch.

Credits
-------

Inspired from the D implementation by Shripad K and using agent data from BrowserScope.
