#ifndef SERIALCAN_H_
#define SERIALCAN_H_

#include <iot_core/Interfaces.h>
#include <serial_transport.h>
#include <gpiobj.h>
#include "CanInterface.h"

class SerialCan final : public ICanInterface, public iot_core::IApplicationComponent {
private:
  static const uint32_t CAN_BITRATE = 20UL * 1000UL; // 20 kbit/s
  static constexpr uint32_t MAX_ERR_COUNT = 5;

  // Token bucket rate limiter for 20 kbit/s bus protection
  // Analysis: ~4.4ms per CAN frame (worst case with bit stuffing)
  // Theoretical max: ~220 frames/sec, we use ~12 frames/sec = ~5.4% bus utilization
  static constexpr float MAX_FRAMES_PER_SECOND = 12.0f;
  static constexpr float MAX_BURST_TOKENS = 6.0f;

  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  gpiobj::DigitalOutput& _resetPin;
  gpiobj::DigitalInput& _txEnablePin;
  bool _canAvailable;
  iot_core::IntervalTimer _resetInterval;
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  
  unsigned long _lastTokenRefillMs;
  float _availableTokens;
  
  CanMode _mode = CanMode::ListenOnly;
  CanCounters _counters;

  serial_transport::Endpoint _serial;

public:
  SerialCan(iot_core::ISystem& system, gpiobj::DigitalOutput& resetPin, gpiobj::DigitalInput& txEnablePin) :
    _logger(system.logger("can")),
    _system(system),
    _resetPin(resetPin),
    _txEnablePin(txEnablePin),
    _canAvailable(false),
    _resetInterval(30000),
    _counters(),
    _lastTokenRefillMs(0),
    _availableTokens(MAX_BURST_TOKENS),
    _serial(
      serial_transport::EndpointRole::CLIENT,
      Serial,
      [this] (const uint8_t* payload, uint8_t payloadLen, serial_transport::Endpoint& serial) { processReceived(reinterpret_cast<const char*>(payload), serial); },
      [this] (serial_transport::ConnectionState state, serial_transport::Endpoint& serial) { handleConnectionState(state, serial); },
      [this] (char direction, uint8_t type, uint8_t sequenceNumber, const uint8_t* payload, uint8_t payloadLen) { handleFrame(direction, type, sequenceNumber, payload, payloadLen); }
    )
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
    writer("mode", canModeToString(_mode).cstr());
  }

  bool setMode(CanMode mode) override {
    _mode = mode;
    reset();
    _logger.log(toolbox::format(F("Set mode '%s'."), canModeToString(_mode).cstr()));
    return true;
  }

  CanMode effectiveMode() const {
    return _txEnablePin ? _mode : CanMode::ListenOnly;
  }

  void setup(bool /*connected*/) override {
    _logger.log(iot_core::LogLevel::Info, F("Initializing SerialCan."));
    _serial.setup();
    delay(100);
    reset();
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    _serial.loop();

    refillTokenBucket(); // Periodically refill to maintain budget

    if (!ready() && _resetInterval.elapsed()) {
      _logger.log(iot_core::LogLevel::Error, F("Timeout: resetting CAN module."));
      resetInternal();
    }

    if (_counters.err > MAX_ERR_COUNT) {
      _logger.log(iot_core::LogLevel::Error, F("Error threshold reached: resetting CAN module."));
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
    _logger.log(iot_core::LogLevel::Info, F("Resetting CAN module."));
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

  OperationResult sendCanMessage(const CanMessage& message) override {
    if (effectiveMode() == CanMode::ListenOnly) {
      return OperationResult::NotReady;
    }

    if (!_canAvailable) {
      return OperationResult::NotReady;
    }

    if (_availableTokens < 1.0f) {
      return OperationResult::RateLimited;
    }

    logCanMessage("TX", message);

    if (!queueCanTxMessage(_serial, message.id, message.ext, message.rtr, message.len, message.data)) {
      return OperationResult::QueueFull;
    }

    _availableTokens -= 1.0f;
    _counters.tx += 1;
    return OperationResult::Accepted;
  }

  float getAvailableTokens() const override {
    return _availableTokens;
  }

  CanCounters const& counters() const override {
    return _counters;
  }

private: 
  void refillTokenBucket() {
    unsigned long currentMs = millis();
    if (_lastTokenRefillMs > 0) {
      float elapsedSeconds = (currentMs - _lastTokenRefillMs) / 1000.0f;
      _availableTokens = std::min(_availableTokens + (MAX_FRAMES_PER_SECOND * elapsedSeconds), MAX_BURST_TOKENS);
    }
    _lastTokenRefillMs = currentMs;
  }

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

  void handleConnectionState(serial_transport::ConnectionState state, serial_transport::Endpoint& /*serial*/) {
    iot_core::LogLevel level = state == serial_transport::ConnectionState::CLOSED ? iot_core::LogLevel::Warning : iot_core::LogLevel::Info;
    _logger.log(level, [&] () { return toolbox::format(F("Serial connection: %s"), serial_transport::describe(state)); });
  }

  void handleFrame(char direction, uint8_t type, uint8_t sequenceNumber, const uint8_t* payload, uint8_t payloadLen) {
    _logger.log(iot_core::LogLevel::Trace, [&] () {
      static char logMessage[96]; // "TX|RX FRAME type=XX seq=XX len=X ...";
      int insertPos = 0;
      insertPos += snprintf(logMessage + insertPos, 96 - insertPos, "%cX FRAME T=%02X S=%02X L=%u ", direction, type, sequenceNumber, payloadLen);
      for (size_t i = 0; i < payloadLen; ++i) {
        logMessage[insertPos++] = payload[i];
      }
      logMessage[insertPos] = '\0';
      return logMessage;
    });
  }

  void processReceived(const char* message, serial_transport::Endpoint& serial) {
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
      if (strncmp(start + 6, "OK", 2) == 0) {
        // Success - no action needed
      } else if (strncmp(start + 6, "ENVAL", 5) == 0) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, F("CANTX ENVAL: Invalid CAN message format or parameters"));
      } else if (strncmp(start + 6, "ESEND", 5) == 0) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, F("CANTX ESEND: CAN TX buffer full, message dropped"));
      } else if (strncmp(start + 6, "ENOAV", 5) == 0) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, F("CANTX ENOAV: CAN module not available"));
      } else {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, [&] () { return toolbox::format(F("CANTX unknown error: %s"), message); });
      }
    } else if (strncmp(start, "READY", 5) == 0) {
      serial.queue("SETUP %X %s", CAN_BITRATE, toSetupModeString(effectiveMode()));
    } else if (strncmp(start, "SETUP ", 6) == 0) {
      _canAvailable = strncmp(start + 6, "OK ", 3) == 0;
      if (_canAvailable) {
        _logger.log(iot_core::LogLevel::Info, message);
        if (_readyHandler) _readyHandler();
      } else {
        _logger.log(iot_core::LogLevel::Error, message);
      }
    } else {
      _counters.err += 1;
      _logger.log(iot_core::LogLevel::Error, message);
    }
  }

  void logCanMessage(const char* prefix, const CanMessage& message) {
    _logger.log(iot_core::LogLevel::Debug, [&] () {
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
