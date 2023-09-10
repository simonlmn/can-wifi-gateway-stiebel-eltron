#ifndef RESTAPI_H_
#define RESTAPI_H_

#include "src/iot-core/Interfaces.h"
#include "src/iot-core/Utils.h"
#include "src/iot-core/Config.h"
#include "src/iot-core/ChunkedResponse.h"
#include "src/iot-core/JsonWriter.h"
#include "src/iot-core/JsonDiagnosticsCollector.h"
#include <ESP8266WebServer.h>
#include <uri/UriBraces.h>
#include <uri/UriGlob.h>
#include "Serializer.h"
#include "DataAccess.h"
#include "StiebelEltronProtocol.h"

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
static const char ARG_ONLY_CONFIGURED[] PROGMEM = "onlyConfigured";
static const char ARG_NUMBERS_AS_DECIMALS[] PROGMEM = "numbersAsDecimals";
static const char ARG_ACCESS_MODE[] PROGMEM = "accessMode";
static const char ARG_VALIDATE_ONLY[] PROGMEM = "validateOnly";

class RestApi final : public iot_core::IApplicationComponent {
private:
  iot_core::Logger& _logger;
  iot_core::ISystem& _system;
  iot_core::IApplicationContainer& _application;

  DataAccess& _access;
  StiebelEltronProtocol& _protocol;

  ESP8266WebServer _server;
  iot_core::ChunkedResponse<ESP8266WebServer> _response;

  iot_core::TimingStatistics<10u> _callStatistics;
  
public:
  RestApi(iot_core::ISystem& system, iot_core::IApplicationContainer& application, DataAccess& access, StiebelEltronProtocol& protocol)
  : _logger(system.logger()), _system(system), _application(application), _access(access), _protocol(protocol), _server(80), _response(_server) {}
  
  const char* name() const override {
    return "api";
  }

  bool configure(const char* /*name*/, const char* /*value*/) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> /*writer*/) const override {
  }

  void setup(bool /*connected*/) override {
    _server.enableCORS(true);
    _server.collectHeaders(FPSTR(HEADER_ACCEPT));

    _server.on(UriGlob(F("*")), HTTP_OPTIONS, [this]() { // generic reply to make "pre-flight" checks work
      _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
      _server.send(204);
    });
    
    _server.on(F("/"), HTTP_GET, _callStatistics.wrap([this]() {
      _server.sendHeader("Content-Encoding", "gzip");
      #ifdef WIFI_GATEWAY_UI_H
        _server.send_P(200, CONTENT_TYPE_HTML, wifi_gateway_ui::HTML, wifi_gateway_ui::HTML_SIZE);
      #else
        _server.send_P(200, CONTENT_TYPE_HTML, no_ui::HTML, no_ui::HTML_SIZE);
      #endif
    }));

    _server.on(F("/api/data"), HTTP_GET, _callStatistics.wrap([this]() {
      if (_server.hasArg(FPSTR(ARG_ONLY_UNDEFINED))) {
        getItems([] (DataEntry const& entry) { return !entry.hasDefinition(); });
      } else if (_server.hasArg(FPSTR(ARG_ONLY_CONFIGURED))) {
        getItems([] (DataEntry const& entry) { return entry.isConfigured(); });
      } else {
        getItems();
      }
    }));

    _server.on(UriBraces(F("/api/data/{}/{}/{}")), HTTP_PUT, _callStatistics.wrap([this]() {
      doItem([&] (DataAccess::DataKey const& key, DataEntry const& entry) {
        if (!entry.writable || !entry.hasDefinition()) {
          return false;
        }
        
        ValueAccessMode accessMode = valueAccessModeFromString(_server.arg(FPSTR(ARG_ACCESS_MODE)).c_str());
        const char* valueString = _server.arg(FPSTR(ARG_PLAIN)).c_str();

        const char* space = strrchr(valueString, ' ');
        if (space != nullptr) {
          const char* symbol = unitSymbol(entry.definition->unit);
          if (strcmp(space + 1, symbol) != 0) {
            _logger.log(iot_core::LogLevel::Warning, "api", [&] () { return iot_core::format(F("PUT data: unit mismatch %s != %s"), space + 1, symbol); });
            return false;
          }
        }

        uint32_t rawValue = entry.definition->toRaw(valueString, space);

        if (Codec::isError(rawValue)) {
          _logger.log(iot_core::LogLevel::Warning, "api", [&] () { return iot_core::format(F("PUT data: value error %s"), valueString); });
          return false;
        }
  
        return _access.write(key, rawValue, accessMode);
      });
    }));

    _server.on(F("/api/subscriptions"), HTTP_GET, _callStatistics.wrap([this]() {
      getDataConfigs([] (DataEntry const& entry) { return entry.subscribed; });
    }));

    _server.on(F("/api/subscriptions"), HTTP_POST, _callStatistics.wrap([this]() {      
      postDataConfig([&] (DataAccess::DataKey const& key) { return _access.addSubscription(key); });
    }));

    _server.on(UriBraces(F("/api/subscriptions/{}/{}/{}")), HTTP_DELETE, _callStatistics.wrap([this]() {
      doItem([&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeSubscription(key); return true; });
    }));

    _server.on(F("/api/writable"), HTTP_GET, _callStatistics.wrap([this]() {
      getDataConfigs([](DataEntry const& entry) { return entry.writable; });
    }));

    _server.on(F("/api/writable"), HTTP_POST, _callStatistics.wrap([this]() {
      postDataConfig([&] (DataAccess::DataKey const& key) { return _access.addWritable(key); });
    }));

    _server.on(UriBraces(F("/api/writable/{}/{}/{}")), HTTP_DELETE, _callStatistics.wrap([this]() {
      doItem([&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeWritable(key); return true; });
    }));

    _server.on(F("/api/definitions"), HTTP_GET, _callStatistics.wrap([this]() {
      getDefinitions();
    }));

    _server.on(F("/api/devices"), HTTP_GET, _callStatistics.wrap([this]() {
      getDevices();
    }));
    
    _server.on(F("/api/system/reset"), HTTP_POST, [this]() {
      _system.schedule([&] () { _system.reset(); });
      _system.reset();
      _server.send(204);
    });

    _server.on(F("/api/system/factory-reset"), HTTP_POST, [this]() {
      _system.schedule([&] () { _system.factoryReset(); });
      _server.send(204);
    });

    _server.on(F("/api/system/stop"), HTTP_POST, [this]() {
      _system.stop();
      _server.send(204);
    });

    _server.on(F("/api/system/status"), HTTP_GET, [this]() {
      if (!_response.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }
      
      {
        auto writer = iot_core::makeJsonWriter(_response);
        auto collector = iot_core::makeJsonDiagnosticsCollector(writer);
        _application.getDiagnostics(collector);
      }

      _response.end();
    });

    _server.on(F("/api/system/logs"), HTTP_GET, [this]() {
      if (!_response.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }
      
      _logger.output([&] (const char* entry) {
        _response.write(entry);
      });

      _response.end();
    });

    _server.on(F("/api/system/log-level"), HTTP_GET, [this]() {
      if (!_response.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }

      _response.write(iot_core::logLevelToString(_logger.initialLogLevel()));
      _response.write('\n');

      for (auto entry : _logger.logLevels()) {
        _response.write(entry.first);
        _response.write('=');
        _response.write(iot_core::logLevelToString(entry.second));
        _response.write('\n');
      }

      _response.end();
    });

    _server.on(F("/api/system/log-level"), HTTP_PUT, [this]() {
      iot_core::LogLevel logLevel = iot_core::logLevelFromString(_server.arg("plain").c_str());

      _logger.initialLogLevel(logLevel);
      
      _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), iot_core::logLevelToString(_logger.initialLogLevel()));
    });

    _server.on(UriBraces(F("/api/system/log-level/{}")), HTTP_PUT, [this]() {
      const auto& category = _server.pathArg(0);
      iot_core::LogLevel logLevel = iot_core::logLevelFromString(_server.arg("plain").c_str());

      _logger.logLevel(iot_core::make_static(category.c_str()), logLevel);
      
      _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), iot_core::logLevelToString(_logger.logLevel(category.c_str())));
    });

    _server.on(F("/api/system/config"), HTTP_GET, [this]() {
      if (!_response.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }

      _application.getAllConfig([&] (const char* path, const char* value) {
        _response.write(path);
        _response.write(iot_core::ConfigParser::SEPARATOR);
        _response.write(value);
        _response.write(iot_core::ConfigParser::END);
        _response.write('\n');
      });

      _response.end();
    });

    _server.on(F("/api/system/config"), HTTP_PUT, [this]() {
      const char* body = _server.arg("plain").c_str();

      iot_core::ConfigParser config {const_cast<char*>(body)};

      if (_application.configureAll(config)) {
        _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), body);
      } else {
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::EMPTY_STRING);
      }
    });

    _server.on(UriBraces(F("/api/system/config/{}")), HTTP_GET, [this]() {
      const auto& category = _server.pathArg(0);

      if (!_response.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
        _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
        return;
      }

      _application.getConfig(category.c_str(), [&] (const char* name, const char* value) {
        _response.write(name);
        _response.write(iot_core::ConfigParser::SEPARATOR);
        _response.write(value);
        _response.write(iot_core::ConfigParser::END);
        _response.write('\n');
      });

      _response.end();
    });

    _server.on(UriBraces(F("/api/system/config/{}")), HTTP_PUT, [this]() {
      const auto& category = _server.pathArg(0);
      const char* body = _server.arg("plain").c_str();

      iot_core::ConfigParser config {const_cast<char*>(body)};

      if (_application.configure(category.c_str(), config)) {
        _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), body);
      } else {
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::EMPTY_STRING);
      }
    });
  }

  void loop(iot_core::ConnectionStatus status) override {
    switch (status) {
      case iot_core::ConnectionStatus::Reconnected:
        _server.begin();
        break;
      case iot_core::ConnectionStatus::Connected:
        _server.handleClient();
        break;
      case iot_core::ConnectionStatus::Disconnecting:
        _server.close();
        break;
      case iot_core::ConnectionStatus::Disconnected:
        // do nothing
        break;
    }
  }

  void getDiagnostics(iot_core::IDiagnosticsCollector& collector) const override {
    collector.addValue("callCount", iot_core::convert<size_t>::toString(_callStatistics.count(), 10));
    collector.addValue("callAvg", iot_core::convert<unsigned long>::toString(_callStatistics.avg(), 10));
    collector.addValue("callMin", iot_core::convert<unsigned long>::toString(_callStatistics.min(), 10));
    collector.addValue("callMax", iot_core::convert<unsigned long>::toString(_callStatistics.max(), 10));
  }

private:
  /**
   * Do a POST operation for one or more data configurations. The operation is executed for each item
   * independently, and only if all operations are successful is the whole operation answered with
   * successful status code.
   * 
   * Expects one or more value IDs with or without device ID. Multiple values are separated by commas.
   * 
   * For example:
   * 123,42@HEA/2,42@HEA/1
   * 
   * It is allowed to have spaces or other whitespace _after_ the commas.
   */
  void postDataConfig(std::function<bool(DataAccess::DataKey const&)> itemOperation = {}) {
    _system.lyield();

    const char* body = _server.arg(FPSTR(ARG_PLAIN)).c_str();
    char* next;
    while (true) {
      ValueId valueId = iot_core::convert<ValueId>::fromString(body, &next, 0);
      if (next == body) {
        break;
      }
      const auto& definition = getDefinition(valueId);
      if (definition.isUnknown()) {
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("unknown ID: %u"), valueId));
        return;
      }
      
      switch (*next) {
        case '\0':
        case ',':
        {
          // plain valueId
          if (!definition.source.isExact()) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("device ID required for %u"), valueId));
            return;
          }
          if (!itemOperation({definition.source, valueId})) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("operation failed for %u"), valueId));
            return;
          }
          break;
        }
        case '@':
        {
          // <valueId>@<DeviceType>/<address>
          iot_core::Maybe<DeviceId> key = DeviceId::fromString(next + 1, &next);
          if (!key.hasValue) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("device ID invalid for %u"), valueId));
            return;
          }
          if (!itemOperation({key.value, valueId})) {
            _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("operation failed for %u@%s"), valueId, key.value.toString()));
            return;
          }
          break;
        }
        default:
        {
          _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("unexpected format: %s"), next));
          return;
        }
      }
      if (*next == ',') {
        next += 1;
      }
      body = next;

      _system.lyield();
    }

    if (strlen(body) > 0) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), iot_core::format(F("unrecognized value: %s"), body));
    } else {
      _server.send(204, FPSTR(CONTENT_TYPE_PLAIN), iot_core::EMPTY_STRING);
    }
  }

  /**
   * Produce a list of data configurations based on the optional predicate.
   */
  void getDataConfigs(std::function<bool(DataEntry const&)> predicate = {}) {
    if (!_response.begin(200, FPSTR(CONTENT_TYPE_PLAIN))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }
    
    const auto& collectionData = _access.getData();

    for (auto& data : collectionData) {
      if (!predicate || predicate(data.second)) {
        _response.write(iot_core::format("%u@%s,\n", data.second.id, data.second.source.toString()));
        _system.lyield();
      }
    }

    _response.end();
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
    _system.lyield();

    _logger.log(iot_core::LogLevel::Debug, "api", [&] () { return iot_core::format(F("doItem '%s': %s"), _server.pathArg(0).c_str(), _server.arg(FPSTR(ARG_PLAIN)).c_str()); });

    bool validateOnly = _server.hasArg(FPSTR(ARG_VALIDATE_ONLY));
    
    DeviceType type = deviceTypeFromString(_server.pathArg(0).c_str());
    if (type == DeviceType::Any) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("device type invalid"));
      return;
    }

    char* end;
    const char* addressArg = _server.pathArg(1).c_str();
    long addressNumber = iot_core::convert<long>::fromString(addressArg, &end, 10);
    if (end == addressArg || addressNumber < 0 || addressNumber > 0x7F) {
      _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("device address invalid"));
      return;
    }

    const char* valueIdArg = _server.pathArg(2).c_str();
    long valueIdNumber = iot_core::convert<long>::fromString(valueIdArg, &end, 0);
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

    _system.lyield();

    if (validateOnly) {
      _server.send(200, FPSTR(CONTENT_TYPE_PLAIN), _server.arg(FPSTR(ARG_PLAIN)));
    } else {
      if (itemOperation(key, *entry)) {
        _server.send(202, FPSTR(CONTENT_TYPE_PLAIN), iot_core::EMPTY_STRING);
      } else {
        _logger.log(iot_core::LogLevel::Warning, name(), F("doItem: operation failed"));
        _server.send(400, FPSTR(CONTENT_TYPE_PLAIN), F("operation failed"));
      }
    }
  }

  /**
   * Produce a list of items based on the optional predicate.
   */
  void getItems(std::function<bool(DataEntry const&)> predicate = {}) {
    iot_core::DateTime updatedSince;
    if (_server.hasArg(FPSTR(ARG_UPDATED_SINCE))) {
      updatedSince.fromString(_server.arg(FPSTR(ARG_UPDATED_SINCE)).c_str());
    }
    bool numbersAsDecimals = _server.hasArg(FPSTR(ARG_NUMBERS_AS_DECIMALS));

    if (!_response.begin(200, FPSTR(CONTENT_TYPE_JSON))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }
    
    const auto& collectionData = _access.getData();

    auto writer = iot_core::makeJsonWriter(_response);

    writer.openObject();
    writer.propertyString(F("retrievedOn"), _access.currentDateTime().toString());
    writer.propertyPlain(F("totalItems"), iot_core::convert<size_t>::toString(collectionData.size(), 10));
    writer.property(F("items"));
    writer.openObject();

    size_t i = 0u;
    DeviceType type;
    DeviceAddress address;
    for (auto& data : collectionData) {
      if (data.second.lastUpdate >= updatedSince && (!predicate || predicate(data.second))) {
        if (i == 0) {
          type = data.first.first.type;
          address = data.first.first.address;
          writer.property(deviceTypeToString(type));
          writer.openObject();
          writer.property(iot_core::convert<DeviceAddress>::toString(address, 10));
          writer.openObject();
        } else {
          if (type != data.first.first.type) {
            writer.close();
            writer.close();
            type = data.first.first.type;
            address = data.first.first.address;
            writer.property(deviceTypeToString(type));
            writer.openObject();
            writer.property(iot_core::convert<DeviceAddress>::toString(address, 10));
            writer.openObject();            
          } else if (address != data.first.first.address) {
            writer.close();
            address = data.first.first.address;
            writer.property(iot_core::convert<DeviceAddress>::toString(address, 10));
          }
        }

        writer.property(iot_core::convert<ValueId>::toString(data.second.id, 10));
        serializer::serialize(writer, data.second, false, numbersAsDecimals);

        ++i;

        _system.lyield();
      }
    }

    if (i > 0) {
      writer.close();
      writer.close();
    }

    writer.close();
    writer.propertyPlain(F("actualItems"), iot_core::convert<size_t>::toString(i, 10));
    writer.close();
    
    _response.end();
  }

  void getDefinitions() {
    if (!_response.begin(200, FPSTR(CONTENT_TYPE_JSON))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }
    
    auto writer = iot_core::makeJsonWriter(_response);

    writer.openList();
    for (auto& definition : DEFINITIONS) {
      serializer::serialize(writer, definition);
      _system.lyield();
    }
    writer.close();
    
    _response.end();
  }

  void getDevices() {
    if (!_response.begin(200, FPSTR(CONTENT_TYPE_JSON))) {
      _server.send(505, FPSTR(CONTENT_TYPE_PLAIN), F("HTTP1.1 required"));
      return;
    }

    auto writer = iot_core::makeJsonWriter(_response);
    
    writer.openObject();
    writer.propertyString(F("this"), _protocol.getThisDeviceId().toString());
    writer.property(F("others"));
    writer.openList();
    for (auto& deviceId : _protocol.getOtherDeviceIds()) {
      writer.stringValue(deviceId.toString());
      _system.lyield();
    }
    writer.close();
    writer.close();

    _response.end();
  }
};

#endif
