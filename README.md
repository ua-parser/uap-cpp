ua_parser C++ Library
=====================

Usage
-----

To build the (static) library:

    $ make uaparser_cpp

To build and run the tests:

    $ make test

A recent (GCC >= 4.8 or Clang >= 3.9 both work) C++11 compiler is required.

Dependencies
------------

* boost_regex, yaml-cpp (0.5 API)
* gtest (for testing)
* uap-core from https://github.com/ua-parser/uap-core, same directory level as uap-cpp. You can clone this repo with --recurse-submodules to get it.

Contributing
------------

Pull requests are welcome. Use `clang-format -i *.cpp *.h` to format the sources before sending the patch.

Author:
-------

  * Alex Şuhan <alex.suhan@gmail.com>

  Based on the D implementation by Shripad K and using agent data from BrowserScope.
