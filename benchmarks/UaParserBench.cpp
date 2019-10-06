#include "../UaParser.h"

#include <fstream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("Usage: %s <regexes.yaml> <input file> <times to repeat>\n", argv[0]);
    return -1;
  }

  std::vector<std::string> input;
  {
    std::ifstream infile(argv[2]);
    std::string line;
    while (std::getline(infile, line)) {
      input.push_back(line);
    }
  }

  uap_cpp::UserAgentParser p(argv[1]);

  int n = atoi(argv[3]);
  for (int i = 0; i < n; i++) {
    for (const auto& user_agent_string : input) {
      p.parse(user_agent_string);
    }
  }

  return 0;
}
