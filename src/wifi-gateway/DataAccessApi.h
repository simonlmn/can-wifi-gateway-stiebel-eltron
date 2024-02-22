#ifndef DATAACCESSAPI_H_
#define DATAACCESSAPI_H_

#include <iot_core/api/Interfaces.h>
#include <uri/UriBraces.h>
#include <jsons/Writer.h>
#include <toolbox/Repository.h>
#include "Serializer.h"
#include "DataAccess.h"
#include "ValueConversion.h"

static const char ARG_UPDATED_SINCE[] = "updatedSince";
static const char ARG_ITEM_FILTER[] = "filter";
static const char ARG_ITEM_FILTER_CONFIGURED[] = "configured";
static const char ARG_ITEM_FILTER_NOT_CONFIGURED[] = "notConfigured";
static const char ARG_NUMBERS_AS_DECIMALS[] = "numbersAsDecimals";
static const char ARG_ACCESS_MODE[] = "accessMode";
static const char ARG_VALIDATE_ONLY[] = "validateOnly";

class DataConfig final {
  ValueId _valueId {};
  DeviceId _source {};
  bool _subscribed {false};
  bool _writable {false};

public:
  DataConfig() {}
  DataConfig(const DataEntry& entry) : DataConfig(entry.id, entry.source, entry.subscribed, entry.writable) {}
  DataConfig(ValueId valueId, DeviceId source, bool subscribed, bool writable) :
    _valueId(valueId),
    _source(source),
    _subscribed(subscribed),
    _writable(writable)
  {}

  const ValueId& valueId() const { return _valueId; }
  const DeviceId& source() const { return _source; }
  bool subscribed() const { return _subscribed; }
  bool writable() const { return _writable; }

  void serialize(jsons::IWriter& output) const {
    output.openObject();
    output.property(F("valueId")).number(_valueId);
    output.property(F("source")).string(_source.toString());
    output.property(F("subscribed")).boolean(_subscribed);
    output.property(F("writable")).boolean(_writable);
    output.close();
  }

  bool deserialize(jsons::Value& input) {
    auto object = input.asObject();
    if (object.valid()) {
      for (auto& property : object) {
        if (property.name() == "valueId" && property.type() == jsons::ValueType::Integer) {
          _valueId = property.asInteger().get();
        } else if (property.name() == "source" && property.type() == jsons::ValueType::String) {
          auto deviceId = DeviceId::fromString(property.asString().get());
          if (deviceId) {
            _source = deviceId.get();
          } else {
            return false;
          }
        } else if (property.name() == "subscribed" && property.type() == jsons::ValueType::Boolean) {
          _subscribed = property.asBoolean().get();
        } else if (property.name() == "writable" && property.type() == jsons::ValueType::Boolean) {
          _writable = property.asBoolean().get();
        } else {
          return false;
        }
      }
      return true;
    } else {
      return false;
    }
  }
};

class DataAccessApi final : public iot_core::api::IProvider {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;

  DataAccess& _access;
  IConversionService& _conversionService;
  IDefinitionRepository& _definitions;

public:
  DataAccessApi(iot_core::ISystem& system, DataAccess& access, IConversionService& conversionService, IDefinitionRepository& definitions)
  : _logger(system.logger("api")), _system(system), _access(access), _conversionService(conversionService), _definitions(definitions) {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/api/data"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      if (request.hasArg(ARG_ITEM_FILTER)) {
        if (request.arg(ARG_ITEM_FILTER) == ARG_ITEM_FILTER_CONFIGURED) {
          getItems(request, response, [] (DataEntry const& entry) { return entry.isConfigured(); });
        } else if (request.arg(ARG_ITEM_FILTER) == ARG_ITEM_FILTER_NOT_CONFIGURED) {
          getItems(request, response, [] (DataEntry const& entry) { return !entry.isConfigured(); });
        } else {
          getItems(request, response);
        }
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

    server.on(F("/api/data/config"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDataConfigs(request, response);
    });

    server.on(F("/api/data/config"), iot_core::api::HttpMethod::POST, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      postDataConfigs(request, response);
    });

    server.on(UriBraces(F("/api/data/config/{}/{}/{}")), iot_core::api::HttpMethod::PUT, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      putDataConfig(request, response);
    });

    server.on(UriBraces(F("/api/data/config/{}/{}/{}")), iot_core::api::HttpMethod::DELETE, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      deleteDataConfig(request, response);
    });
  }

private:
  
  /**
   * Do a POST operation for one or more data configurations. The operation is executed for each item
   * independently, and only if all operations are successful is the whole operation answered with
   * successful status code.
   * 
   * Expects a list of DataConfig objects.
   * 
   * For example:
   * [ {"valueId": 123, "source": "SYS/0", "subscribed": true}, {"valueId": 42, "source": "HEA/2", "writable": true } ]
   */
  void postDataConfigs(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    auto reader = jsons::makeReader(request.body());
    auto json = reader.begin();
    for (auto& value : json.asList()) {
      DataConfig config;
      if (config.deserialize(value)) {
        if (config.subscribed()) {
          _access.addSubscription({config.source(), config.valueId()});
        } else {
          _access.removeSubscription({config.source(), config.valueId()});
        }
        if (config.writable()) {
          _access.addWritable({config.source(), config.valueId()});
        } else {
          _access.removeWritable({config.source(), config.valueId()});
        }
      } else {
        response.code(iot_core::api::ResponseCode::BadRequest)
          .contentType(iot_core::api::ContentType::TextPlain)
          .sendSingleBody().write(F("Failed to parse data config.")); // TODO improve error reporting
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
  void getDataConfigs(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }
    
    const auto& collectionData = _access.getData();

    auto writer = jsons::makeWriter(body);

    writer.openList();
    for (auto& data : collectionData) {
      DataConfig{data.second}.serialize(writer);
    }
    writer.close();

    writer.end();    
  }

  void putDataConfig(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
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

    DataConfig config;
    auto reader = jsons::makeReader(request.body());
    auto json = reader.begin();
    if (!config.deserialize(json)) {
      response.code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("Failed to parse data config.")); // TODO improve error reporting
      return;
    }
    reader.end();

    if (reader.failed()) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("JSON error: %s"), reader.diagnostics().errorMessage.cstr()));
    }

    if (config.source() != key.first) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("Source device ID %s must match source %s from path."), config.source().toString(0), key.first.toString(1)));
      return;
    }

    if (config.valueId() != key.second) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("Value ID %u must match ID %u from path."), config.valueId(), key.second));
      return;
    }

    if (validateOnly) {
      response
        .code(iot_core::api::ResponseCode::Ok)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(request.body().content());
    } else {
      if (config.subscribed()) {
        _access.addSubscription(key);
      } else {
        _access.removeSubscription(key);
      }
      if (config.writable()) {
        _access.addWritable(key);
      } else {
        _access.removeWritable(key);
      }
      response.code(iot_core::api::ResponseCode::Ok);
    }
  }

  void deleteDataConfig(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
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
    
    _access.removeSubscription(key);
    _access.removeWritable(key);

    response.code(iot_core::api::ResponseCode::OkNoContent);
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
          writer.property(deviceTypeToString(type)).openObject();
          writer.property(iot_core::convert<DeviceAddress>::toString(address, 10)).openObject();
        } else {
          if (type != data.first.first.type) {
            writer.close();
            writer.close();
            type = data.first.first.type;
            address = data.first.first.address;
            writer.property(deviceTypeToString(type)).openObject();
            writer.property(iot_core::convert<DeviceAddress>::toString(address, 10)).openObject();
          } else if (address != data.first.first.address) {
            writer.close();
            address = data.first.first.address;
            writer.property(iot_core::convert<DeviceAddress>::toString(address, 10)).openObject();
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
};

#endif
