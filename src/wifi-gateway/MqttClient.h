#ifndef MQTTCLIENT_H_
#define MQTTCLIENT_H_

#include <iot_core/Interfaces.h>
#include <iot_core/Buffer.h>
#include <iot_core/Utils.h>
#include <jsons/Writer.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "DataAccess.h"
#include "Serializer.h"

class MqttClient final : public iot_core::IApplicationComponent {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;

  DataAccess& _access;
  IConversionService& _conversion;
  IDefinitionRepository& _definitions;
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
  iot_core::Buffer<640u> _buffer;

  bool _enabled = false;
  char _brokerAddress[16] = {};
  uint16_t _brokerPort = 1883;
  char _topic[32] = {};

  size_t _discardedUpdates = 0u;

public:
  MqttClient(iot_core::ISystem& system, DataAccess& access, IConversionService& conversion, IDefinitionRepository& definitions) :
    _logger(system.logger("mqc")),
    _system(system),
    _access(access),
    _conversion(conversion),
    _definitions(definitions),
    _wifiClient(),
    _mqttClient(_wifiClient)
  {
    toolbox::strref(F("52.29.250.158") /* broker.hivemq.com */).copy(_brokerAddress, 16, true);
    toolbox::strref(F("can-wifi-gateway-stiebel-eltron")).copy(_topic, 32, true);
  }

  const char* name() const override {
    return "mqc";
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "enabled") == 0) return setEnabled(iot_core::convert<bool>::fromString(value, false));
    if (strcmp(name, "broker") == 0) return setBrokerAddress(value);
    if (strcmp(name, "port") == 0) return setBrokerPort(iot_core::convert<uint16_t>::fromString(value, nullptr, 10));
    if (strcmp(name, "topic") == 0) return setTopic(value);
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("enabled", iot_core::convert<bool>::toString(_enabled));
    writer("broker", _brokerAddress);
    writer("port", iot_core::convert<uint16_t>::toString(_brokerPort, 10));
    writer("topic", _topic);
  }

  bool setEnabled(bool enabled) {
    if (enabled != _enabled) {
      _enabled = enabled;
      reset();
    }
    _logger.log(toolbox::format(F("MQTT client %s."), _enabled ? "enabled" : "disabled"));
    return true;
  }

  bool setBrokerAddress(const char* address) {
    toolbox::strref(address).copy(_brokerAddress, 16, true);
    _logger.log(toolbox::format(F("Using address '%s'."), _brokerAddress));
    reset();
    return true;
  }

  bool setBrokerPort(uint16_t port) {
    _brokerPort = port;
    _logger.log(toolbox::format(F("Using port %u."), _brokerPort));
    reset();
    return true;
  }

  bool setTopic(const char* topic) {
    toolbox::strref(topic).copy(_topic, 32, true);
    _logger.log(toolbox::format(F("Using topic '%s'."), _topic));
    return true;
  }

  void setup(bool /*connected*/) override {
    _mqttClient.setServer(_brokerAddress, _brokerPort);
    _access.onUpdate([&] (DataEntry const& entry) { handleUpdate(entry); });
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    if (!_enabled) {
      return;
    }

    _mqttClient.loop();

    if (!_mqttClient.connected()) {
      _mqttClient.connect(toolbox::format(F("wifi-gateway-%s"), _system.id()));
    } else {
      if (_discardedUpdates != 0u) {
        _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Discarded %u updates while disconnected."), _discardedUpdates));
      }
      _discardedUpdates = 0u;
    }
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& /*collector*/) const override {
  }

private:
  void reset() {
    _mqttClient.disconnect();
    _mqttClient.setServer(_brokerAddress, _brokerPort);
    _discardedUpdates = 0u;
  }

  void handleUpdate(DataEntry const& entry) {
    if (!_enabled) {
      return;
    }

    if (!_mqttClient.connected()) {
      if (_discardedUpdates == 0u) {
        _logger.log(iot_core::LogLevel::Warning, F("Disconnected, discarding updates."));
      }
      _discardedUpdates += 1u;
      return;
    }
    
    _buffer.clear();
    auto writer = jsons::makeWriter(_buffer);
    serializer::serialize(writer, _conversion, _definitions, entry, true, true);
    if (writer.failed()) {
      _logger.log(iot_core::LogLevel::Error, F("Serializing data entry failed."));
    } else if (_buffer.overrun()) {
      _logger.log(iot_core::LogLevel::Warning, F("Serialized data entry too large for buffer."));
    } else {
      _mqttClient.publish(
        toolbox::format("%s/%s/%u/%u", _topic, deviceTypeToString(entry.source.type), entry.source.address, entry.id),
        _buffer.data(),
        _buffer.size()
      );
    }
  }
};

#endif
