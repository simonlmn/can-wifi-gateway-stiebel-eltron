#ifndef FAKECAN_H_
#define FAKECAN_H_

#include <iot_core/Interfaces.h>
#include <iot_core/Utils.h>
#include "CanInterface.h"

class FakeCan final : public ICanInterface, public iot_core::IApplicationComponent {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  bool _canAvailable;
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  
public:
  FakeCan(iot_core::ISystem& system) :
    _logger(system.logger("fan")),
    _system(system),
    _canAvailable(false)
  {
  }

  const char* name() const override {
    return "fan";
  }

  bool configure(const char* name, const char* value) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
  }

  bool setMode(CanMode mode) override {
    return true;
  }

  void setup(bool /*connected*/) override {
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    if (!_canAvailable && millis() > 10000) {
      _canAvailable = true;
      if (_readyHandler) {
        Serial.println("CAN calling reading handler");
        _readyHandler();
      }
    }

    if (_canAvailable && millis() % 1000 == 0) {
      if (_messageHandler) {
        CanMessage message;
        message.id = 0x180u;
        message.ext = false;
        message.rtr = false;
        message.len = 7;
        message.data[0] = 0xD0u;
        message.data[1] = 0x3Cu;
        message.data[2] = 0xFAu;
        message.data[6] = 0x00u;

        message.data[3] = 0x01u;
        message.data[4] = 0x22u;
        message.data[5] = 0x0Eu;
        _messageHandler(message);

        message.data[3] = 0x01u;
        message.data[4] = 0x23u;
        message.data[5] = 0x09u;
        _messageHandler(message);

        message.data[3] = 0x01u;
        message.data[4] = 0x24u;
        message.data[5] = 0x17u;
        _messageHandler(message);

        message.data[3] = 0x01u;
        message.data[4] = 0x25u;
        message.data[5] = 0x14u;
        _messageHandler(message);

        message.data[3] = 0x01u;
        message.data[4] = 0x26u;
        message.data[5] = 0x22u;
        _messageHandler(message);
      }
    }
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& collector) const override {
  }

  void reset() override {
    _canAvailable = false;
  }

  bool ready() const override {
    return _canAvailable;
  }

  void onReady(std::function<void()> readyHandler) override {
    _readyHandler = readyHandler;
    if (_canAvailable) {
      if (_readyHandler) _readyHandler();
    }
  }

  void onMessage(std::function<void(const CanMessage& message)> messageHandler) override {
    _messageHandler = messageHandler;
  }

  OperationResult sendCanMessage(const CanMessage& message) override {
    return OperationResult::Accepted; // FakeCan always accepts messages
  }

  float getAvailableTokens() const override {
    return 999.0f; // FakeCan has unlimited budget
  }

  CanCounters const& counters() const override {
    return {};
  }
};

#endif
