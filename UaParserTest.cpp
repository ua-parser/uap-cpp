#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>
#include "UaParser.h"
#ifdef WITH_MT_TEST
#include <future>
#endif  // WITH_MT_TEST

namespace {

const std::string UA_CORE_DIR = "./uap-core";

const uap_cpp::UserAgentParser g_ua_parser(UA_CORE_DIR + "/regexes.yaml");

TEST(UserAgentParser, basic) {
  const auto uagent = g_ua_parser.parse(
      "Mozilla/5.0 (iPhone; CPU iPhone OS 5_1_1 like Mac OS X) "
      "AppleWebKit/534.46 "
      "(KHTML, like Gecko) Version/5.1 Mobile/9B206 Safari/7534.48.3");
  ASSERT_EQ("Mobile Safari", uagent.browser.family);
  ASSERT_EQ("5", uagent.browser.major);
  ASSERT_EQ("1", uagent.browser.minor);
  ASSERT_EQ("", uagent.browser.patch);
  ASSERT_EQ("Mobile Safari 5.1.0", uagent.browser.toString());
  ASSERT_EQ("5.1.0", uagent.browser.toVersionString());

  ASSERT_EQ("iOS", uagent.os.family);
  ASSERT_EQ("5", uagent.os.major);
  ASSERT_EQ("1", uagent.os.minor);
  ASSERT_EQ("1", uagent.os.patch);
  ASSERT_EQ("iOS 5.1.1", uagent.os.toString());
  ASSERT_EQ("5.1.1", uagent.os.toVersionString());

  ASSERT_EQ("Mobile Safari 5.1.0/iOS 5.1.1", uagent.toFullString());

  ASSERT_EQ("iPhone", uagent.device.family);

  ASSERT_FALSE(uagent.isSpider());
}

TEST(UserAgentParser, DeviceTypeMobile) {
  ASSERT_TRUE(uap_cpp::UserAgentParser::device_type(
                  "Mozilla/5.0 (iPhone; CPU iPhone OS "
                  "5_1_1 like Mac OS X) AppleWebKit/534.46 "
                  "(KHTML, like Gecko) Version/5.1 "
                  "Mobile/9B206 Safari/7534.48.3") ==
              uap_cpp::DeviceType::kMobile);
}

TEST(UserAgentParser, DeviceTypeTablet) {
  ASSERT_TRUE(uap_cpp::UserAgentParser::device_type(
                  "Mozilla/5.0 (Linux; U; en-us; KFTT Build/IML74K) "
                  "AppleWebKit/535.19 (KHTML, like Gecko) Silk/2.0 "
                  "Safari/535.19 Silk-Accelerated=false") ==
              uap_cpp::DeviceType::kTablet);
}

TEST(UserAgentParser, DeviceTypeDesktop) {
  ASSERT_TRUE(
      uap_cpp::UserAgentParser::device_type(
          "Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.4 (KHTML, like "
          "Gecko) Chrome/98 Safari/537.4 (StatusCake)") ==
      uap_cpp::DeviceType::kDesktop);
}

bool has_field(const YAML::Node& root, const std::string& fname) {
  const auto& yaml_field = root[fname];
  return yaml_field.IsDefined();
}

std::string string_field(const YAML::Node& root, const std::string& fname) {
  const auto& yaml_field = root[fname];
  return yaml_field.IsNull() ? "" : yaml_field.as<std::string>();
}

void test_browser_or_os(const std::string file_path, const bool browser) {
  auto root = YAML::LoadFile(file_path);
  const auto& test_cases = root["test_cases"];
  for (const auto& test : test_cases) {
    const auto major = string_field(test, "major");
    const auto minor = string_field(test, "minor");
    const auto patch = string_field(test, "patch");
    const bool has_patch_minor = has_field(test, "patch_minor");
    const auto patch_minor =
        has_patch_minor ? string_field(test, "patch_minor") : "";
    const auto family = string_field(test, "family");
    const auto unparsed = string_field(test, "user_agent_string");
    const auto uagent = g_ua_parser.parse(unparsed);
    const auto& agent = browser ? uagent.browser : uagent.os;

    EXPECT_EQ(major, agent.major);
    EXPECT_EQ(minor, agent.minor);
    EXPECT_EQ(patch, agent.patch);
    if (has_patch_minor) {
      EXPECT_EQ(patch_minor, agent.patch_minor);
    }
    EXPECT_EQ(family, agent.family);
  }
}

void test_device(const std::string file_path) {
  auto root = YAML::LoadFile(file_path);
  const auto& test_cases = root["test_cases"];
  for (const auto& test : test_cases) {
    const auto unparsed = string_field(test, "user_agent_string");
    const auto uagent = g_ua_parser.parse(unparsed);
    const auto family = string_field(test, "family");
    const auto brand = string_field(test, "brand");
    const auto model = string_field(test, "model");

    EXPECT_EQ(family, uagent.device.family);
    EXPECT_EQ(brand, uagent.device.brand);
    EXPECT_EQ(model, uagent.device.model);
  }
}

TEST(OsVersion, test_os) {
  test_browser_or_os(UA_CORE_DIR + "/tests/test_os.yaml", false);
}

TEST(OsVersion, test_ua) {
  test_browser_or_os(UA_CORE_DIR + "/tests/test_ua.yaml", true);
}

TEST(BrowserVersion, firefox_user_agent_strings) {
  test_browser_or_os(
      UA_CORE_DIR + "/test_resources/firefox_user_agent_strings.yaml", true);
}

TEST(BrowserVersion, opera_mini_user_agent_strings) {
  test_browser_or_os(
      UA_CORE_DIR + "/test_resources/opera_mini_user_agent_strings.yaml", true);
}

TEST(BrowserVersion, pgts_browser_list) {
  test_browser_or_os(UA_CORE_DIR + "/test_resources/pgts_browser_list.yaml",
                     true);
}

TEST(OsVersion, additional_os_tests) {
  test_browser_or_os(UA_CORE_DIR + "/test_resources/additional_os_tests.yaml",
                     false);
}

TEST(BrowserVersion, podcasting_user_agent_strings) {
  test_browser_or_os(
      UA_CORE_DIR + "/test_resources/podcasting_user_agent_strings.yaml", true);
}

TEST(DeviceFamily, test_device) {
  test_device(UA_CORE_DIR + "/tests/test_device.yaml");
}

#ifdef WITH_MT_TEST
namespace {

void do_multithreaded_test(const std::function<void()>& work) {
  static constexpr int NUM_WORKERS = 4;

  std::vector<std::future<void>> workers;
  for (int i = 0; i < NUM_WORKERS; ++i) {
    workers.push_back(std::async(std::launch::async, [&work]() { work(); }));
  }
  for (auto& worker : workers) {
    worker.wait();
  }
}

}  // namespace

TEST(OsVersion, test_os_mt) {
  do_multithreaded_test(
      [] { test_browser_or_os(UA_CORE_DIR + "/tests/test_os.yaml", false); });
}

TEST(OsVersion, test_ua_mt) {
  do_multithreaded_test(
      [] { test_browser_or_os(UA_CORE_DIR + "/tests/test_ua.yaml", true); });
}

TEST(BrowserVersion, firefox_user_agent_strings_mt) {
  do_multithreaded_test([] {
    test_browser_or_os(
        UA_CORE_DIR + "/test_resources/firefox_user_agent_strings.yaml", true);
  });
}

TEST(BrowserVersion, opera_mini_user_agent_strings_mt) {
  do_multithreaded_test([] {
    test_browser_or_os(
        UA_CORE_DIR + "/test_resources/opera_mini_user_agent_strings.yaml",
        true);
  });
}

TEST(BrowserVersion, pgts_browser_list_mt) {
  do_multithreaded_test([] {
    test_browser_or_os(UA_CORE_DIR + "/test_resources/pgts_browser_list.yaml",
                       true);
  });
}

TEST(OsVersion, additional_os_tests_mt) {
  do_multithreaded_test([] {
    test_browser_or_os(UA_CORE_DIR + "/test_resources/additional_os_tests.yaml",
                       false);
  });
}

TEST(BrowserVersion, podcasting_user_agent_strings_mt) {
  do_multithreaded_test([] {
    test_browser_or_os(
        UA_CORE_DIR + "/test_resources/podcasting_user_agent_strings.yaml",
        true);
  });
}

TEST(DeviceFamily, test_device_mt) {
  do_multithreaded_test(
      [] { test_device(UA_CORE_DIR + "/tests/test_device.yaml"); });
}
#endif  // WITH_MT_TEST

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
