#include "SnippetIndex.h"

#include "StringUtils.h"

namespace uap_cpp {

namespace {

inline bool is_snippet_char(char c, bool prev_was_backslash) {
  if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
      ('0' <= c && c <= '9')) {
    // Letters and digits are snippet characters, unless preceded by backslash
    return !prev_was_backslash;
  }
  switch (c) {
    case ' ':
    case '_':
    case '-':
    case '/':
    case ',':
    case ';':
    case '=':
    case '%':
      // These are always snippet characters
      return true;
    default:
      // Other characters are snippet characters if preceded by backslash
      return prev_was_backslash;
  }
}

inline bool possibly_skip_block(const char*& s, const StringView& view) {
  if (*s == '(' || *s == '[' || *s == '{') {
    bool had_alternative_operators = false;
    const char* closing_parenthesis =
        get_closing_parenthesis(view.from(s), &had_alternative_operators);
    if (closing_parenthesis) {
      if (*s == '{' || had_alternative_operators ||
          (*s == '(' &&
           is_optional_operator(view.from(closing_parenthesis + 1)))) {
        // Always skip count blocks  {1,2}
        // Skip block with alteratives  (ab|ba) or [ab]
        // Skip optional block  (a)? (b)* (a){0,}
        s = closing_parenthesis;
        return true;
      }
    }
  }
  return false;
}

bool has_root_level_alternatives(const StringView& view) {
  const char* s = view.start();

  bool prev_was_backslash = false;
  int level = 0;
  while (!view.isEnd(s)) {
    if (!prev_was_backslash) {
      if (*s == '(') {
        ++level;
      } else if (*s == ')') {
        --level;
      } else if (*s == '|' && level == 0) {
        return true;
      } else if (*s == '[') {
        // Must ignore character-level alternatives  [a(b|c]
        const char* closing_parenthesis = get_closing_parenthesis(view.from(s));
        if (closing_parenthesis) {
          s = closing_parenthesis;
        }
      }
    }
    prev_was_backslash = *s == '\\' && !prev_was_backslash;
    ++s;
  }
  return false;
}

inline uint8_t to_byte(char c, bool to_lowercase = true) {
  uint8_t b = c;
  // TODO: Duplicate nodes instead of having case-insensitive expressions?
  if (to_lowercase && ('A' <= c && c <= 'Z')) {
    b |= 0x20;
  }
  return b;
}

}  // namespace

SnippetIndex::SnippetSet SnippetIndex::registerSnippets(
    const StringView& expression) {
  SnippetSet out;

  if (has_root_level_alternatives(expression)) {
    // Skip whole expression if alternatives on root level  a|b
    return out;
  }

  const char* s = expression.start();
  const char* snippet_start = nullptr;
  TrieNode* node = nullptr;

  bool prev_was_backslash = false;
  while (!expression.isEnd(s)) {
    if (is_snippet_char(*s, prev_was_backslash)) {
      if (!node) {
        snippet_start = s;
        node = &trieRootNode_;
      }

      TrieNode*& next_node = node->transitions_[to_byte(*s)];
      if (!next_node) {
        next_node = new TrieNode;
        next_node->parent_ = node;
      }
      node = next_node;
    } else {
      if (node) {
        const char* snippet_end = s;
        if (is_optional_operator(expression.from(snippet_end))) {
          // Do not include optional characters  a? a*
          --snippet_end;
          node = node->parent_;
        }

        registerSnippet(snippet_start, snippet_end, node, out);

        snippet_start = nullptr;
        node = nullptr;
      }
    }

    if (!prev_was_backslash) {
      possibly_skip_block(s, expression);
    }

    prev_was_backslash = *s == '\\' && !prev_was_backslash;
    ++s;
  }

  if (node) {
    registerSnippet(snippet_start, s, node, out);
  }

  return out;
}

void SnippetIndex::registerSnippet(const char* start,
                                   const char* end,
                                   TrieNode* node,
                                   SnippetIndex::SnippetSet& out) {
  if (node && end - start > 2) {
    if (!node->snippetId_) {
      node->snippetId_ = ++maxSnippetId_;
    }
    out.insert(node->snippetId_);
  }
}

SnippetIndex::TrieNode::~TrieNode() {
  for (auto* node : transitions_) {
    delete node;
  }
}

SnippetIndex::SnippetSet SnippetIndex::getSnippets(
    const StringView& text) const {
  SnippetSet out;

  const char* s = text.start();
  while (!text.isEnd(s)) {
    const char* s2 = s;
    const TrieNode* node = &trieRootNode_;
    while (node && !text.isEnd(s2)) {
      // Every character can be the start of a snippet (actually, only snippet
      // characters, but unconditionally looking it up in the array is faster)
      node = node->transitions_[to_byte(*s2)];
      if (node && node->snippetId_) {
        out.insert(node->snippetId_);
      }
      ++s2;
    }
    ++s;
  }

  return out;
}

namespace {
template <class TrieNode, class Map>
void build_map(const TrieNode& node, const std::string& base_string, Map& map) {
  if (node.snippetId_) {
    map.insert(std::make_pair(node.snippetId_, base_string));
  }

  for (int i = 0; i < 256; i++) {
    auto* next_node = node.transitions_[i];
    if (next_node) {
      std::string next_string(base_string);
      next_string += (char)i;
      build_map(*next_node, next_string, map);
    }
  }
}
}  // namespace

std::map<SnippetIndex::SnippetId, std::string>
SnippetIndex::getRegisteredSnippets() const {
  std::map<SnippetId, std::string> map;
  build_map(trieRootNode_, "", map);
  return map;
}

}  // namespace uap_cpp
