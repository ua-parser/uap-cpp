#pragma once

#include <re2/re2.h>
#include <memory>
#include <string>

namespace uap_cpp {

class Match;

/**
 * Wrapper around a re2 regular expression
 */
class Pattern {
 public:
  Pattern();
  Pattern(const std::string&, bool case_sensitive = true);

  void assign(const std::string&, bool case_sensitive = true);

  bool match(const std::string&, Match&) const;

 private:
  std::unique_ptr<re2::RE2> regex_;
  size_t groupCount_;
};

/**
 * Intended to be used with thread_local to avoid initialization cost
 */
class Match {
 public:
  Match();

  size_t size() const;
  const std::string& get(size_t index) const;

 private:
  friend class Pattern;
  enum { MAX_MATCHES = 10 };
  std::string strings_[MAX_MATCHES];
  re2::RE2::Arg args_[MAX_MATCHES];
  const re2::RE2::Arg* argPtrs_[MAX_MATCHES];
  size_t count_;
};

}  // namespace uap_cpp
