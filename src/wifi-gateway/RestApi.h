#ifndef RESTAPI_H_
#define RESTAPI_H_

#include <iot_core/api/Interfaces.h>
#include <iot_core/JsonWriter.h>
#include <uri/UriBraces.h>
#include "Serializer.h"
#include "DataAccess.h"
#include "StiebelEltronProtocol.h"

#if __has_include("src/wifi-gateway-ui/dist/wifi-gateway-ui.generated.h")
#  include "src/wifi-gateway-ui/dist/wifi-gateway-ui.generated.h"
#else
#  include "no-ui.generated.h"
#endif

static const char ARG_UPDATED_SINCE[] = "updatedSince";
static const char ARG_ONLY_UNDEFINED[] = "onlyUndefined";
static const char ARG_ONLY_CONFIGURED[] = "onlyConfigured";
static const char ARG_NUMBERS_AS_DECIMALS[] = "numbersAsDecimals";
static const char ARG_ACCESS_MODE[] = "accessMode";
static const char ARG_VALIDATE_ONLY[] = "validateOnly";

class RestApi final : public iot_core::api::IProvider {
private:
  iot_core::Logger& _logger;
  iot_core::ISystem& _system;

  DataAccess& _access;
  StiebelEltronProtocol& _protocol;
  
public:
  RestApi(iot_core::ISystem& system, DataAccess& access, StiebelEltronProtocol& protocol)
  : _logger(system.logger()), _system(system), _access(access), _protocol(protocol) {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/"), iot_core::api::HttpMethod::GET, [this](const iot_core::api::IRequest&, iot_core::api::IResponse& response) {
      response
        .code(iot_core::api::ResponseCode::Ok)
        .contentType(iot_core::api::ContentType::TextHtml)
        .header("Content-Encoding", "gzip")
        .sendSingleBody()
      #ifdef WIFI_GATEWAY_UI_H
        .write(wifi_gateway_ui::HTML, wifi_gateway_ui::HTML_SIZE);
      #else
        .write(no_ui::HTML, no_ui::HTML_SIZE);
      #endif
    });

    server.on(F("/api/data"), iot_core::api::HttpMethod::GET, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      if (request.hasArg(ARG_ONLY_UNDEFINED)) {
        getItems(request, response, [] (DataEntry const& entry) { return !entry.hasDefinition(); });
      } else if (request.hasArg(ARG_ONLY_CONFIGURED)) {
        getItems(request, response, [] (DataEntry const& entry) { return entry.isConfigured(); });
      } else {
        getItems(request, response);
      }
    });

    server.on(UriBraces(F("/api/data/{}/{}/{}")), iot_core::api::HttpMethod::PUT, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      doItem(request, response, [&] (DataAccess::DataKey const& key, DataEntry const& entry) {
        if (!entry.writable || !entry.hasDefinition()) {
          return false;
        }
        
        ValueAccessMode accessMode = valueAccessModeFromString(request.arg(ARG_ACCESS_MODE));
        const char* valueString = request.body().content();

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
    });

    server.on(F("/api/subscriptions"), iot_core::api::HttpMethod::GET, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDataConfigs(request, response, [] (DataEntry const& entry) { return entry.subscribed; });
    });

    server.on(F("/api/subscriptions"), iot_core::api::HttpMethod::POST, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      postDataConfig(request, response, [&] (DataAccess::DataKey const& key) { return _access.addSubscription(key); });
    });

    server.on(UriBraces(F("/api/subscriptions/{}/{}/{}")), iot_core::api::HttpMethod::DELETE, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      doItem(request, response, [&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeSubscription(key); return true; });
    });

    server.on(F("/api/writable"), iot_core::api::HttpMethod::GET, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDataConfigs(request, response, [](DataEntry const& entry) { return entry.writable; });
    });

    server.on(F("/api/writable"), iot_core::api::HttpMethod::POST, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      postDataConfig(request, response, [&] (DataAccess::DataKey const& key) { return _access.addWritable(key); });
    });

    server.on(UriBraces(F("/api/writable/{}/{}/{}")), iot_core::api::HttpMethod::DELETE, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      doItem(request, response, [&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeWritable(key); return true; });
    });

    server.on(F("/api/definitions"), iot_core::api::HttpMethod::GET, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDefinitions(request, response);
    });

    server.on(F("/api/devices"), iot_core::api::HttpMethod::GET, [this](const iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDevices(request, response);
    });  
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
  void postDataConfig(const iot_core::api::IRequest& request, iot_core::api::IResponse& response, std::function<bool(DataAccess::DataKey const&)> itemOperation = {}) {
    _system.lyield();

    const char* body = request.body().content();
    char* next;
    while (true) {
      ValueId valueId = iot_core::convert<ValueId>::fromString(body, &next, 0);
      if (next == body) {
        break;
      }
      const auto& definition = getDefinition(valueId);
      if (definition.isUnknown()) {
        response
          .code(iot_core::api::ResponseCode::BadRequest)
          .contentType(iot_core::api::ContentType::TextPlain)
          .sendSingleBody().write(iot_core::format(F("unknown ID: %u"), valueId));
        return;
      }
      
      switch (*next) {
        case '\0':
        case ',':
        {
          // plain valueId
          if (!definition.source.isExact()) {
            response
              .code(iot_core::api::ResponseCode::BadRequest)
              .contentType(iot_core::api::ContentType::TextPlain)
              .sendSingleBody().write(iot_core::format(F("device ID required for %u"), valueId));
            return;
          }
          if (!itemOperation({definition.source, valueId})) {
            response
              .code(iot_core::api::ResponseCode::BadRequest)
              .contentType(iot_core::api::ContentType::TextPlain)
              .sendSingleBody().write(iot_core::format(F("operation failed for %u"), valueId));
            return;
          }
          break;
        }
        case '@':
        {
          // <valueId>@<DeviceType>/<address>
          iot_core::Maybe<DeviceId> key = DeviceId::fromString(next + 1, &next);
          if (!key.hasValue) {
            response
              .code(iot_core::api::ResponseCode::BadRequest)
              .contentType(iot_core::api::ContentType::TextPlain)
              .sendSingleBody().write(iot_core::format(F("device ID invalid for %u"), valueId));
            return;
          }
          if (!itemOperation({key.value, valueId})) {
            response
              .code(iot_core::api::ResponseCode::BadRequest)
              .contentType(iot_core::api::ContentType::TextPlain)
              .sendSingleBody().write(iot_core::format(F("operation failed for %u@%s"), valueId, key.value.toString()));
            return;
          }
          break;
        }
        default:
        {
          response
            .code(iot_core::api::ResponseCode::BadRequest)
            .contentType(iot_core::api::ContentType::TextPlain)
            .sendSingleBody().write(iot_core::format(F("unexpected format: %s"), next));
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
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(iot_core::format(F("unrecognized value: %s"), body));
    } else {
      response.code(iot_core::api::ResponseCode::OkNoContent);
    }
  }

  /**
   * Produce a list of data configurations based on the optional predicate.
   */
  void getDataConfigs(const iot_core::api::IRequest&, iot_core::api::IResponse& response, std::function<bool(DataEntry const&)> predicate = {}) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }
    
    const auto& collectionData = _access.getData();

    for (auto& data : collectionData) {
      if (!predicate || predicate(data.second)) {
        body.write(iot_core::format("%u@%s,\n", data.second.id, data.second.source.toString()));
        _system.lyield();
      }
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
  void doItem(const iot_core::api::IRequest& request, iot_core::api::IResponse& response, std::function<bool(DataAccess::DataKey const&, DataEntry const&)> itemOperation = {}) {
    _system.lyield();

    _logger.log(iot_core::LogLevel::Debug, "api", [&] () { return iot_core::format(F("doItem '%s/%s/%s': %s"), request.pathArg(0), request.pathArg(1), request.pathArg(2), request.body().content()); });

    bool validateOnly = request.hasArg(ARG_VALIDATE_ONLY);
    
    DeviceType type = deviceTypeFromString(request.pathArg(0));
    if (type == DeviceType::Any) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("device type invalid"));
      return;
    }

    char* end;
    const char* addressArg = request.pathArg(1);
    long addressNumber = iot_core::convert<long>::fromString(addressArg, &end, 10);
    if (end == addressArg || addressNumber < 0 || addressNumber > 0x7F) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("device address invalid"));
      return;
    }

    const char* valueIdArg = request.pathArg(2);
    long valueIdNumber = iot_core::convert<long>::fromString(valueIdArg, &end, 0);
    if (end == valueIdArg || valueIdNumber < 0 || valueIdNumber > 0xFFFF) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("value ID invalid"));
      return;
    }

    const DataAccess::DataKey key {DeviceId{type, DeviceAddress(addressNumber)}, ValueId(valueIdNumber)};
    const DataEntry* entry = _access.getEntry(key);
    if (entry == nullptr) {
      response
        .code(iot_core::api::ResponseCode::BadRequestNotFound)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("item not found"));
      return;
    }

    _system.lyield();

    if (validateOnly) {
      response
        .code(iot_core::api::ResponseCode::Ok)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(request.body().content());
    } else {
      if (itemOperation(key, *entry)) {
        response.code(iot_core::api::ResponseCode::OkAccepted);
      } else {
        _logger.log(iot_core::LogLevel::Warning, "api", F("doItem: operation failed"));
        response
          .code(iot_core::api::ResponseCode::BadRequest)
          .contentType(iot_core::api::ContentType::TextPlain)
          .sendSingleBody().write(F("operation failed"));
      }
    }
  }

  /**
   * Produce a list of items based on the optional predicate.
   */
  void getItems(const iot_core::api::IRequest& request, iot_core::api::IResponse& response, std::function<bool(DataEntry const&)> predicate = {}) {
    iot_core::DateTime updatedSince;
    if (request.hasArg(ARG_UPDATED_SINCE)) {
      updatedSince.fromString(request.arg(ARG_UPDATED_SINCE));
    }
    bool numbersAsDecimals = request.hasArg(ARG_NUMBERS_AS_DECIMALS);

    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }
    
    const auto& collectionData = _access.getData();

    auto writer = iot_core::makeJsonWriter(body);

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
  }

  void getDefinitions(const iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = iot_core::makeJsonWriter(body);

    writer.openList();
    for (auto& definition : DEFINITIONS) {
      serializer::serialize(writer, definition);
      _system.lyield();
    }
    writer.close();
  }

  void getDevices(const iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = iot_core::makeJsonWriter(body);
    
    writer.openObject();
    writer.property(F("this"));
    writer.openObject();
    for (auto& [name, device] : _protocol.getDevices()) {
      writer.propertyString(device->deviceId().toString(), device->description()),
      _system.lyield();
    }
    writer.close();
    writer.property(F("others"));
    writer.openList();
    for (auto& deviceId : _protocol.getOtherDeviceIds()) {
      writer.stringValue(deviceId.toString());
      _system.lyield();
    }
    writer.close();
    writer.close();
  }
};

#endif
