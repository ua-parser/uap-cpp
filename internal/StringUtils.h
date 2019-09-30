#include "StringView.h"

namespace {

const char* getClosingParenthesis(const uap_cpp::StringView& view,
                                  bool* hadAlternativeOperators = nullptr) {
  if (hadAlternativeOperators) {
    *hadAlternativeOperators = false;
  }

  const char* s = view.start();

  char startChar = *s;
  char endChar;

  bool mayBeNested;
  int level = 0;
  switch (startChar) {
  case '(':
    endChar = ')';
    mayBeNested = true;
    break;
  case '[':
    endChar = ']';
    mayBeNested = false;
    break;
  case '{':
    endChar = '}';
    mayBeNested = false;
    break;
  default:
    return nullptr;
  }
  ++s;

  if (mayBeNested) {
    ++level;
  }

  while (!view.isEnd(s)) {
    if (*s == '\\') {
      ++s;
    } else if (*s == endChar) {
      if (mayBeNested) {
        --level;
      }
      if (level == 0) {
        if (startChar == '[' && hadAlternativeOperators) {
          *hadAlternativeOperators = true;
        }
        if (!(startChar == '[' && s - view.start() == 1)) {
          return s;
        }
      }
    } else if (*s == startChar) {
      if (mayBeNested) {
        ++level;
      }
    } else if (startChar == '(' && *s == '|' && level == 1) {
      if (hadAlternativeOperators) {
        *hadAlternativeOperators = true;
      }
    }
    ++s;
  }
  return nullptr;
}

inline bool isOptionalOperator(const uap_cpp::StringView& view) {
  const char* s = view.start();
  if (view.isEnd(s)) {
    return false;
  }
  if (*s == '{') {
    return !view.isEnd(s + 1) && (s[1] == '0' || s[1] == ',');
  }
  return *s == '*' || *s == '?';
}

}
