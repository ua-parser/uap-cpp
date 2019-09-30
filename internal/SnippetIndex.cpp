#include "SnippetIndex.h"

#include "StringUtils.h"

namespace uap_cpp {

namespace {

inline bool isSnippetChar(char c, bool prevWasBackslash) {
  if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
      ('0' <= c && c <= '9')) {
    // Letters and digits are snippet characters, unless preceded by backslash
    return !prevWasBackslash;
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
      return prevWasBackslash;
  }
}

inline bool possiblySkipBlock(const char*& s, const StringView& view) {
  if (*s == '(' || *s == '[' || *s == '{') {
    bool hadAlternativeOperators = false;
    const char* closingParenthesis =
        getClosingParenthesis(view.from(s), &hadAlternativeOperators);
    if (closingParenthesis) {
      if (*s == '{' || hadAlternativeOperators ||
          (*s == '(' &&
           isOptionalOperator(view.from(closingParenthesis + 1)))) {
        // Always skip count blocks  {1,2}
        // Skip block with alteratives  (ab|ba) or [ab]
        // Skip optional block  (a)? (b)* (a){0,}
        s = closingParenthesis;
        return true;
      }
    }
  }
  return false;
}

bool hasRootLevelAlternatives(const StringView& view) {
  const char* s = view.start();

  bool prevWasBackslash = false;
  int level = 0;
  while (!view.isEnd(s)) {
    if (!prevWasBackslash) {
      if (*s == '(') {
        ++level;
      } else if (*s == ')') {
        --level;
      } else if (*s == '|' && level == 0) {
        return true;
      } else if (*s == '[') {
        // Must ignore character-level alternatives  [a(b|c]
        const char* closingParenthesis = getClosingParenthesis(view.from(s));
        if (closingParenthesis) {
          s = closingParenthesis;
        }
      }
    }
    prevWasBackslash = *s == '\\' && !prevWasBackslash;
    ++s;
  }
  return false;
}

inline uint8_t toByte(char c, bool toLowercase = true) {
  uint8_t b = c;
  // TODO: Duplicate nodes instead of having case-insensitive expressions?
  if (toLowercase && ('A' <= c && c <= 'Z')) {
    b |= 0x20;
  }
  return b;
}

}  // namespace

SnippetIndex::SnippetSet SnippetIndex::registerSnippets(
    const StringView& expression) {
  SnippetSet out;

  if (hasRootLevelAlternatives(expression)) {
    // Skip whole expression if alternatives on root level  a|b
    return out;
  }

  const char* s = expression.start();
  const char* snippetStart = nullptr;
  TrieNode* node = nullptr;

  bool prevWasBackslash = false;
  while (!expression.isEnd(s)) {
    if (isSnippetChar(*s, prevWasBackslash)) {
      if (!node) {
        snippetStart = s;
        node = &trieRootNode_;
      }

      TrieNode*& nextNode = node->transitions_[toByte(*s)];
      if (!nextNode) {
        nextNode = new TrieNode;
        nextNode->parent_ = node;
      }
      node = nextNode;
    } else {
      if (node) {
        const char* snippetEnd = s;
        if (isOptionalOperator(expression.from(snippetEnd))) {
          // Do not include optional characters  a? a*
          --snippetEnd;
          node = node->parent_;
        }

        registerSnippet(snippetStart, snippetEnd, node, out);

        snippetStart = nullptr;
        node = nullptr;
      }
    }

    if (!prevWasBackslash) {
      possiblySkipBlock(s, expression);
    }

    prevWasBackslash = *s == '\\' && !prevWasBackslash;
    ++s;
  }

  if (node) {
    registerSnippet(snippetStart, s, node, out);
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
      node = node->transitions_[toByte(*s2)];
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
void buildMap(const TrieNode& node, const std::string& baseString, Map& map) {
  if (node.snippetId_) {
    map.insert(std::make_pair(node.snippetId_, baseString));
  }

  for (int i = 0; i < 256; i++) {
    auto* nextNode = node.transitions_[i];
    if (nextNode) {
      std::string nextString(baseString);
      nextString += (char)i;
      buildMap(*nextNode, nextString, map);
    }
  }
}
}  // namespace

std::map<SnippetIndex::SnippetId, std::string>
SnippetIndex::getRegisteredSnippets() const {
  std::map<SnippetId, std::string> map;
  buildMap(trieRootNode_, "", map);
  return map;
}

}  // namespace uap_cpp
