#include "UaParser.h"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "internal/AlternativeExpander.h"
#include "internal/MakeUnique.h"
#include "internal/Pattern.h"
#include "internal/ReplaceTemplate.h"
#include "internal/SnippetIndex.h"
#include "internal/SnippetMapping.h"
#include "internal/StringUtils.h"

namespace {

struct GenericStore {
  uap_cpp::ReplaceTemplate replacement;
  uap_cpp::Pattern regExpr;
  int index{0};
};

struct DeviceStore : GenericStore {
  uap_cpp::ReplaceTemplate brandReplacement;
  uap_cpp::ReplaceTemplate modelReplacement;
};

struct AgentStore : GenericStore {
  uap_cpp::ReplaceTemplate majorVersionReplacement;
  uap_cpp::ReplaceTemplate minorVersionReplacement;
  uap_cpp::ReplaceTemplate patchVersionReplacement;
  uap_cpp::ReplaceTemplate patchMinorVersionReplacement;
};

struct GenericStoreComparator {
  bool operator()(const GenericStore* lhs, const GenericStore* rhs) const {
    return lhs->index < rhs->index;
  }
};

void fill_device_store(const YAML::Node& device_parser,
                       std::vector<std::unique_ptr<DeviceStore>>& device_stores,
                       uap_cpp::SnippetIndex& snippet_index,
                       uap_cpp::SnippetMapping<const DeviceStore*>& mappings) {
  device_stores.emplace_back(uap_cpp::make_unique<DeviceStore>());
  DeviceStore& device = *device_stores.back();
  device.index = device_stores.size();

  std::string regex;
  bool regex_flag = false;
  for (auto it = device_parser.begin(); it != device_parser.end(); ++it) {
    const auto& key = it->first.as<std::string>();
    const auto& value = it->second.as<std::string>();
    if (key == "regex") {
      regex = value;
    } else if (key == "regex_flag" && value == "i") {
      regex_flag = true;
    } else if (key == "device_replacement") {
      device.replacement = value;
    } else if (key == "model_replacement") {
      device.modelReplacement = value;
    } else if (key == "brand_replacement") {
      device.brandReplacement = value;
    } else {
      assert(false);
    }
  }

  device.regExpr.assign(regex, !regex_flag);

  for (const auto& e : uap_cpp::AlternativeExpander::expand(regex)) {
    auto snippets = snippet_index.registerSnippets(e);
    mappings.addMapping(snippets, &device);
  }
}

void fill_agent_store(const YAML::Node& node,
                      const std::string& repl,
                      const std::string& major_repl,
                      const std::string& minor_repl,
                      const std::string& patch_repl,
                      std::vector<std::unique_ptr<AgentStore>>& agent_stores,
                      uap_cpp::SnippetIndex& snippet_index,
                      uap_cpp::SnippetMapping<const AgentStore*>& mapping) {
  agent_stores.emplace_back(uap_cpp::make_unique<AgentStore>());
  AgentStore& agent_store = *agent_stores.back();
  agent_store.index = agent_stores.size();

  assert(node.Type() == YAML::NodeType::Map);
  for (auto it = node.begin(); it != node.end(); ++it) {
    const auto& key = it->first.as<std::string>();
    const auto& value = it->second.as<std::string>();
    if (key == "regex") {
      agent_store.regExpr.assign(value);

      for (const auto& e : uap_cpp::AlternativeExpander::expand(value)) {
        auto snippets = snippet_index.registerSnippets(e);
        mapping.addMapping(snippets, &agent_store);
      }
    } else if (key == repl) {
      agent_store.replacement = value;
    } else if (key == major_repl && !value.empty()) {
      if (value != "$2") {
        agent_store.majorVersionReplacement = value;
      }
    } else if (key == minor_repl && !value.empty()) {
      if (value != "$3") {
        agent_store.minorVersionReplacement = value;
      }
    } else if (key == patch_repl && !value.empty()) {
      if (value != "$4") {
        agent_store.patchVersionReplacement = value;
      }
    } else {
      // Ignore invalid key.
    }
  }
}

struct UAStore {
  explicit UAStore(const std::string& regexes_file_path) {
    auto regexes = YAML::LoadFile(regexes_file_path);

    const auto& user_agent_parsers = regexes["user_agent_parsers"];
    for (const auto& user_agent : user_agent_parsers) {
      fill_agent_store(user_agent,
                       "family_replacement",
                       "v1_replacement",
                       "v2_replacement",
                       "v3_replacement",
                       browserStore,
                       browserSnippetIndex,
                       browserMapping);
    }

    const auto& os_parsers = regexes["os_parsers"];
    for (const auto& o : os_parsers) {
      fill_agent_store(o,
                       "os_replacement",
                       "os_v1_replacement",
                       "os_v2_replacement",
                       "os_v3_replacement",
                       osStore,
                       osSnippetIndex,
                       osMapping);
    }

    const auto& device_parsers = regexes["device_parsers"];
    for (const auto& device_parser : device_parsers) {
      fill_device_store(
          device_parser, deviceStore, deviceSnippetIndex, deviceMapping);
    }
  }

  std::vector<std::unique_ptr<DeviceStore>> deviceStore;
  std::vector<std::unique_ptr<AgentStore>> osStore;
  std::vector<std::unique_ptr<AgentStore>> browserStore;

  uap_cpp::SnippetIndex deviceSnippetIndex;
  uap_cpp::SnippetIndex osSnippetIndex;
  uap_cpp::SnippetIndex browserSnippetIndex;

  uap_cpp::SnippetMapping<const DeviceStore*> deviceMapping;
  uap_cpp::SnippetMapping<const AgentStore*> osMapping;
  uap_cpp::SnippetMapping<const AgentStore*> browserMapping;
};

/////////////
// HELPERS //
/////////////

uap_cpp::Device parse_device_impl(const std::string& ua,
                                  const UAStore* ua_store) {
  uap_cpp::Device device;

  auto snippets = ua_store->deviceSnippetIndex.getSnippets(ua);

  std::set<const DeviceStore*, GenericStoreComparator> regexps;
  ua_store->deviceMapping.getExpressions(snippets, regexps);

  for (const auto& entry : regexps) {
    const auto& d = *entry;

    thread_local uap_cpp::Match m;

    if (d.regExpr.match(ua, m)) {
      if (d.replacement.empty() && m.size() > 1) {
        device.family = m.get(1);
      } else {
        device.family = d.replacement.expand(m);
      }
      trim(device.family);

      if (!d.brandReplacement.empty()) {
        device.brand = d.brandReplacement.expand(m);
        trim(device.brand);
      }

      if (d.modelReplacement.empty() && m.size() > 1) {
        device.model = m.get(1);
      } else {
        device.model = d.modelReplacement.expand(m);
      }
      trim(device.model);

      break;
    }
  }

  return device;
}

template <class AGENT, class AGENT_STORE>
void fill_agent(AGENT& agent,
                const AGENT_STORE& store,
                const uap_cpp::Match& m) {
  if (store.replacement.empty() && m.size() > 1) {
    agent.family = m.get(1);
  } else {
    agent.family = store.replacement.expand(m);
  }
  trim(agent.family);

  if (!store.majorVersionReplacement.empty()) {
    agent.major = store.majorVersionReplacement.expand(m);
  } else if (m.size() > 2) {
    agent.major = m.get(2);
  }
  if (!store.minorVersionReplacement.empty()) {
    agent.minor = store.minorVersionReplacement.expand(m);
  } else if (m.size() > 3) {
    agent.minor = m.get(3);
  }
  if (!store.patchVersionReplacement.empty()) {
    agent.patch = store.patchVersionReplacement.expand(m);
  } else if (m.size() > 4) {
    agent.patch = m.get(4);
  }
  if (m.size() == 6 && (m.get(5).empty() || m.get(5)[0] != '.')) {
    agent.patch_minor = m.get(5);
  }
}

uap_cpp::Agent parse_browser_impl(const std::string& ua,
                                  const UAStore* ua_store) {
  uap_cpp::Agent browser;

  auto snippets = ua_store->browserSnippetIndex.getSnippets(ua);

  std::set<const AgentStore*, GenericStoreComparator> regexps;
  ua_store->browserMapping.getExpressions(snippets, regexps);

  for (const auto& entry : regexps) {
    const auto& b = *entry;
    thread_local uap_cpp::Match m;
    if (b.regExpr.match(ua, m)) {
      fill_agent(browser, b, m);
      break;
    }
  }

  return browser;
}

uap_cpp::Agent parse_os_impl(const std::string& ua, const UAStore* ua_store) {
  uap_cpp::Agent os;

  auto snippets = ua_store->osSnippetIndex.getSnippets(ua);

  std::set<const AgentStore*, GenericStoreComparator> regexps;
  ua_store->osMapping.getExpressions(snippets, regexps);

  for (const auto& entry : regexps) {
    const auto& o = *entry;
    thread_local uap_cpp::Match m;
    if (o.regExpr.match(ua, m)) {
      fill_agent(os, o, m);
      break;
    }
  }

  return os;
}

}  // namespace

namespace uap_cpp {

UserAgentParser::UserAgentParser(const std::string& regexes_file_path)
    : regexes_file_path_{regexes_file_path} {
  ua_store_ = new UAStore(regexes_file_path);
}

UserAgentParser::~UserAgentParser() {
  delete static_cast<const UAStore*>(ua_store_);
}

UserAgent UserAgentParser::parse(const std::string& ua) const noexcept {
  const auto ua_store = static_cast<const UAStore*>(ua_store_);

  try {
    const auto device = parse_device_impl(ua, ua_store);
    const auto os = parse_os_impl(ua, ua_store);
    const auto browser = parse_browser_impl(ua, ua_store);
    return {device, os, browser};
  } catch (...) {
    return {Device(), Agent(), Agent()};
  }
}

Device UserAgentParser::parse_device(const std::string& ua) const noexcept {
  try {
    return parse_device_impl(ua, static_cast<const UAStore*>(ua_store_));
  } catch (...) {
    return Device();
  }
}

Agent UserAgentParser::parse_os(const std::string& ua) const noexcept {
  try {
    return parse_os_impl(ua, static_cast<const UAStore*>(ua_store_));
  } catch (...) {
    return Agent();
  }
}

Agent UserAgentParser::parse_browser(const std::string& ua) const noexcept {
  try {
    return parse_browser_impl(ua, static_cast<const UAStore*>(ua_store_));
  } catch (...) {
    return Agent();
  }
}

DeviceType UserAgentParser::device_type(const std::string& ua) noexcept {
  // https://gist.github.com/dalethedeveloper/1503252/931cc8b613aaa930ef92a4027916e6687d07feac
  static const uap_cpp::Pattern rx_mob(
      "Mobile|iP(hone|od|ad)|Android|BlackBerry|IEMobile|Kindle|NetFront|Silk-"
      "Accelerated|(hpw|web)OS|Fennec|Minimo|Opera "
      "M(obi|ini)|Blazer|Dolfin|Dolphin|Skyfire|Zune");
  static const uap_cpp::Pattern rx_tabl(
      "(tablet|ipad|playbook|silk)|(android.*)", false);
  thread_local uap_cpp::Match m;
  try {
    if (rx_tabl.match(ua, m) &&
        m.get(2).find("Mobile") == std::string::npos) {
      return DeviceType::kTablet;
    } else if (rx_mob.match(ua, m)) {
      return DeviceType::kMobile;
    }
    return DeviceType::kDesktop;
  } catch (...) {
    return DeviceType::kUnknown;
  }
}

}  // namespace uap_cpp
