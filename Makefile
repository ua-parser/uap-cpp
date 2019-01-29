LDFLAGS += -lboost_regex -lyaml-cpp
CXXFLAGS += -std=c++0x -Wall -Werror -g -fPIC -O3

# wildcard object build target
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $*.cpp -o $*.o
	@$(CXX) -MM $(CXXFLAGS) $*.cpp > $*.d

uaparser_cpp: libuaparser_cpp.a

libuaparser_cpp.a: UaParser.o
	ar rcs $@ $^

libuaparser_cpp.so: UaParser.o
	$(CXX) $< -shared $(LDFLAGS) -o $@

UaParserTest: libuaparser_cpp.a UaParserTest.o
	$(CXX) $^ -o $@ libuaparser_cpp.a $(LDFLAGS) -lgtest -lpthread

test: UaParserTest
	./UaParserTest

# clean everything generated
clean:
	find . -name "*.o" -exec rm -rf {} \; # clean up object files
	find . -name "*.d" -exec rm -rf {} \; # clean up dependencies
	rm -f UaParserTest *.a *.so

# automatically include the generated *.d dependency make targets
# that are created from the wildcard %.o build target above
-include $(OBJS:.o=.d)
