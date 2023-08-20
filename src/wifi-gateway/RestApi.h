#pragma once

#include <ESP8266WebServer.h>
#include <Arduino_JSON.h>
#include <uri/UriBraces.h>
#include <uri/UriGlob.h>
#include "NodeBase.h"
#include "DataAccess.h"
#include "StiebelEltronProtocol.h"
#include "ResponseBuffer.h"

#if __has_include("src/wifi-gateway-ui/dist/wifi-gateway-ui.generated.h")
#  include "src/wifi-gateway-ui/dist/wifi-gateway-ui.generated.h"
#else
#  include "no-ui.generated.h"
#endif

static const char CONTENT_TYPE_PLAIN[] PROGMEM = "text/plain";
static const char CONTENT_TYPE_HTML[] PROGMEM = "text/html";
static const char CONTENT_TYPE_JSON[] PROGMEM = "application/json";
static const char HEADER_ACCEPT[] PROGMEM = "Accept";
static const char ARG_PLAIN[] PROGMEM = "plain";
static const char ARG_UPDATED_SINCE[] PROGMEM = "updatedSince";
static const char ARG_ONLY_UNDEFINED[] PROGMEM = "onlyUndefined";
static const char ARG_NUMBERS_AS_DECIMALS[] PROGMEM = "numbersAsDecimals";
static const char ARG_ACCESS_MODE[] PROGMEM = "accessMode";
static const char ARG_VALIDATE_ONLY[] PROGMEM = "validateOnly";

class RestApi {
private:
  ESP8266WebServer _server;
  ResponseBuffer<ESP8266WebServer> _buffer;

  NodeBase& _node;
  DataAccess& _access;
  StiebelEltronProtocol& _protocol;
  
public:
  RestApi(NodeBase& node, DataAccess& access, StiebelEltronProtocol& protocol) : _server(80), _buffer(_server), _node(node), _access(access), _protocol(protocol) {}
  
  void setup() {
    _server.enableCORS(true);
    _server.collectHeaders(FPSTR(HEADER_ACCEPT));

    _server.on(UriGlob(F("*")), HTTP_OPTIONS, [this]() { // generic reply to make "pre-flight" checks work
      _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
      _server.send(204);
    });
    
    _server.on(F("/"), HTTP_GET, [this]() {
      _server.sendHeader("Content-Encoding", "gzip");
      #ifdef WIFI_GATEWAY_UI_H
        _server.send_P(200, CONTENT_TYPE_HTML, wifi_gateway_ui::HTML, wifi_gateway_ui::HTML_SIZE);
      #else
        _server.send_P(200, CONTENT_TYPE_HTML, no_ui::HTML, no_ui::HTML_SIZE);
      #endif
    });

    _server.on(F("/api/data"), HTTP_GET, [this]() {
      if (_server.hasArg(FPSTR(ARG_ONLY_UNDEFINED))) {
        getItems([] (DataEntry const& entry) { return !entry.hasDefinition(); });
      } else {
        getItems();
      }
    });

    _server.on(UriBraces(F("/api/data/{}/{}/{}")), HTTP_PUT, [this]() {
      doItem([&] (DataAccess::DataKey const& key, DataEntry const& entry) {
        if (!entry.writable || !entry.hasDefinition()) {
          return false;
        }
        
        ValueAccessMode accessMode = getValueAccessModeFromString(_server.arg(FPSTR(ARG_ACCESS_MODE)).c_str());
        const char* valueString = _server.arg(FPSTR(ARG_PLAIN)).c_str();

        const char* space = strrchr(valueString, ' ');
        if (space != nullptr) {
          const char* unitSymbol = getUnitSymbol(entry.definition->unit);
          if (strcmp(space + 1, unitSymbol) != 0) {
            if (_node.logLevel("api") > 0) _node.log("api", format("PUT data: unit mismatch %s != %s", space + 1, unitSymbol));
            return false;
          }
        }

        uint32_t rawValue = entry.definition->toRaw(valueString, space);

        if (Codec::isError(rawValue)) {
          if (_node.logLevel("api") > 0) _node.log("api", format("PUT data: value error %s", valueString));
          return false;
        }
  
        return _access.write(key, rawValue, accessMode);
      });
    });

    _server.on(F("/api/subscriptions"), HTTP_GET, [this]() {
      getItems([] (DataEntry const& entry) { return entry.subscribed; });
    });

    _server.on(F("/api/subscriptions"), HTTP_POST, [this]() {      
      postItems([&] (DataAccess::DataKey const& key) { return _access.addSubscription(key); });
    });

    _server.on(UriBraces(F("/api/subscriptions/{}/{}/{}")), HTTP_DELETE, [this]() {
      doItem([&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeSubscription(key); return true; });
    });

    _server.on(F("/api/writable"), HTTP_GET, [this]() {
      getItems([](DataEntry const& entry) { return entry.writable; });
    });

    _server.on(F("/api/writable"), HTTP_POST, [this]() {
      postItems([&] (DataAccess::DataKey const& key) { return _access.addWritable(key); });
    });

    _server.on(UriBraces(F("/api/writable/{}/{}/{}")), HTTP_DELETE, [this]() {
      doItem([&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeWritable(key); return true; });
    });

    _server.on(F("/api/definitions"), HTTP_GET, [this]() {
      getDefinitions();
    });

    _server.on(F("/api/devices"), HTTP_GET, [this]() {
      getDevices();
    });
    
    _server.on(F("/api/node/reset"), HTTP_POST, [this]() {
      _node.reset();
      _server.send(204);
    });

    _server.on(F("/api/node/stop"), HTTP_POST, [this]() {
      _node.stop();
      _server.send(204);
    });

    _server.on(F("/api/node/status"), HTTP_GET, [this]() {
      _server.send(200, FPSTR(CONTENT_TYPE_JSON), JSON.stringify(_node.getDiagnostics()));
    });

    _server.on(F("/api/node/logs"), HTTP_GET, [this]() {
      bool sendHtml = _server.hasHeader(FPSTR(HEADER_ACCEPT)) && _server.header(FPSTR(HEADER_ACCEPT)).startsWith(FPSTR(CONTENT_TYPE_HTML));

      if (!_buffer.begin(200, sendHtml ? FPSTR(CONTENT_TYPE_HTML) : FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }

      if (sendHtml) {
        _buffer.plainText(F(
          "<!DOCTYPE html>\n"
          "<html><head><title>Node Logs</title></head><body>"
          "<pre id=\"log\"></pre>"
          "<div id=\"status\"></div>"
          "<script>"
            "function status(s){"
              "document.getElementById(\"status\").innerText = s;"
            "}"
            "function refresh(){"
              "status(\"Loading...\");"
              "fetch(\"http://")); _buffer.plainText(WiFi.localIP().toString().c_str()); _buffer.plainText(F(":8080/node/logs\")"
              ".then(r => r.text())"
              ".then(t => {"
                "document.getElementById(\"log\").innerHTML = t;"
                "window.scrollTo(0, document.body.scrollHeight);"
                "status(\"Updated: \" + (new Date()).toISOString());"
                "setTimeout(refresh, 5000);"
              "})"
              ".catch(e => {"
                "status(\"Error: \" + e);"
                "setTimeout(refresh, 5000);"
              "});"
            "}"
            "refresh();"
          "</script>"
        "</body></html>"));
      } else {
        _node.getLogger().output([&] (const char* entry) {
          _buffer.plainText(entry);
          _node.lyield();
        });
      }

      _buffer.end();
    });

    _server.on(UriBraces(F("/api/node/log-level/{}")), HTTP_PUT, [this]() {
      const auto& category = _server.pathArg(0);
      auto logLevel = _server.arg("plain").toInt();

      _node.logLevel(category, logLevel);
      
      _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), String((int)_node.logLevel(category)));
    });

    _server.on(F("/api/node/config"), HTTP_GET, [this]() {
      if (!_buffer.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }

      _node.getAllConfig([&] (const char* path, const char* value) {
        _buffer.plainText(path);
        _buffer.plainChar(ConfigParser::SEPARATOR);
        _buffer.plainText(value);
        _buffer.plainChar(ConfigParser::END);
        _buffer.plainChar('\n');
      });

      _buffer.end();
    });

    _server.on(F("/api/node/config"), HTTP_PUT, [this]() {
      const char* body = _server.arg("plain").c_str();

      ConfigParser config {const_cast<char*>(body)};

      if (_node.configureAll(config)) {
        _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), body);
      } else {
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), EMPTY_STRING);
      }
    });

    _server.on(UriBraces(F("/api/node/config/{}")), HTTP_GET, [this]() {
      const auto& category = _server.pathArg(0);

      if (!_buffer.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }

      _node.getConfig(category, [&] (const char* name, const char* value) {
        _buffer.plainText(name);
        _buffer.plainChar(ConfigParser::SEPARATOR);
        _buffer.plainText(value);
        _buffer.plainChar(ConfigParser::END);
        _buffer.plainChar('\n');
      });

      _buffer.end();
    });

    _server.on(UriBraces(F("/api/node/config/{}")), HTTP_PUT, [this]() {
      const auto& category = _server.pathArg(0);
      const char* body = _server.arg("plain").c_str();

      ConfigParser config {const_cast<char*>(body)};

      if (_node.configure(category, config)) {
        _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), body);
      } else {
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), EMPTY_STRING);
      }
    });
  }

  void begin() {
    _server.begin();
  }

  void end() {
    _server.close();
  }

  void loop() {
    _server.handleClient();
  }

private:
  /**
   * Do a POST operation for one or more items. The operation is executed for each item independently,
   * and only if all operations are successful is the whole operation answerd with successful status code.
   * 
   * Expects one or more value IDs with or without device ID. Multiple values are separated by commas.
   * 
   * For example:
   * 123,42@HEA/2,42@HEA/1
   * 
   * It is allowed to have spaces or other whitespace _after_ the commas.
   */
  void postItems(std::function<bool(DataAccess::DataKey const&)> itemOperation = {}) {
    _node.lyield();

    const char* body = _server.arg(FPSTR(ARG_PLAIN)).c_str();
    char* next;
    while (true) {
      ValueId valueId = strtol(body, &next, 0);
      if (next == body) {
        break;
      }
      const auto& definition = getDefinition(valueId);
      if (definition.isUnknown()) {
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("unknown ID: %u"), valueId));
        return;
      }
      
      switch (*next) {
        case '\0':
        case ',':
        {
          // plain valueId
          if (!definition.source.isExact()) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("device ID required for %u"), valueId));
            return;
          }
          if (!itemOperation({definition.source, valueId})) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("operation failed for %u"), valueId));
            return;
          }
          break;
        }
        case '@':
        {
          // <valueId>@<DeviceType>/<address>
          Maybe<DeviceId> key = DeviceId::fromString(next + 1, &next);
          if (!key.hasValue) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("device ID invalid for %u"), valueId));
            return;
          }
          if (!itemOperation({key.value, valueId})) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("operation failed for %u@%s"), valueId, key.value.toString()));
            return;
          }
          break;
        }
        default:
        {
          _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("unexpected format: %s"), next));
          return;
        }
      }
      if (*next == ',') {
        next += 1;
      }
      body = next;

      _node.lyield();
    }

    if (strlen(body) > 0) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), format(F("unrecognized value: %s"), body));
    } else {
      _server.send(204, FPSTR(CONTENT_TYPE_PLAIN), EMPTY_STRING);
    }
  }

  /**
   * Execute an operation on a specific, single item identified by a data key.
   * 
   * The endpoint _must_ provide three path arguments which correspond to:
   *  1. DeviceType
   *  2. DeviceAddress
   *  3. ValueId
   */
  void doItem(std::function<bool(DataAccess::DataKey const&, DataEntry const&)> itemOperation = {}) {
    _node.lyield();

    if (_node.logLevel("api") > 1) _node.log("api", format("doItem: %s", _server.arg(FPSTR(ARG_PLAIN)).c_str()));

    bool validateOnly = _server.hasArg(FPSTR(ARG_VALIDATE_ONLY));
    
    DeviceType type = deviceTypeFromString(_server.pathArg(0).c_str());
    if (type == DeviceType::Any) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("device type invalid"));
      return;
    }

    char* end;
    const char* addressArg = _server.pathArg(1).c_str();
    long addressNumber = strtol(addressArg, &end, 10);
    if (end == addressArg || addressNumber < 0 || addressNumber > 0x7F) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("device address invalid"));
      return;
    }

    const char* valueIdArg = _server.pathArg(2).c_str();
    long valueIdNumber = strtol(valueIdArg, &end, 0);
    if (end == valueIdArg || valueIdNumber < 0 || valueIdNumber > 0xFFFF) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("value ID invalid"));
      return;
    }

    const DataAccess::DataKey key {DeviceId{type, DeviceAddress(addressNumber)}, ValueId(valueIdNumber)};
    const DataEntry* entry = _access.getEntry(key);
    if (entry == nullptr) {
      _server.send(404, FPSTR(CONTENT_TYPE_PLAIN), F("item not found"));
      return;
    }

    _node.lyield();

    if (validateOnly) {
      _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), _server.arg(FPSTR(ARG_PLAIN)));
    } else {
      if (itemOperation(key, *entry)) {
        _server.send(202, FPSTR(CONTENT_TYPE_PLAIN), EMPTY_STRING);
      } else {
        if (_node.logLevel("api") > 0) _node.log("api", "doItem: operation failed");
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("operation failed"));
      }
    }
  }

  /**
   * Produce a list of items based on the optional predicate.
   */
  void getItems(std::function<bool(DataEntry const&)> predicate = {}) {
    DateTime updatedSince;
    if (_server.hasArg(FPSTR(ARG_UPDATED_SINCE))) {
      updatedSince.fromString(_server.arg(FPSTR(ARG_UPDATED_SINCE)).c_str());
    }
    bool numbersAsDecimals = _server.hasArg(FPSTR(ARG_NUMBERS_AS_DECIMALS));

    if (!_buffer.begin(200, FPSTR(CONTENT_TYPE_JSON))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }
    
    const auto& collectionData = _access.getData();

    _buffer.jsonObjectOpen();
    _buffer.jsonPropertyString(F("retrievedOn"), _access.getCurrentDateTime().toString());
    _buffer.jsonSeparator();
    _buffer.jsonPropertyRaw(F("totalItems"), toConstStr(collectionData.size(), 10));
    _buffer.jsonSeparator();
    _buffer.jsonPropertyStart(F("items"));
    _buffer.jsonObjectOpen();

    size_t i = 0u;
    DeviceType type;
    DeviceAddress address;
    for (auto& data : collectionData) {
      if (data.second.lastUpdate >= updatedSince && (!predicate || predicate(data.second))) {
        if (i == 0) {
          type = data.first.first.type;
          address = data.first.first.address;
          _buffer.jsonPropertyStart(deviceTypeName(type));
          _buffer.jsonObjectOpen();
          _buffer.jsonPropertyStart(toConstStr(address, 10));
          _buffer.jsonObjectOpen();
        } else {
          if (type != data.first.first.type) {
            type = data.first.first.type;
            address = data.first.first.address;
            _buffer.jsonObjectClose();
            _buffer.jsonObjectClose();
            _buffer.jsonSeparator();
            _buffer.jsonPropertyStart(deviceTypeName(type));
            _buffer.jsonObjectOpen();
            _buffer.jsonPropertyStart(toConstStr(address, 10));
            _buffer.jsonObjectOpen();            
          } else if (address != data.first.first.address) {
            address = data.first.first.address;
            _buffer.jsonObjectClose();
            _buffer.jsonSeparator();
            _buffer.jsonPropertyStart(toConstStr(address, 10));
            _buffer.jsonObjectOpen();
          } else {
            _buffer.jsonSeparator();
          }
        }

        _buffer.jsonPropertyStart(toConstStr(data.second.id, 10));
        writeItem(data.second, numbersAsDecimals);

        ++i;

        _node.lyield();
      }
    }

    if (i > 0) {
      _buffer.jsonObjectClose();
      _buffer.jsonObjectClose();
    }

    _buffer.jsonObjectClose();
    _buffer.jsonObjectClose();
    _buffer.end();
  }

  void writeItem(const DataEntry& entry, bool numbersAsDecimals = false) {
    _buffer.jsonObjectOpen();
    _buffer.jsonPropertyRaw(F("id"), toConstStr(entry.id, 10));
    _buffer.jsonSeparator();
    if (entry.hasDefinition()) {
      _buffer.jsonPropertyString(F("name"), entry.definition->name);
      _buffer.jsonSeparator();
      _buffer.jsonPropertyString(F("accessMode"), getValueAccessModeString(entry.definition->accessMode));
      _buffer.jsonSeparator();
      if (entry.definition->unit != Unit::Unknown) {
        _buffer.jsonPropertyString(F("unit"), getUnitSymbol(entry.definition->unit));
        _buffer.jsonSeparator();
      }
    }
    if (entry.lastUpdate.isSet()) {
      if (numbersAsDecimals) {
        _buffer.jsonPropertyString(F("rawValue"), getRawValueAsDecString(entry.rawValue));
      } else {
        _buffer.jsonPropertyString(F("rawValue"), getRawValueAsHexString(entry.rawValue));
      }
      _buffer.jsonSeparator();
      _buffer.jsonPropertyRaw(F("value"), entry.definition->fromRaw(entry.rawValue));
      _buffer.jsonSeparator();
    }
    _buffer.jsonPropertyString(F("lastUpdate"), entry.lastUpdate.toString());
    _buffer.jsonSeparator();
    _buffer.jsonPropertyString(F("source"), entry.source.toString());
    _buffer.jsonSeparator();
    _buffer.jsonPropertyRaw(F("subscribed"), entry.subscribed ? F("true") : F("false"));
    _buffer.jsonSeparator();
    _buffer.jsonPropertyRaw(F("writable"), entry.writable ? F("true") : F("false"));
    _buffer.jsonObjectClose();
  }

  void getDefinitions() {
    if (!_buffer.begin(200, FPSTR(CONTENT_TYPE_JSON))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }
    
    _buffer.jsonListOpen();

    bool first = true;
    for (auto& definition : DEFINITIONS) {
        if (!first) {
          _buffer.jsonSeparator();
        }
        first = false;

        _buffer.jsonObjectOpen();

        _buffer.jsonPropertyRaw(F("id"), toConstStr(definition.id, 10));
        _buffer.jsonSeparator();
        _buffer.jsonPropertyString(F("name"), definition.name);
        _buffer.jsonSeparator();
        _buffer.jsonPropertyString(F("unit"), getUnitSymbol(definition.unit));
        _buffer.jsonSeparator();
        _buffer.jsonPropertyString(F("accessMode"), getValueAccessModeString(definition.accessMode));
        _buffer.jsonSeparator();
        _buffer.jsonPropertyString(F("source"), definition.source.toString());
        
        _buffer.jsonObjectClose();

        _node.lyield();
    }

    _buffer.jsonListClose();
    _buffer.end();
  }

  void getDevices() {
    if (!_buffer.begin(200, FPSTR(CONTENT_TYPE_JSON))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }
    
    _buffer.jsonObjectOpen();
    _buffer.jsonPropertyString(F("this"), _protocol.getThisDeviceId().toString());
    _buffer.jsonSeparator();
    _buffer.jsonPropertyStart(F("others"));
    _buffer.jsonListOpen();

    bool first = true;
    for (auto& deviceId : _protocol.getOtherDeviceIds()) {
        if (!first) {
          _buffer.jsonSeparator();
        }
        first = false;

        _buffer.jsonString(deviceId.toString());
        
        _node.lyield();
    }

    _buffer.jsonListClose();
    _buffer.jsonObjectClose();
    _buffer.end();
  }
};
