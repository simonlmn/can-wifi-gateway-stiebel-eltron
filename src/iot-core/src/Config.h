#ifndef IOT_CORE_CONFIG_H_
#define IOT_CORE_CONFIG_H_

#include <LittleFS.h>
#include "Interfaces.h"

namespace iot_core {

class ConfigParser final : public IConfigParser {
public:
  static constexpr char SEPARATOR = '=';
  static constexpr char END = ';';

private:
  char* _config;

public:
  ConfigParser() : _config(nullptr) {}

  ConfigParser(char* config) : _config(config) {}
  
  bool parse(std::function<bool(char* name, const char* value)> processEntry) const override {
    if (_config == nullptr) {
      return false;
    }

    char* start = _config;
    char* separator = nullptr;
    char* end = nullptr;

    while((separator = strchr(start, SEPARATOR)) != nullptr) {
      if ((end = strchr(separator, END)) == nullptr) {
        break;
      }

      *separator = '\0';
      *end = '\0';
      bool success = processEntry(start, separator + 1);
      *separator = SEPARATOR;
      *end = END;
        
      if (!success) {
        break;
      }

      start = end + 1;

      while (*start == '\n') {
        start += 1;
      }
    }

    return *start == '\0';
  }
};

static const char CONFIG_FILE_HEADER[] = "~C1.0";
static constexpr size_t MAX_CONFIG_SIZE = 256u;
static char CONFIG_BUFFER[MAX_CONFIG_SIZE + 1u];

ConfigParser readConfigFile(const char* filename) {
  CONFIG_BUFFER[0] = '\0';
  char* buffer = CONFIG_BUFFER;

  auto configFile = LittleFS.open(filename, "r");
  if (configFile) {
    if (configFile.available() > 5) {
      char header[6] = {0};
      configFile.readBytes(header, 5);
      if (strcmp(header, CONFIG_FILE_HEADER) == 0) {
        auto size = configFile.readBytes(CONFIG_BUFFER, MAX_CONFIG_SIZE);
        CONFIG_BUFFER[size] = '\0';
        buffer = CONFIG_BUFFER;
      } else {
        // Different file format or version, ignore for now -> config will be empty.
        buffer = nullptr;
      }
    }
    configFile.close();
  }

  return {buffer};
}

void writeConfigFile(const char* filename, IConfigurable* configurable) {
  auto configFile = LittleFS.open(filename, "w");
  if (configFile) {
    configFile.write(CONFIG_FILE_HEADER);
    configurable->getConfig([&] (const char* name, const char* value) {
        configFile.write(name);
        configFile.write(ConfigParser::SEPARATOR);
        configFile.write(value);
        configFile.write(ConfigParser::END);
    });
    configFile.close();
  }
}

}

#endif
