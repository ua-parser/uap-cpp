ifndef KEEP_ENV_VARS
LDFLAGS += -lre2 -lyaml-cpp
CXXFLAGS += -std=c++14 -Wall -Werror -g -fPIC -O3
endif

# wildcard object build target
%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $*.cpp -o $*.o
	@$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $*.cpp > $*.d

uaparser_cpp: libuaparser_cpp.a

OBJECT_FILES = UaParser.o \
	internal/Pattern.o \
	internal/AlternativeExpander.o \
	internal/SnippetIndex.o \
	internal/ReplaceTemplate.o

libuaparser_cpp.a: $(OBJECT_FILES)
	ar rcs $@ $^

libuaparser_cpp.so: $(OBJECT_FILES)
	$(CXX) $< -shared $(LDFLAGS) -o $@

UaParserTest: libuaparser_cpp.a UaParserTest.o
	$(CXX) $^ -o $@ libuaparser_cpp.a $(LDFLAGS) -lgtest -lpthread

test: UaParserTest libuaparser_cpp.a
	./UaParserTest

# clean everything generated
clean:
	find . -name "*.o" -exec rm -rf {} \; # clean up object files
	find . -name "*.d" -exec rm -rf {} \; # clean up dependencies
	rm -f UaParserTest *.a *.so

# automatically include the generated *.d dependency make targets
# that are created from the wildcard %.o build target above
-include $(OBJS:.o=.d)
