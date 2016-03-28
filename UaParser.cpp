#include "UaParser.h"

#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <yaml-cpp/yaml.h>

namespace {

typedef std::map<std::string::size_type, size_t> i2tuple;

struct GenericStore {
  std::string replacement;
  i2tuple replacementMap;
  boost::regex regExpr;
};

struct DeviceStore : GenericStore {
  std::string brandReplacement;
  std::string modelReplacement;
  i2tuple brandReplacementMap;
  i2tuple modelReplacementMap;
};

struct AgentStore : GenericStore {
  std::string majorVersionReplacement;
  std::string minorVersionReplacement;
};

void mark_placeholders(i2tuple& replacement_map, const std::string& device_property) {
  auto loc = device_property.rfind("$");
  while (loc != std::string::npos) {
    replacement_map[loc] = std::stoi(device_property.substr(loc + 1, 1));

    if (loc < 2)
      break;

    loc = device_property.rfind("$", loc - 2);
  }
  return;
}

AgentStore fill_agent_store(const YAML::Node& node,
                            const std::string& repl,
                            const std::string& maj_repl,
                            const std::string& min_repl) {
  AgentStore agent_store;
  assert(node.Type() == YAML::NodeType::Map);
  for (auto it = node.begin(); it != node.end(); ++it) {
    const auto key = it->first.as<std::string>();
    const auto value = it->second.as<std::string>();
    if (key == "regex") {
      agent_store.regExpr.assign(value, boost::regex::optimize | boost::regex::normal);
    } else if (key == repl) {
      agent_store.replacement = value;
      mark_placeholders(agent_store.replacementMap, agent_store.replacement);
    } else if (key == maj_repl && !value.empty()) {
      agent_store.majorVersionReplacement = value;
    } else if (key == min_repl && !value.empty()) {
      try {
        agent_store.minorVersionReplacement = value;
      } catch (...) {
      }
    } else {
      assert(false);
    }
  }
  return agent_store;
}

struct UAStore {
  explicit UAStore(const std::string& regexes_file_path) {
    auto regexes = YAML::LoadFile(regexes_file_path);

    const auto& user_agent_parsers = regexes["user_agent_parsers"];
    for (const auto& user_agent : user_agent_parsers) {
      const auto browser = fill_agent_store(user_agent, "family_replacement", "v1_replacement", "v2_replacement");
      browserStore.push_back(browser);
    }

    const auto& os_parsers = regexes["os_parsers"];
    for (const auto& o : os_parsers) {
      const auto os = fill_agent_store(o, "os_replacement", "os_v1_replacement", "os_v2_replacement");
      osStore.push_back(os);
    }

    const auto& device_parsers = regexes["device_parsers"];
    for (const auto& d : device_parsers) {
      DeviceStore device;
      bool regex_flag = false;
      for (auto it = d.begin(); it != d.end(); ++it) {
        const auto key = it->first.as<std::string>();
        const auto value = it->second.as<std::string>();
        if (key == "regex") {
          device.regExpr.assign(value, boost::regex::optimize | boost::regex::normal);
        } else if (key == "regex_flag" && value == "i") {
          regex_flag = true;
        } else if (key == "device_replacement") {
          device.replacement = value;
          mark_placeholders(device.replacementMap, device.replacement);
        } else if (key == "model_replacement") {
          device.modelReplacement = value;
          mark_placeholders(device.modelReplacementMap, device.modelReplacement);
        } else if (key == "brand_replacement") {
          device.brandReplacement = value;
          mark_placeholders(device.brandReplacementMap, device.brandReplacement);
        } else {
          assert(false);
        }
      }
      if (regex_flag == true) {
        device.regExpr.assign(device.regExpr.str(),
                              boost::regex::optimize | boost::regex::icase | boost::regex::normal);
      }
      deviceStore.push_back(device);
    }
  }

  std::vector<DeviceStore> deviceStore;
  std::vector<AgentStore> osStore;
  std::vector<AgentStore> browserStore;
};

template <class AGENT, class AGENT_STORE>
void fill_agent(AGENT& agent, const AGENT_STORE& store, const boost::smatch& m) {
  if (m.size() > 1) {
    agent.family =
        !store.replacement.empty() ? boost::regex_replace(store.replacement, boost::regex("\\$1"), m[1].str()) : m[1];
  } else {
    agent.family =
        !store.replacement.empty() ? boost::regex_replace(store.replacement, boost::regex("\\$1"), m[0].str()) : m[0];
  }
  boost::algorithm::trim(agent.family);

  // The chunk above is slightly faster than the one below.
  // if ( store.replacement.empty() && m.size() > 1) {
  //   agent.family = m[1].str();
  // } else {
  //     agent.family = store.replacement;
  //     if ( ! store.replacementMap.empty()) {
  //       replace_all_placeholders(agent.family,m,store.replacementMap);
  //     }
  // }

  if (!store.majorVersionReplacement.empty()) {
    agent.major = store.majorVersionReplacement;
  } else if (m.size() > 2) {
    agent.major = m[2].str();
  }
  if (!store.minorVersionReplacement.empty()) {
    agent.minor = store.minorVersionReplacement;
  } else if (m.size() > 3) {
    agent.minor = m[3].str();
  }
  if (m.size() > 4) {
    agent.patch = m[4].str();
  }
}

/////////////
// HELPERS //
/////////////

void replace_all_placeholders(std::string& ua_property, const boost::smatch& result, const i2tuple& replacement_map) {
  for (auto iter = replacement_map.rbegin(); iter != replacement_map.rend(); ++iter) {
    ua_property.replace(iter->first, 2, result[iter->second].str());
  }
  boost::algorithm::trim(ua_property);
  return;
}

UserAgent parse_impl(const std::string& ua, const UAStore* ua_store) {
  UserAgent uagent;

  for (const auto& b : ua_store->browserStore) {
    auto& browser = uagent.browser;
    boost::smatch m;
    if (boost::regex_search(ua, m, b.regExpr)) {
      fill_agent(browser, b, m);
      break;
    } else {
      browser.family = "Other";
    }
  }

  for (const auto& o : ua_store->osStore) {
    auto& os = uagent.os;
    boost::smatch m;
    if (boost::regex_search(ua, m, o.regExpr)) {
      fill_agent(os, o, m);
      break;
    } else {
      os.family = "Other";
    }
  }

  for (const auto& d : ua_store->deviceStore) {
    auto& device = uagent.device;
    boost::smatch m;

    if (boost::regex_search(ua, m, d.regExpr)) {
      if (d.replacement.empty() && m.size() > 1) {
        device.family = m[1].str();
      } else {
        device.family = d.replacement;
        if (!d.replacementMap.empty()) {
          replace_all_placeholders(device.family, m, d.replacementMap);
        }
      }

      if (!d.brandReplacement.empty()) {
        device.brand = d.brandReplacement;
        if (!d.brandReplacementMap.empty()) {
          replace_all_placeholders(device.brand, m, d.brandReplacementMap);
        }
      }
      if (d.modelReplacement.empty() && m.size() > 1) {
        device.model = m[1].str();
      } else {
        device.model = d.modelReplacement;
        if (!d.modelReplacementMap.empty()) {
          replace_all_placeholders(device.model, m, d.modelReplacementMap);
        }
      }
      break;
    } else {
      device.family = "Other";
    }
  }
  return uagent;
}

}  // namespace

UserAgentParser::UserAgentParser(const std::string& regexes_file_path) : regexes_file_path_{regexes_file_path} {
  ua_store_ = new UAStore(regexes_file_path);
}

UserAgentParser::~UserAgentParser() {
  delete static_cast<const UAStore*>(ua_store_);
}

UserAgent UserAgentParser::parse(const std::string& ua) const {
  return parse_impl(ua, static_cast<const UAStore*>(ua_store_));
}
