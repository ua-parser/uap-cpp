#pragma once

#include <string>

namespace uap_cpp {

struct Generic {
  Generic() : family("Other") {}
  std::string family;
};

struct Device : Generic {
  std::string model;
  std::string brand;
};

struct Agent : Generic {
  std::string major;
  std::string minor;
  std::string patch;
  std::string patch_minor;

  std::string toString() const { return family + " " + toVersionString(); }

  std::string toVersionString() const {
    return (major.empty() ? "0" : major) + "." + (minor.empty() ? "0" : minor) + "." + (patch.empty() ? "0" : patch);
  }
};

struct UserAgent {
  Device device;

  Agent os;
  Agent browser;

  std::string toFullString() const { return browser.toString() + "/" + os.toString(); }

  bool isSpider() const { return device.family == "Spider"; }
};

enum DeviceType {
  kUnknown = 0, kDesktop, kMobile, kTablet
};

class UserAgentParser {
public:
  explicit UserAgentParser(const std::string& regexes_file_path);

  UserAgent parse(const std::string&) const;

  Device parse_device(const std::string&) const;
  Agent parse_os(const std::string&) const;
  Agent parse_browser(const std::string&) const;
  
  DeviceType device_type(const std::string&) const;

  ~UserAgentParser();

private:
  const std::string regexes_file_path_;
  const void* ua_store_;
};

}  // namespace uap_cpp
