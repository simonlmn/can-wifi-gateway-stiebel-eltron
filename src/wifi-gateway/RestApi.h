#ifndef RESTAPI_H_
#define RESTAPI_H_

#include <iot_core/api/Interfaces.h>
#include <uri/UriBraces.h>
#include <jsons/Writer.h>
#include <toolbox/Repository.h>
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
  iot_core::Logger _logger;
  iot_core::ISystem& _system;

  DataAccess& _access;
  StiebelEltronProtocol& _protocol;
  IConversionService& _conversionService;
  IConversionRepository& _conversions;
  IDefinitionRepository& _definitions;

public:
  RestApi(iot_core::ISystem& system, DataAccess& access, StiebelEltronProtocol& protocol, IConversionService& conversionService, IConversionRepository& conversions, IDefinitionRepository& definitions)
  : _logger(system.logger("api")), _system(system), _access(access), _protocol(protocol), _conversionService(conversionService), _conversions(conversions), _definitions(definitions) {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest&, iot_core::api::IResponse& response) {
      response
        .code(iot_core::api::ResponseCode::Ok)
        .contentType(iot_core::api::ContentType::TextHtml)
        .header("Content-Encoding", "gzip")
        .sendSingleBody()
      #ifdef WIFI_GATEWAY_UI_H
        .write(toolbox::strref{FPSTR(wifi_gateway_ui::HTML), wifi_gateway_ui::HTML_SIZE});
      #else
        .write(toolbox::strref{FPSTR(no_ui::HTML), no_ui::HTML_SIZE});
      #endif
    });

    server.on(F("/api/data"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      if (request.hasArg(ARG_ONLY_UNDEFINED)) {
        getItems(request, response, [this] (DataEntry const& entry) { return !_definitions.get(entry.id).isUndefined(); });
      } else if (request.hasArg(ARG_ONLY_CONFIGURED)) {
        getItems(request, response, [] (DataEntry const& entry) { return entry.isConfigured(); });
      } else {
        getItems(request, response);
      }
    });

    server.on(UriBraces(F("/api/data/{}/{}/{}")), iot_core::api::HttpMethod::PUT, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      doItem(request, response, [&] (DataAccess::DataKey const& key, DataEntry const& entry) {
        ValueAccessMode accessMode = valueAccessModeFromString(request.arg(ARG_ACCESS_MODE));

        auto reader = jsons::makeReader(request.body());
        auto json = reader.begin();
        auto rawValue = _conversionService.fromJson(json, key.second);
        reader.end();
        if (reader.failed()) {
          _logger.log(iot_core::LogLevel::Warning, [&] () { return toolbox::format(F("PUT data: JSON error %s"), reader.diagnostics().errorMessage.cstr()); });
          return false;
        }

        if (rawValue) {
          return _access.write(key, rawValue.get(), accessMode);
        } else {
          _logger.log(iot_core::LogLevel::Warning, [&] () { return toolbox::format(F("PUT data: value error %s"), request.body().content().cstr()); });
          return false;
        }
      });
    });

    server.on(F("/api/subscriptions"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDataConfigs(request, response, [] (DataEntry const& entry) { return entry.subscribed; });
    });

    server.on(F("/api/subscriptions"), iot_core::api::HttpMethod::POST, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      postDataConfig(request, response, [&] (DataAccess::DataKey const& key) { return _access.addSubscription(key); });
    });

    server.on(UriBraces(F("/api/subscriptions/{}/{}/{}")), iot_core::api::HttpMethod::DELETE, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      doItem(request, response, [&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeSubscription(key); return true; });
    });

    server.on(F("/api/writable"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDataConfigs(request, response, [](DataEntry const& entry) { return entry.writable; });
    });

    server.on(F("/api/writable"), iot_core::api::HttpMethod::POST, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      postDataConfig(request, response, [&] (DataAccess::DataKey const& key) { return _access.addWritable(key); });
    });

    server.on(UriBraces(F("/api/writable/{}/{}/{}")), iot_core::api::HttpMethod::DELETE, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      doItem(request, response, [&] (DataAccess::DataKey const& key, DataEntry const& entry) { _access.removeWritable(key); return true; });
    });

    server.on(F("/api/definitions"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDefinitions(request, response);
    });

    server.on(UriBraces(F("/api/definitions")), iot_core::api::HttpMethod::PUT, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      putDefinitions(request, response);
    });

    server.on(UriBraces(F("/api/definitions")), iot_core::api::HttpMethod::DELETE, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      deleteDefinitions(request, response);
    });

    server.on(UriBraces(F("/api/definitions/{}")), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDefinition(request, response);
    });

    server.on(UriBraces(F("/api/definitions/{}")), iot_core::api::HttpMethod::PUT, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      putDefinition(request, response);
    });

    server.on(UriBraces(F("/api/definitions/{}")), iot_core::api::HttpMethod::DELETE, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      deleteDefinition(request, response);
    });

    server.on(F("/api/devices"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDevices(request, response);
    });  
  }

private:
  /**
   * Do a POST operation for one or more data configurations. The operation is executed for each item
   * independently, and only if all operations are successful is the whole operation answered with
   * successful status code.
   * 
   * Expects a list of DataKeys.
   * 
   * For example:
   * [ {"valueId": 123, "source": "SYS/0"}, {"valueId": 42, "source": "HEA/2"} ]
   */
  void postDataConfig(iot_core::api::IRequest& request, iot_core::api::IResponse& response, std::function<bool(DataAccess::DataKey const&)> itemOperation = {}) {
    auto reader = jsons::makeReader(request.body());
    auto json = reader.begin();
    for (auto& value : json.asList()) {
      auto config = value.asObject();
      if (config.valid()) {
        ValueId valueId;
        DeviceId source;
        for (auto& property : config) {
          if (property.name() == "valueId" && property.type() == jsons::ValueType::Integer) {
            valueId = property.asInteger().get();
          } else if (property.name() == "source" && property.type() == jsons::ValueType::String) {
            auto deviceId = DeviceId::fromString(property.asString().get());
            if (deviceId) {
              source = deviceId.get();
            } else {
              response.code(iot_core::api::ResponseCode::BadRequest)
                .contentType(iot_core::api::ContentType::TextPlain)
                .sendSingleBody().write(toolbox::format(F("Value error: invalid source '%s'"), property.asString().get().cstr()));
              return;
            }
          } else {
            response.code(iot_core::api::ResponseCode::BadRequest)
              .contentType(iot_core::api::ContentType::TextPlain)
              .sendSingleBody().write(toolbox::format(F("JSON error: unexpected property %s"), property.name().cstr()));
            return;
          }
        }
        if (!itemOperation({source, valueId})) {
          response
            .code(iot_core::api::ResponseCode::BadRequest)
            .contentType(iot_core::api::ContentType::TextPlain)
            .sendSingleBody().write(toolbox::format(F("Operation failed for %u@%s"), valueId, source.toString()));
          return;
        }
      } else {
        response.code(iot_core::api::ResponseCode::BadRequest); // TODO error message
        return;
      }
    }
    reader.end();

    if (reader.failed()) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("JSON error: %s"), reader.diagnostics().errorMessage.cstr()));
    } else {
      response.code(iot_core::api::ResponseCode::OkNoContent);
    }
  }

  /**
   * Produce a list of data configurations based on the optional predicate.
   */
  void getDataConfigs(iot_core::api::IRequest&, iot_core::api::IResponse& response, std::function<bool(DataEntry const&)> predicate = {}) {
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
        body.write(toolbox::format("%u@%s,\n", data.second.id, data.second.source.toString()));
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
  void doItem(iot_core::api::IRequest& request, iot_core::api::IResponse& response, std::function<bool(DataAccess::DataKey const&, DataEntry const&)> itemOperation = {}) {
    _system.lyield();

    _logger.log(iot_core::LogLevel::Debug, [&] () { return toolbox::format(F("doItem '%s/%s/%s': %s"), request.pathArg(0), request.pathArg(1), request.pathArg(2), request.body().content()); });

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
    toolbox::strref addressArg = request.pathArg(1);
    long addressNumber = iot_core::convert<long>::fromString(addressArg.cstr(), &end, 10);
    if (end == addressArg.cstr() || addressNumber < 0 || addressNumber > 0x7F) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("device address invalid"));
      return;
    }

    toolbox::strref valueIdArg = request.pathArg(2);
    long valueIdNumber = iot_core::convert<long>::fromString(valueIdArg.cstr(), &end, 0);
    if (end == valueIdArg.cstr() || valueIdNumber < 0 || valueIdNumber > 0xFFFF) {
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
        _logger.log(iot_core::LogLevel::Warning, F("doItem: operation failed"));
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
  void getItems(iot_core::api::IRequest& request, iot_core::api::IResponse& response, std::function<bool(DataEntry const&)> predicate = {}) {
    iot_core::DateTime updatedSince;
    if (request.hasArg(ARG_UPDATED_SINCE)) {
      updatedSince.fromString(request.arg(ARG_UPDATED_SINCE).cstr());
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

    auto writer = jsons::makeWriter(body);

    writer.openObject();
    writer.property(F("retrievedOn")).string(_access.currentDateTime().toString());
    writer.property(F("totalItems")).number(collectionData.size());
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
        serializer::serialize(writer, _conversionService, _definitions, data.second, false, numbersAsDecimals);

        ++i;

        _system.lyield();
      }
    }

    if (i > 0) {
      writer.close();
      writer.close();
    }

    writer.close();
    writer.property(F("actualItems")).number(i);
    writer.close();

    writer.end();
  }

  void getDefinitions(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);

    writer.openObject();
    for (auto& mapping : _definitions.all()) {
      writer.property(iot_core::convert<long>::toString(mapping.key, 10));
      mapping.value.serialize(writer, _conversions);
    }
    writer.close();

    writer.end();
  }

  void putDefinitions(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    auto transaction = toolbox::beginTransaction(_definitions);

    auto reader = jsons::makeReader(request.body());
    auto json = reader.begin();
    for (auto& property : json.asObject()) {
      ValueId valueId = iot_core::convert<ValueId>::fromString(property.name().cstr(), nullptr, 10); // TODO validation
      ValueDefinition definition {};
      if (!definition.deserialize(property, _conversions) || reader.failed()) {
        response
          .code(iot_core::api::ResponseCode::BadRequest)
          .contentType(iot_core::api::ContentType::TextPlain)
          .sendSingleBody().write(toolbox::format(F("JSON error: %s"), reader.diagnostics().errorMessage.cstr()));
        return;
      }
      
      if (!_definitions.store(valueId, definition)) {
        response.code(iot_core::api::ResponseCode::InsufficientStorage);
        return;
      }
    }

    if (reader.end().failed()) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("JSON error: %s"), reader.diagnostics().errorMessage.cstr()));
      return;
    }

    transaction.commit();

    getDefinitions(request, response);
  }

  void deleteDefinitions(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    _definitions.removeAll();
    _definitions.commit();
    response.code(iot_core::api::ResponseCode::OkNoContent);
  }

  void getDefinition(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    ValueId valueId = iot_core::convert<ValueId>::fromString(request.pathArg(0).cstr(), nullptr, 10); // TODO validation

    auto& definition = _definitions.get(valueId);

    if (definition.isUndefined()) {
      response.code(iot_core::api::ResponseCode::BadRequestNotFound);
      return;
    }

    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);
    definition.serialize(writer, _conversions);
    writer.end();
  }

  void putDefinition(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    auto transaction = toolbox::beginTransaction(_definitions);

    ValueId valueId = iot_core::convert<ValueId>::fromString(request.pathArg(0).cstr(), nullptr, 10); // TODO validation
    ValueDefinition definition {};
    
    auto reader = jsons::makeReader(request.body());
    auto json = reader.begin();
    if (!definition.deserialize(json, _conversions) || reader.end().failed()) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("JSON error: %s"), reader.diagnostics().errorMessage.cstr()));
      return;
    }
    
    if (!_definitions.store(valueId, definition)) {
      response.code(iot_core::api::ResponseCode::InsufficientStorage);
      return;
    }

    transaction.commit();

    getDefinition(request, response);
  }

  void deleteDefinition(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    ValueId valueId = iot_core::convert<ValueId>::fromString(request.pathArg(0).cstr(), nullptr, 10); // TODO validation
    _definitions.remove(valueId);
    _definitions.commit();
    response.code(iot_core::api::ResponseCode::OkNoContent);
  }

  void getDevices(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);
    
    writer.openObject();
    writer.property(F("this"));
    writer.openObject();
    for (auto& [name, device] : _protocol.getDevices()) {
      writer.property(device->deviceId().toString()).string(device->description());
    }
    writer.close();
    writer.property(F("others"));
    writer.openList();
    for (auto& deviceId : _protocol.getOtherDeviceIds()) {
      writer.string(deviceId.toString());
    }
    writer.close();
    writer.close();

    writer.end();
  }
};

#endif
