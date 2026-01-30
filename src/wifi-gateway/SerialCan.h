#ifndef SERIALCAN_H_
#define SERIALCAN_H_

#include <iot_core/Interfaces.h>
#include <toolbox/String.h>
#include <toolbox/Conversion.h>
#include <serial_transport.h>
#include <gpiobj.h>
#include "CanInterface.h"

class SerialCan final : public ICanInterface, public iot_core::IApplicationComponent {
private:
  static constexpr uint32_t MAX_ERR_COUNT = 5;

  static const uint32_t CAN_BITRATE = 20000UL; // 20 kbit/s
  static constexpr float MAX_BUS_UTILIZATION = 0.1f;

  static constexpr uint32_t CAN_MAX_BITS_PER_FRAME = 125ul; // 125 bits per CAN frame (worst case with bit stuffing)
  static constexpr float MAX_FRAMES_PER_SECOND = MAX_BUS_UTILIZATION * (CAN_BITRATE / CAN_MAX_BITS_PER_FRAME);
  static constexpr float MAX_BURST_TOKENS = MAX_FRAMES_PER_SECOND / 2.0f;

  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  gpiobj::DigitalOutput& _resetPin;
  gpiobj::DigitalInput& _txEnablePin;
  bool _canReady;
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
    _canReady(false),
    _resetInterval(5000),
    _counters(),
    _lastTokenRefillMs(0),
    _availableTokens(MAX_BURST_TOKENS),
    _serial(
      serial_transport::EndpointRole::CLIENT,
      Serial,
      [this] (const uint8_t* payload, uint8_t payloadLen, serial_transport::Endpoint& serial) { processReceived({reinterpret_cast<const char*>(payload), payloadLen}, serial); },
      [this] (serial_transport::ConnectionState state, serial_transport::Endpoint& serial) { handleConnectionState(state, serial); },
      [this] (char direction, uint8_t type, uint8_t sequenceNumber, const uint8_t* payload, uint8_t payloadLen) { logFrame(direction, type, sequenceNumber, payload, payloadLen); }
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
    collector.addValue("available", toolbox::convert<bool>::toString(_canReady));
    collector.addValue("err", toolbox::convert<uint32_t>::toString(_counters.err, 10));
    collector.addValue("rx", toolbox::convert<uint32_t>::toString(_counters.rx, 10));
    collector.addValue("tx", toolbox::convert<uint32_t>::toString(_counters.tx, 10));
  }

  void reset() override {
    _logger.log(iot_core::LogLevel::Info, F("Resetting CAN module."));
    resetInternal();
  }

  bool ready() const override {
    return _canReady;
  }

  void onReady(std::function<void()> readyHandler) override {
    _readyHandler = readyHandler;
    if (_canReady) {
      if (_readyHandler) _readyHandler();
    }
  }

  void onMessage(std::function<void(const CanMessage& message)> messageHandler) override {
    _messageHandler = messageHandler;
  }

  OperationResult sendCanMessage(const CanMessage& message) override {
    if (!_canReady) {
      return OperationResult::NotReady;
    }
    
    if (effectiveMode() == CanMode::ListenOnly) {
      return OperationResult::Unavailable;
    }

    if (_availableTokens < 1.0f) {
      return OperationResult::RateLimited;
    }

    if (!queueCanTxMessage(message.id, message.ext, message.rtr, message.len, message.data)) {
      return OperationResult::QueueFull;
    }

    logCanMessage('T', message);

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
    _canReady = false;
    _counters = CanCounters{};

    _resetPin = true;
    delay(200);
    _serial.reset();
    delay(100);
    _resetPin = false;
    
    _resetInterval.restart();
  }

  void handleConnectionState(serial_transport::ConnectionState state, serial_transport::Endpoint& /*serial*/) {
    if (state == serial_transport::ConnectionState::CLOSED) {
      _resetInterval.restart();
    }
    if (state != serial_transport::ConnectionState::CONNECTED) {
      _canReady = false;
    }

    iot_core::LogLevel level = state == serial_transport::ConnectionState::CLOSED ? iot_core::LogLevel::Warning : iot_core::LogLevel::Info;
    _logger.log(level, toolbox::format(F("Serial connection: %s"), serial_transport::describe(state).ref()));
  }

  void logFrame(char direction, uint8_t type, uint8_t sequenceNumber, const uint8_t* payload, uint8_t payloadLen) {
    bool isDataOrAck = (type == serial_transport::Endpoint::FRAME_TYPE_DATA) || (type == serial_transport::Endpoint::FRAME_TYPE_ACK);
    _logger.log(isDataOrAck ? iot_core::LogLevel::Trace : iot_core::LogLevel::Debug, [&] () {
      static char logMessage[96]; // "TX|RX FRAME type=XX seq=XX len=X ...";
      int insertPos = snprintf(logMessage, 96, "%cX FRAME T=%02X S=%02X L=%u ", direction, type, sequenceNumber, payloadLen);
      for (size_t i = 0; i < payloadLen; ++i) {
        logMessage[insertPos++] = payload[i];
      }
      logMessage[insertPos] = '\0';
      return logMessage;
    });
  }

  bool queueCanTxMessage(uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    return _serial.queue(toolbox::format(F("CANTX %08lX %u %02X %02X %02X %02X %02X %02X %02X %02X"),
      id | (uint32_t(ext) << 31) | (uint32_t(rtr) << 30),
      length,
      data[0],
      data[1],
      data[2],
      data[3],
      data[4],
      data[5],
      data[6],
      data[7]
    ));
  }

  void processReceived(const toolbox::strref& message, serial_transport::Endpoint& serial) {
    toolbox::strref part = message;
    toolbox::strref next;
    
    if (part.startsWith(F("CANRX "))) {
      part = part.skip(6);
      toolbox::Maybe<uint32_t> id = toolbox::convert<uint32_t>::fromString(part, &next, 16) ;
      if (!id) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, toolbox::format(F("CANRX: Invalid ID '%s'"), message.cstr()));
        return;
      }
      part = next;
      toolbox::Maybe<uint8_t> len = toolbox::convert<uint8_t>::fromString(part, &next, 10);
      if (!len || len.get() > 8u) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, toolbox::format(F("CANRX: Invalid length '%s'"), message.cstr()));
        return;
      }
      part = next;

      CanMessage canMessage;
      canMessage.id = id.get() & 0x1FFFFFFFu;
      canMessage.ext = (id.get() & 0x80000000u) != 0;
      canMessage.rtr = (id.get() & 0x40000000u) != 0;
      canMessage.len = len.get();
      for (uint8_t i = 0; i < canMessage.len; ++i) {
        toolbox::Maybe<uint8_t> byte = toolbox::convert<uint8_t>::fromString(part, &next, 16);
        if (!byte) {
          _counters.err += 1;
          _logger.log(iot_core::LogLevel::Error, toolbox::format(F("CANRX: Invalid data at index %u '%s'"), i, message.cstr()));
          return;
        }
        canMessage.data[i] = byte.get();
        part = next;
      }

      logCanMessage('R', canMessage);

      if (_messageHandler) _messageHandler(canMessage);

      _counters.rx += 1;
    } else if (part.startsWith(F("CANTX "))) {
      part = part.skip(6);
      if (part == F("OK")) {
        // Success - no action needed
      } else if (part == F("ENVAL")) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, F("CANTX ENVAL: Invalid CAN message format or parameters"));
      } else if (part == F("ESEND")) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, F("CANTX ESEND: CAN TX buffer full, message dropped"));
      } else if (part == F("ENOAV")) {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, F("CANTX ENOAV: CAN module not available"));
      } else {
        _counters.err += 1;
        _logger.log(iot_core::LogLevel::Error, [&] () { return toolbox::format(F("CANTX unknown error: %s"), message.cstr()); });
      }
    } else if (part == F("READY")) {
      serial.queue(toolbox::format(F("SETUP %X %s"), CAN_BITRATE, toSetupModeString(effectiveMode())));
    } else if (part.startsWith(F("SETUP "))) {
      part = part.skip(6);
      _canReady = part.startsWith(F("OK "));
      if (_canReady) {
        _logger.log(iot_core::LogLevel::Info, message.cstr());
        if (_readyHandler) _readyHandler();
      } else {
        _logger.log(iot_core::LogLevel::Error, message.cstr());
      }
    } else {
      _counters.err += 1;
      _logger.log(iot_core::LogLevel::Error, message.cstr());
    }
  }

  void logCanMessage(char direction, const CanMessage& message) {
    _logger.log(iot_core::LogLevel::Debug, [&] () {
      static char logMessage[36]; // "XX 123456789 xr 8 FFFFFFFFFFFFFFFF";
      int insertPos = 0;
      insertPos += snprintf(logMessage + insertPos, 36 - insertPos, "%cX %x ", direction, message.id);
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
