#pragma once

#include "StringView.h"

#include <string>
#include <vector>

namespace uap_cpp {

/**
 * Regular expression preprocessor that expands alternatives into their
 * possible combinations.
 *
 * For example, the expression "(Something|Other)/\d+" will be expanded to the
 * the expressions "(Something)/\d+" and "(Other)/\d+".
 *
 * This allows for a better mapping of mandatory snippets without complicating
 * the actual indexing implementation.
 */
class AlternativeExpander {
public:
  static std::vector<std::string> expand(const StringView& expression);
};

}
