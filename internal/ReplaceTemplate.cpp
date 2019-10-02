#include "ReplaceTemplate.h"

#include "Pattern.h"

namespace uap_cpp {

ReplaceTemplate::ReplaceTemplate() : approximateSize_(0) {}

ReplaceTemplate::ReplaceTemplate(const std::string& replace_template) {
  const char* start = replace_template.c_str();
  const char* chunk_start = start;
  const char* s = start;
  while (*s) {
    if (s[0] == '$' && '0' <= s[1] && s[1] <= '9') {
      chunks_.push_back(
          std::string(chunk_start, static_cast<size_t>(s - chunk_start)));
      matchIndices_.push_back(static_cast<int>(s[1] - '0'));

      chunk_start = s + 2;
      s = chunk_start;
    } else {
      ++s;
    }
  }
  chunks_.push_back(
      std::string(chunk_start, static_cast<size_t>(s - chunk_start)));

  approximateSize_ = replace_template.size() + 15 * (chunks_.size() - 1);
}

bool ReplaceTemplate::empty() const {
  return chunks_.empty();
}

std::string ReplaceTemplate::expand(const Match& m) const {
  if (chunks_.size() == 1) {
    return chunks_[0];
  }

  std::string s;
  if (approximateSize_ > 0) {
    s.reserve(approximateSize_);
  }

  size_t index = 0;
  for (const auto& chunk : chunks_) {
    if (index > 0) {
      s += m.get(matchIndices_[index - 1]);
    }
    if (!chunk.empty()) {
      s += chunk;
    }
    ++index;
  }

  return s;
}

}  // namespace uap_cpp
