#pragma once

#include <string>
#include <vector>

namespace uap_cpp {

class Match;

/**
 * Takes a string with $1, $2, etc, to be replaced by matched blocks
 */
class ReplaceTemplate {
 public:
  ReplaceTemplate();
  ReplaceTemplate(const std::string&);

  bool empty() const;
  std::string expand(const Match&) const;

 private:
  std::vector<std::string> chunks_;
  std::vector<int> matchIndices_;
  size_t approximateSize_;
};

}  // namespace uap_cpp
