#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace uap_cpp {

/**
 * Maps a set of snippets to a set of expressions.
 */
template <class Expression>
class SnippetMapping {
 public:
  typedef uint32_t SnippetId;

  /**
   * Add an expression to the mapping. The snippets all need to be present
   * for an expression to match. The snippets need to be ordered.
   */
  template <class SnippetSet>
  void addMapping(const SnippetSet& snippets, const Expression& expression) {
    TrieNode* node = &trieRootNode_;
    for (SnippetId snippet : snippets) {
      auto& slot = node->transitions_[snippet];
      if (!slot) {
        slot = std::make_unique<TrieNode>();
      }
      node = slot.get();
    }
    node->expressions_.insert(expression);
  }

  /**
   * Find expressions covered by the found set of snippets. The expressions
   * should not require a snippet that is not in the matched set of snippets.
   * The snippet set needs to be ordered the same way as when it was added.
   */
  template <class SnippetSet, class Result>
  void getExpressions(const SnippetSet& snippets, Result& expressions) const {
    auto end = snippets.end();
    getExpressionsRecursively(
        snippets.begin(), end, trieRootNode_, expressions);
  }

 private:
  struct TrieNode {
    std::unordered_map<SnippetId, std::unique_ptr<TrieNode>> transitions_;
    std::unordered_set<Expression> expressions_;
  };
  TrieNode trieRootNode_;

  template <class Iterator, class Result>
  void getExpressionsRecursively(Iterator it,
                                 const Iterator& end,
                                 const TrieNode& node,
                                 Result& expressions) const {
    if (!node.expressions_.empty()) {
      expressions.insert(node.expressions_.begin(), node.expressions_.end());
    }

    while (it != end) {
      auto findIt = node.transitions_.find(*it);
      ++it;

      if (findIt != node.transitions_.end()) {
        getExpressionsRecursively(it, end, *findIt->second, expressions);
      }
    }
  }
};

}  // namespace uap_cpp
