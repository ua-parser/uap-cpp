#include "AlternativeExpander.h"

#include "StringUtils.h"

namespace uap_cpp {

std::vector<std::string> AlternativeExpander::expand(
    const StringView& expression) {
  std::vector<std::string> out;
  std::string prefix;
  Stack next;
  expand(expression, std::move(prefix), std::move(next), out);
  return out;
}

namespace {

bool isNegativeLookaheadOperator(const StringView& view) {
  const char* s = view.start();
  if (view.isEnd(s) || view.isEnd(s + 1)) {
    return false;
  }
  return s[0] == '?' && s[1] == '!';
}

bool isNonCaptureOperator(const StringView& view) {
  const char* s = view.start();
  if (view.isEnd(s) || view.isEnd(s + 1)) {
    return false;
  }
  return s[0] == '?' && s[1] == ':';
}

}  // namespace

void AlternativeExpander::expand(const StringView& view,
                                 std::string prefix,
                                 Stack next,
                                 std::vector<std::string>& out) {
  // First go through root-level alternatives
  {
    const char* s = view.start();
    int level = 0;
    bool prevWasBackslash = false;
    while (!view.isEnd(s)) {
      if (!prevWasBackslash) {
        if (*s == '(') {
          ++level;
        } else if (*s == ')') {
          if (level > 0) {
            --level;
          }
        } else if (*s == '[') {
          const char* closingParenthesis = getClosingParenthesis(view.from(s));
          if (closingParenthesis) {
            // Skip character-level alternative block
            s = closingParenthesis;
            continue;
          }
        }

        if (level == 0 && *s == '|') {
          // Go through alternative on the left
          expand(view.to(s), prefix, next, out);

          // Go through alternative(s) on the right
          expand(view.from(s + 1), std::move(prefix), std::move(next), out);

          return;
        }
      }

      prevWasBackslash = *s == '\\' && !prevWasBackslash;
      ++s;
    }
  }

  // Now go through root-level parentheses
  const char* s = view.start();
  int level = 0;
  bool prevWasBackslash = false;
  while (!view.isEnd(s)) {
    if (!prevWasBackslash) {
      if (*s == '(') {
        ++level;
        if (level == 1) {
          const char* closingParenthesis = getClosingParenthesis(view.from(s));
          if (!closingParenthesis) {
            // Bad expression
            --level;
            ++s;
            continue;
          }

          if (isOptionalOperator(view.from(closingParenthesis + 1)) ||
              isNegativeLookaheadOperator(view.from(s + 1))) {
            // Continue after parentheses
            s = closingParenthesis;
            continue;
          }

          // Add ( or (?: to prefix
          const char* innerStart = s + 1;
          if (isNonCaptureOperator(view.from(innerStart))) {
            innerStart += 2;
          }
          prefix.append(view.start(), innerStart - view.start());

          next.emplace_back(view.from(closingParenthesis));

          // Recursively go through what is enclosed by parentheses
          expand(StringView(innerStart, closingParenthesis),
                 std::move(prefix),
                 std::move(next),
                 out);
          return;
        }
      } else if (*s == ')') {
        if (level > 0) {
          --level;
        }
      } else if (*s == '[') {
        const char* closingParenthesis = getClosingParenthesis(view.from(s));
        if (closingParenthesis) {
          // Skip character-level alternative block
          s = closingParenthesis;
          continue;
        }
      }
    }

    prevWasBackslash = *s == '\\' && !prevWasBackslash;
    ++s;
  }

  // No alternatives or parentheses, so add to to prefix
  prefix.append(view.start(), s - view.start());

  if (next.empty()) {
    // Reached end of string, add what has been collected
    out.emplace_back(std::move(prefix));
  } else {
    // Pop the stack and continue with rest of string
    StringView nextView = next.back();
    next.pop_back();

    expand(nextView, std::move(prefix), std::move(next), out);
  }
}

}  // namespace uap_cpp
