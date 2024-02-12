#ifndef STIEBELELTRONPROTOCOLAPI_H_
#define STIEBELELTRONPROTOCOLAPI_H_

#include <iot_core/api/Interfaces.h>
#include <jsons/Writer.h>
#include "StiebelEltronProtocol.h"

class StiebelEltronProtocolApi final : public iot_core::api::IProvider {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  StiebelEltronProtocol& _protocol;

public:
  StiebelEltronProtocolApi(iot_core::ISystem& system, StiebelEltronProtocol& protocol)
  : _logger(system.logger("api")), _system(system), _protocol(protocol) {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/api/devices"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest& request, iot_core::api::IResponse& response) {
      getDevices(request, response);
    });  
  }

private:
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
