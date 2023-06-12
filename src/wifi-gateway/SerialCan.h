#pragma once

#include "NodeBase.h"
#include "CanInterface.h"

class SerialCan final : public IConfigurable {
private:
  static const uint32_t CAN_BITRATE = 20UL * 1000UL; // 20 kbit/s
  static constexpr uint32_t MAX_ERR_COUNT = 5;

  NodeBase& _node;
  DigitalOutput& _resetPin;
  DigitalInput& _listenModePin;
  unsigned long _lastResetMs;
  bool _canAvailable;
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  
  CanMode _mode = CanMode::ListenOnly;
  CanLogLevel _logLevel = CanLogLevel::Status;
  CanCounters _counters;

public:
  SerialCan(NodeBase& node, DigitalOutput& resetPin, DigitalInput& listenModePin) :
    _node(node),
    _resetPin(resetPin),
    _listenModePin(listenModePin),
    _lastResetMs(0),
    _canAvailable(false),
    _counters()
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
    Serial.begin(57600);
    Serial.setTimeout(1);
    delay(100);
    _node.addConfigurable("can", this);
    reset();
  }

  void loop() {
    _logLevel = (CanLogLevel)_node.logLevel("can");
    readSerial();

    if (!ready() && (_lastResetMs + 30000) < millis()) {
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
      
      static char BUFFER[96];
      size_t length = snprintf(BUFFER, 96, "CANTX %u %u %u %u %u %u %u %u %u %u \r\n",
        message.id | (message.ext << 31) | (message.rtr << 30),
        message.len,
        message.data[0],
        message.data[1],
        message.data[2],
        message.data[3],
        message.data[4],
        message.data[5],
        message.data[6],
        message.data[7]
      );
      if (length > 96) {
        return;
      }

      Serial.write(BUFFER, length);
      
      _counters.tx += 1;
    }
  }

  CanCounters const& counters() const {
    return _counters;
  }

private:
  static const size_t RECEIVE_BUFFER_SIZE = 256;
  char _receiveBuffer[RECEIVE_BUFFER_SIZE];
  size_t _receiveIndex = 0u;

  void resetInternal() {
    _canAvailable = false;
    _receiveIndex = 0;
    _receiveBuffer[0] = '\0';
    _counters = CanCounters{};

    _resetPin.trigger(true, 100);

    _lastResetMs = millis();
  }
  
  void readSerial() {
    auto bytesAvailable = Serial.available();
    
    while (bytesAvailable > 0) {
      auto receivedByte = Serial.read();
      if (receivedByte == '\n' && _receiveIndex > 0 && _receiveBuffer[_receiveIndex - 1] == '\r') {
        // \r\n received, process line
        _receiveBuffer[_receiveIndex - 1] = '\0';
        String line (_receiveBuffer);
        
        processReceivedLine(line);
  
        _receiveIndex = 0;
      } else {
        _receiveBuffer[_receiveIndex] = receivedByte;
        _receiveIndex += 1;
      }

      _receiveIndex = _receiveIndex % RECEIVE_BUFFER_SIZE;
      
      bytesAvailable -= 1;
    }
  }

  void processReceivedLine(const String& line) {
    _node.lyield();
    
    if (line.length() < 5) {
       return;
    }

    auto type = line.substring(0, 5);
    if (type == "CANRX") {
      auto idEndIndex = line.indexOf(' ', 6);
      auto id = line.substring(6, idEndIndex).toInt();
      auto lenEndIndex = line.indexOf(' ', idEndIndex + 1);
      auto len = line.substring(idEndIndex + 1, lenEndIndex).toInt();
      
      CanMessage message;
      message.id = id & 0x1FFFFFFFu;
      message.ext = (id & 0x80000000u) != 0;
      message.rtr = (id & 0x40000000u) != 0;
      message.len = len;
  
      auto prevDataEndIndex = lenEndIndex;
      for (size_t i = 0; i < message.len; ++i) {
        auto dataEndIndex = line.indexOf(' ', prevDataEndIndex + 1);
        message.data[i] = line.substring(prevDataEndIndex + 1, dataEndIndex).toInt();
        prevDataEndIndex = dataEndIndex;
      }

      if (_logLevel >= CanLogLevel::Rx) {
        logCanMessage("RX", message);
      }

      if (_messageHandler) _messageHandler(message);

      _counters.rx += 1;
    } else if (type == "CANTX") {
      auto resultEndIndex = line.indexOf(' ', 6);
      auto result = line.substring(6, resultEndIndex);
  
      if (result != "OK") {
        _counters.err += 1;
        
        if (_logLevel >= CanLogLevel::Error) _node.log("can", line.c_str());
      }
    } else if (type == "READY") {
      Serial.print("SETUP ");
      Serial.print(CAN_BITRATE, DEC);
      switch (effectiveMode())
      {
      case CanMode::ListenOnly:
        Serial.println(" ListenOnlyMode ");
        break;
      case CanMode::Normal:
        Serial.println(" NormalMode ");
        break;
      default:
        Serial.println("  ");
        break;
      }
    } else if (type == "SETUP") {
      auto resultEndIndex = line.indexOf(' ', 6);
      auto result = line.substring(6, resultEndIndex);
      
      if (result == "OK") {
        if (_logLevel >= CanLogLevel::Status) _node.log("can", line.c_str());
        
        _canAvailable = true;
        if (_readyHandler) _readyHandler();
      } else {
        if (_logLevel >= CanLogLevel::Error) _node.log("can", line.c_str());
        
        _canAvailable = false;
      }
    } else {
      _counters.err += 1;
      
      if (_logLevel >= CanLogLevel::Error) _node.log("can", line.c_str());
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
};
