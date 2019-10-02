#include "StringView.h"

namespace {

const char* get_closing_parenthesis(const uap_cpp::StringView& view,
                                    bool* had_alternative_operators = nullptr) {
  if (had_alternative_operators) {
    *had_alternative_operators = false;
  }

  const char* s = view.start();

  char start_char = *s;
  char end_char;

  bool may_be_nested;
  int level = 0;
  switch (start_char) {
  case '(':
    end_char = ')';
    may_be_nested = true;
    break;
  case '[':
    end_char = ']';
    may_be_nested = false;
    break;
  case '{':
    end_char = '}';
    may_be_nested = false;
    break;
  default:
    return nullptr;
  }
  ++s;

  if (may_be_nested) {
    ++level;
  }

  while (!view.isEnd(s)) {
    if (*s == '\\') {
      ++s;
    } else if (*s == end_char) {
      if (may_be_nested) {
        --level;
      }
      if (level == 0) {
        if (start_char == '[' && had_alternative_operators) {
          *had_alternative_operators = true;
        }
        if (!(start_char == '[' && s - view.start() == 1)) {
          return s;
        }
      }
    } else if (*s == start_char) {
      if (may_be_nested) {
        ++level;
      }
    } else if (start_char == '(' && *s == '|' && level == 1) {
      if (had_alternative_operators) {
        *had_alternative_operators = true;
      }
    }
    ++s;
  }
  return nullptr;
}

inline bool is_optional_operator(const uap_cpp::StringView& view) {
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
