#ifndef DEFINITIONSAPI_H_
#define DEFINITIONSAPI_H_

#include <iot_core/api/Interfaces.h>
#include <iot_core/System.h>
#include <uri/UriBraces.h>
#include <jsons/Writer.h>
#include <toolbox/Repository.h>
#include "ValueDefinitions.h"

class DefinitionsApi final : public iot_core::api::IProvider {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;

  IConversionRepository& _conversions;
  IDefinitionRepository& _definitions;

public:
  DefinitionsApi(iot_core::ISystem& system, IConversionRepository& conversions, IDefinitionRepository& definitions)
  : _logger(system.logger("api")), _system(system), _conversions(conversions), _definitions(definitions) {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/api/units"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getUnits(request, response);
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
  }

private:
  void getUnits(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);

    auto writeUnit = [&writer](Unit unit) {
      writer.openObject();
      writer.property(F("name")).string(unitToString(unit));
      writer.property(F("symbol")).string(unitSymbol(unit));
      writer.close();
    };

    writer.openList();
    writeUnit(Unit::Unknown);
    writeUnit(Unit::None);
    writeUnit(Unit::Percent);
    writeUnit(Unit::RelativeHumidity);
    writeUnit(Unit::Hertz);
    writeUnit(Unit::Celsius);
    writeUnit(Unit::Kelvin);
    writeUnit(Unit::Bar);
    writeUnit(Unit::KelvinPerMinute);
    writeUnit(Unit::LiterPerMinute);
    writeUnit(Unit::CubicmeterPerHour);
    writeUnit(Unit::Years);
    writeUnit(Unit::Year);
    writeUnit(Unit::Months);
    writeUnit(Unit::Month);
    writeUnit(Unit::Days);
    writeUnit(Unit::Day);
    writeUnit(Unit::Weekday);
    writeUnit(Unit::Hours);
    writeUnit(Unit::ThousandHours);
    writeUnit(Unit::Hour);
    writeUnit(Unit::Minutes);
    writeUnit(Unit::Minute);
    writeUnit(Unit::Seconds);
    writeUnit(Unit::Second);
    writeUnit(Unit::Watt);
    writeUnit(Unit::KiloWatt);
    writeUnit(Unit::WattHour);
    writeUnit(Unit::KiloWattHour);
    writeUnit(Unit::MegaWattHour);
    writeUnit(Unit::Ampere);
    writeUnit(Unit::Volt);
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
      if (!mapping.value().isUndefined()) {
        writer.property(iot_core::convert<long>::toString(mapping.key(), 10));
        mapping.value().serialize(writer, _conversions);
      }
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
};

#endif
