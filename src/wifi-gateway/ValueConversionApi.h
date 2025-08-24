#ifndef CONVERSIONAPI_H_
#define CONVERSIONAPI_H_

#include <iot_core/api/Interfaces.h>
#include <iot_core/System.h>
#include <uri/UriBraces.h>
#include <jsons/Writer.h>
#include <toolbox/Repository.h>
#include "ValueConversion.h"

class ConversionApi final : public iot_core::api::IProvider {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;

  IConversionRepository& _conversions;
  ICustomConverterRepository& _customConverters;

public:
  ConversionApi(iot_core::ISystem& system, IConversionRepository& conversions, ICustomConverterRepository& customConverters)
  : _logger(system.logger("api")), _system(system), _conversions(conversions), _customConverters(customConverters) {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/api/codecs"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getCodecs(request, response);
    });

    server.on(F("/api/converters"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getConverters(request, response);
    });

    server.on(F("/api/converters/custom"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getCustomConverters(request, response);
    });

    server.on(UriBraces(F("/api/converters/custom/{}")), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getCustomConverter(request, response);
    });

    server.on(UriBraces(F("/api/converters/custom/{}")), iot_core::api::HttpMethod::PUT, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      putCustomConverter(request, response);
    });

    server.on(UriBraces(F("/api/converters/custom/{}")), iot_core::api::HttpMethod::DELETE, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      deleteCustomConverter(request, response);
    });
  }

private:
  void getCodecs(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);

    writer.openList();
    for (auto& codec : _conversions.codecs()) {
      writer.openObject();
      writer.property(F("id")).number(_conversions.getCodecIdByKey(codec->key()));
      writer.property(F("key")).string(codec->key());
      writer.property(F("description")).string(codec->describe());
      writer.close();
    }
    writer.close();

    writer.end();
  }

  void getConverters(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);

    writer.openList();
    for (auto& converter : _conversions.builtinConverters()) {
      writer.openObject();
      writer.property(F("id")).number(_conversions.getConverterIdByKey(converter->key()));
      writer.property(F("key")).string(converter->key());
      writer.property(F("description")).string(converter->describe());
      writer.property(F("builtIn")).boolean(true);
      writer.close();
    }
    for (auto& converter : _conversions.customConverters()) {
      if (converter) {
        writer.openObject();
        writer.property(F("id")).number(_conversions.getConverterIdByKey(converter->key()));
        writer.property(F("key")).string(converter->key());
        writer.property(F("description")).string(converter->describe());
        writer.property(F("builtIn")).boolean(false);
        writer.property(F("config"));
        serialize(writer, converter);
        writer.close();
      }
    }
    writer.close();

    writer.end();
  }

  void getCustomConverters(iot_core::api::IRequest&, iot_core::api::IResponse& response) {
    auto& body = response
      .code(iot_core::api::ResponseCode::Ok)
      .contentType(iot_core::api::ContentType::ApplicationJson)
      .sendChunkedBody();

    if (!body.valid()) {
      return;
    }

    auto writer = jsons::makeWriter(body);

    writer.openList();
    for (auto& converter : _conversions.customConverters()) {
      if (converter) {
        writer.openObject();
        writer.property(F("id")).number(_conversions.getConverterIdByKey(converter->key()));
        writer.property(F("key")).string(converter->key());
        writer.property(F("description")).string(converter->describe());
        writer.property(F("builtIn")).boolean(false);
        writer.property(F("config"));
        serialize(writer, converter);
        writer.close();
      }
    }
    writer.close();

    writer.end();
  }

  void getCustomConverter(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    auto key = request.pathArg(0);
    auto converterId = _conversions.getConverterIdByKey(key);
    if (!isCustomConverterId(converterId)) {
      converterId = iot_core::convert<ConverterId>::fromString(key.cstr(), nullptr, 10);
    }
    auto converter = _customConverters.getCustomConverter(converterId);

    if (!converter) {
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

    writer.openObject();
    writer.property(F("id")).number(_conversions.getConverterIdByKey(converter->key()));
    writer.property(F("key")).string(converter->key());
    writer.property(F("description")).string(converter->describe());
    writer.property(F("builtIn")).boolean(false);
    writer.property(F("config"));
    serialize(writer, converter);
    writer.close();
    
    writer.end();
  }

  void putCustomConverter(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    auto transaction = toolbox::beginTransaction(_customConverters);

    auto key = request.pathArg(0);
    auto converterId = _conversions.getConverterIdByKey(key);
    if (!isCustomConverterId(converterId)) {
      converterId = iot_core::convert<ConverterId>::fromString(key.cstr(), nullptr, 10);
    }
    auto existing = _customConverters.getCustomConverter(converterId);

    ICustomConverter* updated = nullptr;

    auto reader = jsons::makeReader(request.body());
    auto json = reader.begin();
    for (auto& property : json.asObject()) {
      if (property.name() == "id" && property.type() == jsons::ValueType::Integer) {
        if (property.asInteger() != converterId) {
          response
            .code(iot_core::api::ResponseCode::BadRequest)
            .contentType(iot_core::api::ContentType::TextPlain)
            .sendSingleBody().write(toolbox::format(F("Converter ID %u must match ID %u from path."), property.asInteger().get(), converterId));
          return;
        }
      } else if (property.name() == "config" && property.type() == jsons::ValueType::Object) {
        updated = deserialize(property, existing);
      }
    }

    if (reader.end().failed()) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(toolbox::format(F("JSON error: %s"), reader.diagnostics().errorMessage.cstr()));
      return;
    }

    if (!updated) {
      response
        .code(iot_core::api::ResponseCode::BadRequest)
        .contentType(iot_core::api::ContentType::TextPlain)
        .sendSingleBody().write(F("Converter deserialization failed."));
      return;
    }

    if (!_customConverters.store(converterId, updated)) {
      response.code(iot_core::api::ResponseCode::InsufficientStorage);
      return;
    }

    transaction.commit();

    getCustomConverter(request, response);
  }

  void deleteCustomConverter(iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
    auto key = request.pathArg(0);
    auto converterId = _conversions.getConverterIdByKey(key);
    if (!isCustomConverterId(converterId)) {
      converterId = iot_core::convert<ConverterId>::fromString(key.cstr(), nullptr, 10);
    }

    _customConverters.remove(converterId);
    _customConverters.commit();
    response.code(iot_core::api::ResponseCode::OkNoContent);
  }
};

#endif
