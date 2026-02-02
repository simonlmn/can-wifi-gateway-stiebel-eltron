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
  toolbox::str<16> _brokerAddress;
  uint16_t _brokerPort;
  toolbox::str<32> _topic;

  size_t _discardedUpdates = 0u;

public:
  MqttClient(iot_core::ISystem& system, DataAccess& access, IConversionService& conversion, IDefinitionRepository& definitions) :
    _logger(system.logger(F("mqc"))),
    _system(system),
    _access(access),
    _conversion(conversion),
    _definitions(definitions),
    _wifiClient(),
    _mqttClient(_wifiClient),
    _brokerAddress(F("52.29.250.158")),
    _brokerPort(1883),
    _topic(F("can-wifi-gateway-stiebel-eltron"))
  {
  }

  toolbox::strref name() const override {
    return F("mqc");
  }

  bool configure(const toolbox::strref& name, const toolbox::strref& value) override {
    if (name == F("enabled")) return setEnabled(toolbox::convert<bool>::fromString(value).otherwise(false));
    if (name == F("broker")) return setBrokerAddress(value);
    if (name == F("port")) return setBrokerPort(toolbox::convert<uint16_t>::fromString(value, nullptr, 10).otherwise(1883));
    if (name == F("topic")) return setTopic(value);
    return false;
  }

  void getConfig(iot_core::ConfigWriter writer) const override {
    writer(F("enabled"), toolbox::convert<bool>::toString(_enabled));
    writer(F("broker"), _brokerAddress);
    writer(F("port"), toolbox::convert<uint16_t>::toString(_brokerPort, 10));
    writer(F("topic"), _topic);
  }

  bool setEnabled(bool enabled) {
    if (enabled != _enabled) {
      _enabled = enabled;
      reset();
    }
    _logger.log(toolbox::format(F("MQTT client %s."), _enabled ? "enabled" : "disabled"));
    return true;
  }

  bool setBrokerAddress(const toolbox::strref& address) {
    _brokerAddress = address;
    _logger.log(toolbox::format(F("Using address '%s'."), _brokerAddress.cstr()));
    reset();
    return true;
  }

  bool setBrokerPort(uint16_t port) {
    _brokerPort = port;
    _logger.log(toolbox::format(F("Using port %u."), _brokerPort));
    reset();
    return true;
  }

  bool setTopic(const toolbox::strref& topic) {
    _topic = topic;
    _logger.log(toolbox::format(F("Using topic '%s'."), _topic.cstr()));
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
      _mqttClient.connect(toolbox::format(F("wifi-gateway-%s"), _system.id().cstr()));
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
        toolbox::format(F("%s/%s/%u/%u"), _topic.cstr(), deviceTypeToString(entry.source.type).cstr(), entry.source.address, entry.id),
        _buffer.data(),
        _buffer.size()
      );
    }
  }
};

#endif
