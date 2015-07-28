#include "UaParser.h"

#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <glog/logging.h>
#include <yaml-cpp/yaml.h>

namespace {

struct GenericStore {
  std::string replacement;
  boost::regex regExpr;
};

struct DeviceStore : GenericStore {
  std::string brandReplacement;
  std::string modelReplacement;
};

struct AgentStore : GenericStore {
  std::string majorVersionReplacement;
  std::string minorVersionReplacement;
};

typedef AgentStore OsStore;
typedef AgentStore BrowserStore;

#define FILL_AGENT_STORE(node, agent_store, repl, maj_repl, min_repl)    \
    CHECK(node.Type() == YAML::NodeType::Map);                           \
    for (auto it = node.begin(); it != node.end(); ++it) {               \
      const auto key = it->first.as<std::string>();                      \
      const auto value = it->second.as<std::string>();                   \
      if (key == "regex") {                                              \
        agent_store.regExpr.assign(value,                                \
          boost::regex::optimize);                                       \
      } else if (key == repl) {                                          \
        agent_store.replacement = value;                                 \
      } else if (key == maj_repl && !value.empty()) {                    \
        agent_store.majorVersionReplacement = value;                     \
      } else if (key == min_repl && !value.empty()) {                    \
        try {                                                            \
          agent_store.minorVersionReplacement = value;                   \
        } catch (...) {}                                                 \
      } else {                                                           \
        CHECK(false);                                                    \
      }                                                                  \
    }

struct UAStore {
  explicit UAStore(const std::string& regexes_file_path) {
    auto regexes = YAML::LoadFile(regexes_file_path);

    const auto& user_agent_parsers = regexes["user_agent_parsers"];
    for (const auto& user_agent : user_agent_parsers) {
      BrowserStore browser;
      FILL_AGENT_STORE(user_agent, browser, "family_replacement",
        "v1_replacement", "v2_replacement");
      browserStore.push_back(browser);
    }

    const auto& os_parsers = regexes["os_parsers"];
    for (const auto& o : os_parsers) {
      OsStore os;
      FILL_AGENT_STORE(o, os, "os_replacement", "os_v1_replacement",
        "os_v2_replacement");
      osStore.push_back(os);
    }

    const auto& device_parsers = regexes["device_parsers"];
    for (const auto& d : device_parsers) {
      DeviceStore device;
      for (auto it = d.begin(); it != d.end(); ++it) {
        const auto key = it->first.as<std::string>();
        const auto value = it->second.as<std::string>();
        if (key == "regex") {
          device.regExpr.assign(value, boost::regex::optimize);
        }
        else if ( key == "regex_flag" && value == "i" ) {
          device.regExpr.assign(device.regExpr.str(), boost::regex::optimize|boost::regex::icase);
        } else if (key == "device_replacement") {
          device.replacement = value;
        } else if (key == "model_replacement") {
          device.modelReplacement = value;
        } else if (key == "brand_replacement") {
          device.brandReplacement = value;
        } else {
          CHECK(false);
        }
      }
      deviceStore.push_back(device);
    }
  }

  std::vector<DeviceStore> deviceStore;
  std::vector<OsStore> osStore;
  std::vector<BrowserStore> browserStore;
};


////////////
// HELPERS //
/////////////

//Device parsing is different than the other parsing in that a field can have many placeholders
const char* placeholders[] = {"$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9"};
void replace_all_placeholders(std::string& device_property, const boost::smatch &result) {
  std::string::size_type loc;
  for (size_t idx = 1; idx < result.size(); ++idx) {
    loc = device_property.find(placeholders[idx-1]);
    if (loc != std::string::npos) {
      if (result[idx].matched) {
        device_property.replace(loc,2,result[idx].str());
      }
      else
        device_property.erase(loc,2);
    }
  }
  // There should not be placehoders leftover. This is a Workaround ...
  loc = device_property.find_first_of('$');
  if ( loc != std::string::npos )
    device_property.erase(loc,device_property.size());
  return;
}




template<class AGENT, class AGENT_STORE>
void fillAgent(AGENT& agent, const AGENT_STORE& store, const boost::smatch& m) {
  CHECK(!m.empty());
  if (m.size() > 1) {
    agent.family = !store.replacement.empty()
      ? boost::regex_replace(store.replacement, boost::regex("\\$1"), m[1].str())
      : m[1];
  } else {
    agent.family = !store.replacement.empty()
      ? boost::regex_replace(store.replacement, boost::regex("\\$1"), m[0].str())
      : m[0];
  }
  if (!store.majorVersionReplacement.empty()) {
    agent.major = store.majorVersionReplacement;
  } else if (m.size() > 2) {
    const auto s = m[2].str();
    if (!s.empty()) {
      agent.major = s;
    }
  }
  if (!store.minorVersionReplacement.empty()) {
    agent.minor = store.minorVersionReplacement;
  } else if (m.size() > 3) {
    const auto s = m[3].str();
    if (!s.empty()) {
      agent.minor = s;
    }
  }
  if (m.size() > 4) {
    const auto s = m[4].str();
    if (!s.empty()) {
      agent.patch = s;
    }
  }
}


UserAgent parseImpl(const std::string& ua, const UAStore* ua_store) {
  UserAgent uagent;

  for (const auto& b : ua_store->browserStore) {
    auto& browser = uagent.browser;
    boost::smatch m;
    if (boost::regex_search(ua, m, b.regExpr)) {
      fillAgent(browser, b, m);
      break;
    } else {
      browser.family = "Other";
    }
  }

  for (const auto& o : ua_store->osStore) {
    auto& os = uagent.os;
    boost::smatch m;
    if (boost::regex_search(ua, m, o.regExpr)) {
      fillAgent(os, o, m);
      break;
    } else {
      os.family = "Other";
    }
  }

  for (const auto& d : ua_store->deviceStore) {
    auto& device = uagent.device;
    boost::smatch m;

    if (boost::regex_search(ua, m, d.regExpr)) {
      if ( d.replacement.empty() && m.size() > 1) {
        device.family = m[1].str();
      } else {
        device.family = d.replacement;
        replace_all_placeholders(device.family, m);
        boost::algorithm::trim(device.family);
      }

      if ( ! d.brandReplacement.empty() ) {
        device.brand = d.brandReplacement;
        replace_all_placeholders(device.brand, m);
        boost::algorithm::trim(device.brand);
      }

      if (d.modelReplacement.empty() && m.size() > 1) {
        device.model = m[1].str();
      } else {
          device.model = d.modelReplacement;
          replace_all_placeholders(device.model, m);
          boost::algorithm::trim(device.model);
      }
      break;
    } else {
      device.family = "Other";
    }
  }
  return uagent;
}

}  // namespace

UserAgentParser::UserAgentParser(const std::string& regexes_file_path)
  : regexes_file_path_ { regexes_file_path } {
  ua_store_ = new UAStore(regexes_file_path);
}

UserAgentParser::~UserAgentParser() {
  delete static_cast<const UAStore*>(ua_store_);
}

UserAgent UserAgentParser::parse(const std::string& ua) const {
  return parseImpl(ua, static_cast<const UAStore*>(ua_store_));
}
