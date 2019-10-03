#include "StringView.h"

namespace {

inline char get_corresponding_end_char(char start_char) {
  switch (start_char) {
    case '(':
      return ')';
    case '[':
      return ']';
    case '{':
      return '}';
    default:
      return '\0';
  }
}

inline const char* get_closing_parenthesis(
    const uap_cpp::StringView& view,
    bool* had_alternative_operators = nullptr) {
  if (had_alternative_operators) {
    *had_alternative_operators = false;
  }

  const char* s = view.start();

  char start_char = *s;
  char end_char = get_corresponding_end_char(start_char);
  if (!end_char) {
    return nullptr;
  }
  ++s;

  int level = 0;
  bool may_be_nested = start_char == '(';
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

inline bool is_whitespace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

template <class String>
void trim(String& s) {
  size_t trim_left = 0;
  for (auto it = s.begin(); it != s.end(); ++it) {
    if (!is_whitespace(*it)) {
      break;
    }
    ++trim_left;
  }

  if (trim_left == s.size()) {
    s.clear();
  } else {
    size_t trim_right = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
      if (!is_whitespace(*it)) {
        break;
      }
      ++trim_right;
    }

    if (trim_left > 0 || trim_right > 0) {
      if (trim_left == 0) {
        s.resize(s.size() - trim_right);
      } else {
        String copy(s.c_str() + trim_left, s.size() - trim_left - trim_right);
        s.swap(copy);
      }
    }
  }
}

}  // namespace
