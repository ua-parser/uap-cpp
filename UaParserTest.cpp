#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include "UaParser.h"
#include "internal/AlternativeExpander.h"
#include "internal/Pattern.h"
#include "internal/ReplaceTemplate.h"
#include "internal/SnippetIndex.h"
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
  EXPECT_TRUE(uap_cpp::UserAgentParser::device_type(
                  "Mozilla/5.0 (iPhone; CPU iPhone OS 5_1_1 like Mac OS X) "
                  "AppleWebKit/534.46 (KHTML, like Gecko) Version/5.1 "
                  "Mobile/9B206 Safari/7534.48.3") ==
              uap_cpp::DeviceType::kMobile);
  EXPECT_TRUE(
      uap_cpp::UserAgentParser::device_type(
          "Mozilla/5.0 (Linux; Android 8.0.0; SAMSUNG SM-J330FN Build/R16NW) "
          "AppleWebKit/537.36 (KHTML, like Gecko) SamsungBrowser/7.2 "
          "Chrome/11.1.1111.111 Mobile Safari/537.36") ==
      uap_cpp::DeviceType::kMobile);
}

TEST(UserAgentParser, DeviceTypeTablet) {
  EXPECT_TRUE(uap_cpp::UserAgentParser::device_type(
                  "Mozilla/5.0 (Linux; U; en-us; KFTT Build/IML74K) "
                  "AppleWebKit/535.19 (KHTML, like Gecko) Silk/2.0 "
                  "Safari/535.19 Silk-Accelerated=false") ==
              uap_cpp::DeviceType::kTablet);
  EXPECT_TRUE(
      uap_cpp::UserAgentParser::device_type(
          "Mozilla/5.0 (Linux; Android 9; SHT-AL09 Build/HUAWEISHT-AL09; wv) "
          "AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 "
          "Chrome/79.0.3945.136 Safari/537.36") ==
      uap_cpp::DeviceType::kTablet);
}

TEST(UserAgentParser, DeviceTypeDesktop) {
  ASSERT_TRUE(
      uap_cpp::UserAgentParser::device_type(
          "Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.4 (KHTML, like "
          "Gecko) Chrome/98 Safari/537.4 (StatusCake)") ==
      uap_cpp::DeviceType::kDesktop);
}

TEST(UserAgentParser, DeviceTypeMobileOrTablet) {
  const auto test_cases = YAML::LoadFile("./test_device_type_mobile.yaml");
  for (const auto& test : test_cases) {
    const auto device_type =
        uap_cpp::UserAgentParser::device_type(test.as<std::string>());
    ASSERT_TRUE(device_type == uap_cpp::DeviceType::kMobile ||
                device_type == uap_cpp::DeviceType::kTablet);
  }
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
    if (has_patch_minor && !browser) {
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

void test_snippets(const std::string& expression,
                   std::vector<std::string> should_match) {
  uap_cpp::SnippetIndex index;
  auto snippet_set = index.registerSnippets(expression);
  auto snippet_string_map = index.getRegisteredSnippets();

  std::vector<std::string> snippet_strings;
  for (auto snippetId : snippet_set) {
    auto it = snippet_string_map.find(snippetId);
    ASSERT_NE(it, snippet_string_map.end());
    snippet_strings.emplace_back(it->second);
  }

  EXPECT_EQ(snippet_strings, should_match);
}

TEST(SnippetIndex, snippets) {
  test_snippets("foo.bar", {"foo", "bar"});
  test_snippets("foodbar", {"foodbar"});
  test_snippets("food?bar", {"foo", "bar"});
  test_snippets("foo.?bar", {"foo", "bar"});
  test_snippets("foo\\.bar", {"foo", ".bar"});
  test_snippets("(foo)", {"foo"});
  test_snippets("toto(foo|bar)tata", {"toto", "tata"});
  test_snippets("toto(foo)tata", {"toto", "foo", "tata"});
  test_snippets("toto(foo)?tata", {"toto", "tata"});
  test_snippets("toto(foo)*tata", {"toto", "tata"});
  test_snippets("toto(foo){0,10}tata", {"toto", "tata"});
  test_snippets("toto(foo){0,}tata", {"toto", "tata"});
  test_snippets("toto(foo){1,10}tata", {"toto", "foo", "tata"});
  test_snippets("toto(foo){1,}tata", {"toto", "foo", "tata"});
  test_snippets("foo[abc]bar", {"foo", "bar"});
  test_snippets("foo[abc]+bar", {"foo", "bar"});
  test_snippets("foo|bar", {});
  test_snippets("foo[|]bar", {"foo", "bar"});
  test_snippets("[(foo)]bar", {"bar"});
  test_snippets("[]foo]bar", {"bar"});
  test_snippets("(foo[abc)])bar", {"foo", "bar"});
  test_snippets("(foo(abc))bar", {"foo", "abc", "bar"});
  test_snippets("Foo.bAR", {"foo", "bar"});
  test_snippets("/(\\d+)\\.?foo(\\d+)", {"foo"});
  test_snippets("(?:foo|bar);.*(baz)/(\\d+)\\.(\\d+)", {"baz"});
}

void test_expand(const std::string& expression,
                 std::vector<std::string> should_match) {
  EXPECT_EQ(uap_cpp::AlternativeExpander::expand(expression), should_match);
}

TEST(AlternativeExpander, expansions) {
  test_expand("a", {"a"});
  test_expand("(a)", {"(a)"});
  test_expand("(a|b)", {"(a)", "(b)"});
  test_expand("(?:a|b)", {"(?:a)", "(?:b)"});
  test_expand("(a|b)?", {"(a|b)?"});
  test_expand("(a|b)+", {"(a)+", "(b)+"});
  test_expand("(a|b)*", {"(a|b)*"});
  test_expand("(a|b){0,2}", {"(a|b){0,2}"});
  test_expand("a|b", {"a", "b"});
  test_expand("x(a|b)y", {"x(a)y", "x(b)y"});
  test_expand("(a|b)yz", {"(a)yz", "(b)yz"});
  test_expand("xa|by", {"xa", "by"});
  test_expand("(a|b)(c|d)", {"(a)(c)", "(a)(d)", "(b)(c)", "(b)(d)"});
  test_expand("(a(b|c)|d)", {"(a(b))", "(a(c))", "(d)"});
  test_expand("(a", {"(a"});
  test_expand("(a|b", {"(a|b"});
  test_expand("((a", {"((a"});
  test_expand("((a|b", {"((a|b"});
  test_expand("(a((a|b", {"(a((a|b"});
  test_expand("[ab]", {"[ab]"});
  test_expand("[(|)]", {"[(|)]"});
  test_expand("[(a|a)]", {"[(a|a)]"});
  test_expand("a(|)b", {"a()b", "a()b"});
  test_expand("([ab]|[bc])", {"([ab])", "([bc])"});
  test_expand("[[](a|b)", {"[[](a)", "[[](b)"});
  test_expand("[]abc](a|b)", {"[]abc](a)", "[]abc](b)"});
}

std::string match_and_expand(const std::string& expression,
                             const std::string& input_string,
                             const std::string& replace_template) {
  uap_cpp::Pattern p(expression);
  uap_cpp::Match m;
  if (p.match(input_string, m)) {
    return uap_cpp::ReplaceTemplate(replace_template).expand(m);
  }
  return "";
}

TEST(ReplaceTemplate, expansions) {
  EXPECT_EQ(match_and_expand("something", "other", "foo"), "");
  EXPECT_EQ(match_and_expand("something", "something", "foo"), "foo");
  EXPECT_EQ(match_and_expand("some(thing)", "something", "no$1"), "nothing");
  EXPECT_EQ(match_and_expand("so([Mm])eth(ing)", "that's something!", "$1$2$3"),
            "ming");
  EXPECT_EQ(match_and_expand("([^ ]+) (.+)", "a b", "$2-$1"), "b-a");
}
}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
