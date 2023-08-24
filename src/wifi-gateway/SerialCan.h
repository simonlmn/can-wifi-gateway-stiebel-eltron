#pragma once

#include "ApplicationContainer.h"
#include "CanInterface.h"
#include "src/shared/SerialProtocol.h"

class SerialCan final : public IConfigurable {
private:
  static const uint32_t CAN_BITRATE = 20UL * 1000UL; // 20 kbit/s
  static constexpr uint32_t MAX_ERR_COUNT = 5;

  ApplicationContainer& _system;
  DigitalOutput& _resetPin;
  DigitalInput& _txEnablePin;
  bool _canAvailable;
  IntervalTimer _resetInterval;
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  
  CanMode _mode = CanMode::ListenOnly;
  CanLogLevel _logLevel = CanLogLevel::Status;
  CanCounters _counters;

  SerialProtocol _serial;

public:
  SerialCan(ApplicationContainer& system, DigitalOutput& resetPin, DigitalInput& txEnablePin) :
    _system(system),
    _resetPin(resetPin),
    _txEnablePin(txEnablePin),
    _canAvailable(false),
    _resetInterval(30000),
    _counters(),
    _serial([this] (const char* message, SerialProtocol& serial) { processReceived(message, serial); }, [this] (SerialErrorCode errorCode, SerialProtocol& serial) { handleError(errorCode, serial); })
  {
    system.logLevel("can", (int)_logLevel);
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "mode") == 0) return setMode(canModeFromString(value));
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("mode", canModeName(_mode));
  }

  bool setMode(CanMode mode) {
    _mode = mode;
    reset();
    _system.log("can", format("Set mode '%s'.", canModeName(_mode)));
    return true;
  }

  CanMode effectiveMode() const {
    return _txEnablePin ? _mode : CanMode::ListenOnly;
  }

  void setup() {
    if (_logLevel >= CanLogLevel::Status) _system.log("can", "Initializing SerialCan.");
    _serial.setup();
    delay(100);
    _system.addConfigurable("can", this);
    reset();
  }

  void loop() {
    _logLevel = (CanLogLevel)_system.logLevel("can");
    _serial.loop();

    if (!ready() && _resetInterval.elapsed()) {
      if (_logLevel >= CanLogLevel::Error) _system.log("can", "Timeout: resetting CAN module.");
      resetInternal();
    }

    if (_counters.err > MAX_ERR_COUNT) {
      if (_logLevel >= CanLogLevel::Error) _system.log("can", "Error threshold reached: resetting CAN module.");
      resetInternal();
    }
  }

  void reset() {
    if (_logLevel >= CanLogLevel::Status) _system.log("can", "Resetting CAN module.");
    resetInternal();
  }

  bool ready() const {
    return _canAvailable;
  }

  void onReady(std::function<void()> readyHandler) {
    _readyHandler = readyHandler;
    if (_canAvailable) {
      if (_readyHandler) _readyHandler();
    }
  }

  void onMessage(std::function<void(const CanMessage& message)> messageHandler) {
    _messageHandler = messageHandler;
  }

  void sendCanMessage(const CanMessage& message) {
    if (effectiveMode() == CanMode::ListenOnly) {
      return;
    }

    if (_canAvailable) {
      if (_logLevel >= CanLogLevel::Tx) {
        logCanMessage("TX", message);
      }

      // TODO check _serial.canQueue() and/or result from queueCanTxMessage()
      queueCanTxMessage(_serial, message.id, message.ext, message.rtr, message.len, message.data);

      _counters.tx += 1;
    }
  }

  CanCounters const& counters() const {
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

  void handleError(SerialErrorCode errorCode, SerialProtocol& serial) {
    if (_logLevel >= CanLogLevel::Error) _system.log("can", format("Serial error %c: %s", static_cast<char>(errorCode), errorCodeDescription(errorCode)));
    _counters.err += 1;
  }

  void processReceived(const char* message, SerialProtocol& serial) {
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

      if (_logLevel >= CanLogLevel::Rx) {
        logCanMessage("RX", message);
      }

      if (_messageHandler) _messageHandler(message);

      _counters.rx += 1;
    } else if (strncmp(start, "CANTX ", 6) == 0) {
      if (strncmp(start + 6, "OK ", 3) != 0) {
        _counters.err += 1;
        if (_logLevel >= CanLogLevel::Error) _system.log("can", message);
      }
    } else if (strncmp(start, "READY", 5) == 0) {
      serial.queue("SETUP %X %s", CAN_BITRATE, toSetupModeString(effectiveMode()));
    } else if (strncmp(start, "SETUP ", 6) == 0) {
      _canAvailable = strncmp(start + 6, "OK ", 3) == 0;
      if (_canAvailable) {
        if (_logLevel >= CanLogLevel::Status) _system.log("can", message);
        if (_readyHandler) _readyHandler();
      } else {
        if (_logLevel >= CanLogLevel::Error) _system.log("can", message);
      }
    } else {
      _counters.err += 1;
      if (_logLevel >= CanLogLevel::Error) _system.log("can", message);
    }
  }

  void logCanMessage(const char* prefix, const CanMessage& message) {
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
    
    _system.log("can", logMessage);
  }

  const char* toSetupModeString(CanMode mode) const {
    switch (effectiveMode())
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
