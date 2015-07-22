#include "UaParser.h"

#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>

#include <boost/regex.hpp>
#include <glog/logging.h>
#include <yaml-cpp/yaml.h>

namespace {

  struct GenericStore {
      std::string replacement;
      boost::regex regExpr;
  };

  struct DeviceStore : GenericStore {
    std::string brand_replacement;
    std::string model_replacement;
    boost::regex regExpr;
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
        agent_store.regExpr = value;                                     \
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
          device.regExpr.assign(value);
        } else if ( key == "regex_flag") {
            if ( value == "i")
              device.regExpr.assign(device.regExpr.str(), boost::regex::icase);
        } else if (key == "device_replacement") {
          device.replacement = value;
        } else if (key == "model_replacement") {
          device.model_replacement = value;
        } else if (key == "brand_replacement") {
          device.brand_replacement = value;
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

void find_and_replace(std::string &original, const std::string &tofind, const std::string &toreplace) {
    std::string::size_type loc = original.find(tofind);
    if (loc == std::string::npos) {
        return;
    }

    //Because there can be spaces leading the replacement symbol " $1" and when toreplace is empty
    // This can lead to multiple spaces e.g.: "HTC Desire   " or "HTC   Desire"
    //   But you want to avoid the case of "HTC $1$2" where $2 is not empty but $2 , you don't wan't "HTCxyz"
    // If you're replacing empty:
    //   If you have a leading whitespace
    //     If the next character after the placeholder (loc+2) is also a white space OR it's the end of the string
    //       Then get rid of the leading whitespace
    if (toreplace.size()==0) {
        if (loc != 0 && original[loc-1] == ' ') {
            if( (loc+2 < original.size() && original[loc+2]==' ') || (loc+2 >= original.size()) ) {
                original.replace(loc-1, tofind.size()+1, toreplace);
                return;
            }
        }
    }
    //Otherwise just do a normal replace
    original.replace(loc, tofind.size(), toreplace);
}


//Device parsing is different than the other parsing in that a field can have many placeholders
const char* placeholders[]= {"$1", "$2", "$3", "$4","$5", "$6", "$7", "$8", "$9"};
void replace_all_placeholders(std::string& device_property, const boost::smatch &result) {
    for (unsigned int idx = 1; idx < result.size(); ++idx) {
        std::string replacement;
        try {
            replacement = result[idx].str();
        }
        catch (std::length_error &e) {
            replacement = std::string("");
        }
        find_and_replace(device_property, placeholders[idx-1], replacement);
    }
    return;
}


void remove_trailing_whitespaces(std::string &original) {
    //Sometimes te regexes don't actually parse correctly leaving trailing whitespaces
    //The if statment is so that we don't always do a find.
    if (original.back()== ' ') {
        size_t pos = original.find_last_not_of(' ');
        if (pos != std::string::npos) {
            original.resize(pos+1);
        }
    }
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

      if ( d.replacement.empty() && m.size() == 1 ){
        device.family = m[0].str();
      }
      else if (d.replacement.empty() && m.size() > 1) {
        device.family = m[1].str();
      }
      else{
        device.family = d.replacement;
        replace_all_placeholders(device.family, m);
        device.family = boost::regex_replace(device.family, boost::regex("\\$[0-9]"),"");
        remove_trailing_whitespaces(device.family);
      }

     if (d.brand_replacement.empty() && m.size() > 2) {
        device.brand = m[2].str();
      }
      else {
        device.brand = d.brand_replacement;
        replace_all_placeholders(device.brand, m);
        remove_trailing_whitespaces(device.brand);
      }

      if (d.model_replacement.empty() && m.size() > 3) {
         device.model = m[3].str();
       }
       else {
         device.model = d.model_replacement;
         replace_all_placeholders(device.model, m);
         remove_trailing_whitespaces(device.model);
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
