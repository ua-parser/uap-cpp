#pragma once

#include "StringView.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace uap_cpp {

/**
 * Indexes mandatory snippets in regular expressions.
 *
 * For example, in "(a)?(bc)+.* /", "bc" and " /" need to be present in the
 * input string for the expression to match ("a" is optional, however). This
 * class handles indexing snippets in expressions, and quickly returning which
 * snippets are present in an input string.
 */
class SnippetIndex {
 public:
  typedef uint32_t SnippetId;
  typedef std::set<SnippetId> SnippetSet;

  SnippetSet registerSnippets(const StringView& expression);
  SnippetSet getSnippets(const StringView& text) const;

  std::map<SnippetId, std::string> getRegisteredSnippets() const;

 private:
  struct TrieNode {
    ~TrieNode();
    TrieNode* transitions_[256]{nullptr};
    TrieNode* parent_{nullptr};
    SnippetId snippetId_{0};
  };
  TrieNode trieRootNode_;
  SnippetId maxSnippetId_{0};

  void registerSnippet(const char* start,
                       const char* end,
                       TrieNode*,
                       SnippetSet&);
};

}  // namespace uap_cpp
