#pragma once

namespace uap_cpp {

class StringView {
public:
  template <class String>
  StringView(const String& s) : start_(s.data()), end_(start_ + s.size()) {
  }

  StringView(const char* start) : start_(start), end_(nullptr) {
  }

  StringView(const char* start, const char* end) : start_(start), end_(end) {
  }

  const char* start() const {
    return start_;
  }

  bool isEnd(const char* s) const {
    return (end_ && s >= end_) || (!end_ && *s == 0);
  }

  StringView from(const char* start) const {
    return StringView(start, end_);
  }

  StringView to(const char* end) const {
    return StringView(start_, end);
  }

private:
  const char* start_;
  const char* end_;
};

}
