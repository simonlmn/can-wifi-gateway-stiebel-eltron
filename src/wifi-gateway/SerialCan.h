#pragma once

#include "NodeBase.h"
#include "CanInterface.h"
#include "src/shared/SerialProtocol.h"

class SerialCan final : public IConfigurable {
private:
  static const uint32_t CAN_BITRATE = 20UL * 1000UL; // 20 kbit/s
  static constexpr uint32_t MAX_ERR_COUNT = 5;

  NodeBase& _node;
  DigitalOutput& _resetPin;
  DigitalInput& _listenModePin;
  bool _canAvailable;
  IntervalTimer _resetInterval;
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  
  CanMode _mode = CanMode::ListenOnly;
  CanLogLevel _logLevel = CanLogLevel::Status;
  CanCounters _counters;

  SerialProtocol _serial;

public:
  SerialCan(NodeBase& node, DigitalOutput& resetPin, DigitalInput& listenModePin) :
    _node(node),
    _resetPin(resetPin),
    _listenModePin(listenModePin),
    _canAvailable(false),
    _resetInterval(30000),
    _counters(),
    _serial([this] (const char* line, SerialProtocol& serial) { processReceivedLine(line, serial); })
  {
    node.logLevel("can", (int)_logLevel);
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "mode") == 0) return setMode(CanMode(strtol(value, nullptr, 10)));
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("mode", toConstStr(uint8_t(_mode), 10));
  }

  bool setMode(CanMode mode) {
    _mode = mode;
    reset();
    return true;
  }

  CanMode effectiveMode() const {
    return _listenModePin ? CanMode::ListenOnly : _mode;
  }

  void setup() {
    if (_logLevel >= CanLogLevel::Status) _node.log("can", "Initializing SerialCan.");
    _serial.setup();
    delay(100);
    _node.addConfigurable("can", this);
    reset();
  }

  void loop() {
    _logLevel = (CanLogLevel)_node.logLevel("can");
    _serial.receive();

    if (!ready() && _resetInterval.elapsed()) {
      if (_logLevel >= CanLogLevel::Error) _node.log("can", "Timeout: resetting CAN module.");
      resetInternal();
    }
  }

  void reset() {
    if (_logLevel >= CanLogLevel::Status) _node.log("can", "Resetting CAN module.");
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

      sendTxMessage(_serial, message.id, message.ext, message.rtr, message.len, message.data);

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
    _resetPin = false;
    
    _resetInterval.restart();
  }

  void processReceivedLine(const char* line, SerialProtocol& serial) {
    const char* start = line;
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
        if (_logLevel >= CanLogLevel::Error) _node.log("can", line);
      }
    } else if (strncmp(start, "READY", 5) == 0) {
      serial.send("SETUP %X %s", CAN_BITRATE, toSetupModeString(effectiveMode()));
    } else if (strncmp(start, "SETUP ", 6) == 0) {
      _canAvailable = strncmp(start + 6, "OK ", 3) == 0;
      if (_canAvailable) {
        if (_logLevel >= CanLogLevel::Status) _node.log("can", line);
        if (_readyHandler) _readyHandler();
      } else {
        if (_logLevel >= CanLogLevel::Error) _node.log("can", line);
      }
    } else {
      _counters.err += 1;
      if (_logLevel >= CanLogLevel::Error) _node.log("can", line);
    }

    if (_counters.err > MAX_ERR_COUNT) {
      if (_logLevel >= CanLogLevel::Error) _node.log("can", "Error threshold reached: resetting CAN module.");
      resetInternal();
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
    
    _node.log("can", logMessage);
  }

  const char* toSetupModeString(CanMode mode) const {
    switch (effectiveMode())
    {
    case CanMode::ListenOnly:
      return "LIS";
    case CanMode::Normal:
      return "NOR";
    default:
      return "ERR";
    }
  }
};
