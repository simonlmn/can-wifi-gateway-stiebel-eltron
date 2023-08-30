#ifndef MQTTCLIENT_H_
#define MQTTCLIENT_H_

#include "src/iot-core/Interfaces.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

class MqttClient final : public iot_core::IApplicationComponent {
private:
  iot_core::Logger& _logger;
  iot_core::ISystem& _system;

  DataAccess& _access;
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;

  char _brokerAddress[16] = {};
  uint16_t _brokerPort = 1883;
  char _topic[32] = {};

public:
  MqttClient(iot_core::ISystem& system, DataAccess& access) :
    _logger(system.logger()),
    _system(system),
    _access(access),
    _wifiClient(),
    _mqttClient(_wifiClient)
  {
    iot_core::str("52.29.250.158" /* broker.hivemq.com */).copy(_brokerAddress, 15);
    iot_core::str("can-wifi-gateway-stiebel-eltron").copy(_topic, 31);
  }

  const char* name() const override {
    return "mqc";
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "broker") == 0) return setBrokerAddress(value);
    if (strcmp(name, "port") == 0) return setBrokerPort(iot_core::convert<uint16_t>::fromString(value, nullptr, 10));
    if (strcmp(name, "topic") == 0) return setTopic(value);
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("broker", _brokerAddress);
    writer("port", iot_core::convert<uint16_t>::toString(_brokerPort, 10));
    writer("topic", _topic);
  }

  bool setBrokerAddress(const char* address) {
    iot_core::str(address).copy(_brokerAddress, 15);
    _mqttClient.setServer(_brokerAddress, _brokerPort);
    return true;
  }

  bool setBrokerPort(uint16_t port) {
    _brokerPort = port;
    _mqttClient.setServer(_brokerAddress, _brokerPort);
    return true;
  }

  bool setTopic(const char* topic) {
    iot_core::str(topic).copy(_topic, 31);
    return true;
  }

  void setup(bool /*connected*/) override {
    _mqttClient.setServer(_brokerAddress, _brokerPort);
    _access.onUpdate([&] (DataEntry const& entry) {
      if (!_mqttClient.connected()) {
        return;
      }

      if (entry.hasDefinition()) {
        _mqttClient.publish(_topic, iot_core::format("update entry %u: %s", entry.id, entry.definition->fromRaw(entry.rawValue)));
      }
    });
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    _mqttClient.loop();

    if (!_mqttClient.connected()) {
      _mqttClient.connect(iot_core::format("wifi-gateway-%s", _system.id()));
    }
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& /*collector*/) const override {
  }
};

#endif
