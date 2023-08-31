#ifndef SERIALCAN_H_
#define SERIALCAN_H_

#include "src/iot-core/Interfaces.h"
#include "src/iot-core/Utils.h"
#include "src/pins/DigitalOutput.h"
#include "src/pins/DigitalInput.h"
#include "src/serial-protocol/Endpoint.h"
#include "CanInterface.h"

class SerialCan final : public ICanInterface, public iot_core::IApplicationComponent {
private:
  static const uint32_t CAN_BITRATE = 20UL * 1000UL; // 20 kbit/s
  static constexpr uint32_t MAX_ERR_COUNT = 5;

  iot_core::Logger& _logger;
  iot_core::ISystem& _system;
  pins::DigitalOutput& _resetPin;
  pins::DigitalInput& _txEnablePin;
  bool _canAvailable;
  iot_core::IntervalTimer _resetInterval;
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  
  CanMode _mode = CanMode::ListenOnly;
  CanCounters _counters;

  serial_protocol::Endpoint _serial;

public:
  SerialCan(iot_core::ISystem& system, pins::DigitalOutput& resetPin, pins::DigitalInput& txEnablePin) :
    _logger(system.logger()),
    _system(system),
    _resetPin(resetPin),
    _txEnablePin(txEnablePin),
    _canAvailable(false),
    _resetInterval(30000),
    _counters(),
    _serial([this] (const char* message, serial_protocol::Endpoint& serial) { processReceived(message, serial); }, [this] (serial_protocol::ErrorCode errorCode, serial_protocol::Endpoint& serial) { handleError(errorCode, serial); })
  {
  }

  const char* name() const override {
    return "can";
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "mode") == 0) return setMode(canModeFromString(value));
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("mode", canModeToString(_mode));
  }

  bool setMode(CanMode mode) override {
    _mode = mode;
    reset();
    _logger.log(name(), iot_core::format(F("Set mode '%s'."), canModeToString(_mode)));
    return true;
  }

  CanMode effectiveMode() const {
    return _txEnablePin ? _mode : CanMode::ListenOnly;
  }

  void setup(bool /*connected*/) override {
    _logger.log(iot_core::LogLevel::Info, name(), F("Initializing SerialCan."));
    _serial.setup();
    delay(100);
    reset();
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    _serial.loop();

    if (!ready() && _resetInterval.elapsed()) {
      _logger.log(iot_core::LogLevel::Error, name(), F("Timeout: resetting CAN module."));
      resetInternal();
    }

    if (_counters.err > MAX_ERR_COUNT) {
      _logger.log(iot_core::LogLevel::Error, name(), F("Error threshold reached: resetting CAN module."));
      resetInternal();
    }
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& collector) const override {
    collector.addValue("available", iot_core::convert<bool>::toString(_canAvailable));
    collector.addValue("err", iot_core::convert<uint32_t>::toString(_counters.err, 10));
    collector.addValue("rx", iot_core::convert<uint32_t>::toString(_counters.rx, 10));
    collector.addValue("tx", iot_core::convert<uint32_t>::toString(_counters.tx, 10));
  }

  void reset() override {
    _logger.log(iot_core::LogLevel::Info, name(), F("Resetting CAN module."));
    resetInternal();
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

  void sendCanMessage(const CanMessage& message) override {
    if (effectiveMode() == CanMode::ListenOnly) {
      return;
    }

    if (_canAvailable) {
      logCanMessage("TX", message);

      // TODO check _serial.canQueue() and/or result from queueCanTxMessage() ?
      queueCanTxMessage(_serial, message.id, message.ext, message.rtr, message.len, message.data);

      _counters.tx += 1;
    }
  }

  CanCounters const& counters() const override {
    return _counters;
  }

private: 
  void resetInternal() {
    _canAvailable = false;
    _counters = CanCounters{};

    _resetPin = true;
    delay(100);
    _serial.reset();
    delay(100);
    _resetPin = false;
    
    _resetInterval.restart();
  }

  void handleError(serial_protocol::ErrorCode errorCode, serial_protocol::Endpoint& /*serial*/) {
    _logger.log(iot_core::LogLevel::Warning, name(), [&] () { return iot_core::format(F("Serial error %c: %s"), static_cast<char>(errorCode), serial_protocol::describe(errorCode)); });
    _counters.err += 1;
  }

  void processReceived(const char* message, serial_protocol::Endpoint& serial) {
    const char* start = message;
    char* end = nullptr;

    if (strncmp(start, "CANRX ", 6) == 0) {
      auto id = strtol(start + 6, &end, 16);
      if (end == start) {
        _counters.err += 1;
        return;
      }
      start = end;

      auto len = strtol(start, &end, 10);
      if (end == start) {
        _counters.err += 1;
        return;
      }
      start = end;

      CanMessage message;
      message.id = id & 0x1FFFFFFFu;
      message.ext = (id & 0x80000000u) != 0;
      message.rtr = (id & 0x40000000u) != 0;
      message.len = len;

      for (size_t i = 0; i < message.len; ++i) {
        message.data[i] = strtol(start, &end, 16);
        if (end == start) {
          _counters.err += 1;
          return;
        }
        start = end;
      }

      logCanMessage("RX", message);

      if (_messageHandler) _messageHandler(message);

      _counters.rx += 1;
    } else if (strncmp(start, "CANTX ", 6) == 0) {
      if (strncmp(start + 6, "OK ", 3) != 0) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, name(), message);
      }
    } else if (strncmp(start, "READY", 5) == 0) {
      serial.queue("SETUP %X %s", CAN_BITRATE, toSetupModeString(effectiveMode()));
    } else if (strncmp(start, "SETUP ", 6) == 0) {
      _canAvailable = strncmp(start + 6, "OK ", 3) == 0;
      if (_canAvailable) {
        _logger.log(iot_core::LogLevel::Info, name(), message);
        if (_readyHandler) _readyHandler();
      } else {
        _logger.log(iot_core::LogLevel::Error, name(), message);
      }
    } else {
      _counters.err += 1;
      _logger.log(iot_core::LogLevel::Error, name(), message);
    }
  }

  void logCanMessage(const char* prefix, const CanMessage& message) {
    _logger.log(iot_core::LogLevel::Debug, name(), [&] () {
      static char logMessage[36]; // "XX 123456789 xr 8 FFFFFFFFFFFFFFFF";
      int insertPos = 0;
      insertPos += snprintf(logMessage + insertPos, 36 - insertPos, "%s %x ", prefix, message.id);
      if (message.ext) {
        logMessage[insertPos++] = 'x';
      }
      if (message.rtr) {
        logMessage[insertPos++] = 'r';
      }
      insertPos += snprintf(logMessage + insertPos, 36 - insertPos, " %u ", message.len);
      for (size_t i = 0; i < message.len; ++i) {
        insertPos += snprintf(logMessage + insertPos, 36 - insertPos, "%02X", message.data[i]);
      }
      return logMessage;
    });
  }

  const char* toSetupModeString(CanMode mode) const {
    switch (mode)
    {
    case CanMode::ListenOnly:
      return "LIS";
    case CanMode::Normal:
      return "NOR";
    case CanMode::LoopBack:
      return "LOP";
    default:
      return "ERR";
    }
  }
};

#endif
