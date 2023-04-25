#pragma once

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <strings_en.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Arduino_JSON.h>
#include "Logger.h"
#include <map>
#include <functional>

// Enable measurement of chip's VCC
ADC_MODE(ADC_VCC);

class ConfigParser {
  static constexpr char SEPARATOR = '=';
  static constexpr char END = ';';
  char* _config;

public:
  ConfigParser(char* config) : _config(config) {}  
  
  bool parse(std::function<bool(const char*, const char*)> handler) const {
    char* start = _config;
    char* separator = nullptr;
    char* end = nullptr;

    while((separator = strchr(start, SEPARATOR)) != nullptr) {
      if ((end = strchr(separator, END)) == nullptr) {
        break;
      }

      *separator = '\0';
      *end = '\0';
      bool success = handler(start, separator + 1);
      *separator = SEPARATOR;
      *end = END;
        
      if (!success) {
        break;
      }

      start = end + 1;
    }

    return *start == '\0';
  }
};

enum struct ConnectionStatus {
  Disconnected,
  Connecting,
  Connected,
  Disconnecting,
};

class NodeBase {
  const char* _otaPassword;

  WiFiManager _wifiManager;

  JSONVar _diagnostics;

  std::map<String, std::function<bool(const char*, const char*)>> _configHandlers;

  std::map<String, int> _logLevels;
  Logger _logger;

  std::function<void(bool)> _setup;
  std::function<void(ConnectionStatus)> _loop;

  unsigned long _lastMillis = 0u;
  unsigned long _epoch = 0u;
  
  unsigned long _disconnectedSinceMs = 1u;
  unsigned long _disconnectedResetTimeoutMs = 1u * 60u * 1000u; // 1 minute

  bool _stopped;
  
public:
  NodeBase(const char* otaPassword)
    : _otaPassword(otaPassword), _wifiManager(), _diagnostics(), _configHandlers(), _logLevels(), _logger(), _setup(), _loop(), _stopped(false) {}

  void init(std::function<void(bool)> setup, std::function<void(ConnectionStatus)> loop) {
    _setup = setup;
    _loop = loop;
  }

  void setup() {
    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, LOW);

    LittleFS.begin();

    // Set static data which does not change during operation only once
    _diagnostics["chipId"] = String(ESP.getChipId(), HEX);
    _diagnostics["cpuFreq"] = ESP.getCpuFreqMHz();
    _diagnostics["resetReason"] = ESP.getResetReason();
    _diagnostics["sketchMD5"] = ESP.getSketchMD5();
  
    _wifiManager.setConfigPortalBlocking(false);
    bool connected = _wifiManager.autoConnect();

    setupOTA();
    
    log("node", "internal setup done.");

    _setup(connected);

    log("node", "all setup done.");
    
    digitalWrite(BUILTIN_LED, HIGH);
  }

  void loop() {
    auto currentMs = millis();

    updateTimeAndEpoch();

    lyield();
    
    if (connected()) {
      ConnectionStatus status = ConnectionStatus::Connected;      
      if (_disconnectedSinceMs > 0) {
        log("node", format(F("reconnected after %u ms."), currentMs - _disconnectedSinceMs));
        _disconnectedSinceMs = 0;
        status = ConnectionStatus::Connecting;
      }
      
      lyield();
      
      _loop(status);
    } else {
      ConnectionStatus status = ConnectionStatus::Disconnected;
      if (_disconnectedSinceMs == 0 || currentMs < _disconnectedSinceMs /* detect millis() wrap-around */) {
        _disconnectedSinceMs = currentMs;
        log("node", "disconnected.");
        status = ConnectionStatus::Disconnecting;
      }

      if (currentMs > _disconnectedSinceMs + _disconnectedResetTimeoutMs) {
        reset();
      }
      
      blinkSlow();
      
      _loop(status);
    }
  }

  void lyield() {
    yield();
    _wifiManager.process();
    ArduinoOTA.handle();
    yield();
  }

  void reset() {
    ESP.restart();
  }

  void stop() {
    if (_stopped) {
      return;
    }
    
    _stopped = true;
    _setup = [](bool) { };
    _loop = [this](ConnectionStatus) { blinkMedium(); };

    log("node", "STOP!");
  }

  bool connected() const {
    return WiFi.status() == WL_CONNECTED;
  }

  void addConfigHandler(String category, std::function<bool(const char*, const char*)> handler) {
    _configHandlers[category] = handler;
    // TODO send stored config directly?
  }

  bool configure(String category, ConfigParser const& config) {
    auto target = _configHandlers.find(category);
    if (target == _configHandlers.end()) {
      return false;
    }
    return config.parse(target->second);
    // TODO obtain and store new config from target?
  }

  int logLevel(String category) {
    return _logLevels[category];
  }

  void logLevel(String category, int level) {
    _logLevels[category] = level;
  }

  Logger& getLogger() { return _logger; }

  void log(const char* category, const char* message) {
    _logger.log(category, message);
    lyield();
  }

  const JSONVar& getDiagnostics() {
    _diagnostics["epoch"] = _epoch;
    _diagnostics["millis"] = millis();
    _diagnostics["chipVcc"] = ESP.getVcc() / 1000.0;
    _diagnostics["freeHeap"] = (unsigned long) ESP.getFreeHeap();
    _diagnostics["heapFragmentation"] = ESP.getHeapFragmentation();
    _diagnostics["maxFreeBlockSize"] = (long unsigned int)ESP.getMaxFreeBlockSize();
    _diagnostics["wifiRssi"] = WiFi.RSSI();
    _diagnostics["ip"] = WiFi.localIP().toString();
    return _diagnostics;
  }

private:
  void updateTimeAndEpoch() {
    auto currentMs = millis();
    if (currentMs < _lastMillis) {
      _epoch += 1;
    }
    _lastMillis = currentMs;
  }
  
  void setupOTA() {
    ArduinoOTA.setPassword(_otaPassword);
    ArduinoOTA.onStart([this] () { stop(); LittleFS.end(); log("node", "starting OTA update..."); digitalWrite(BUILTIN_LED, HIGH); });
    ArduinoOTA.onEnd([this] () { digitalWrite(BUILTIN_LED, LOW); log("node", "OTA update finished."); });
    ArduinoOTA.onProgress([] (unsigned int progress, unsigned int total) { digitalWrite(BUILTIN_LED, LOW); delay(10); digitalWrite(BUILTIN_LED, HIGH); });
    ArduinoOTA.begin();  
  }

  void blinkSlow() {
    static bool lastToggleState = false;
    static unsigned long lastToggleMs = 0ul;

    auto currentMs = millis();
    if (currentMs > lastToggleMs + 1000ul) {
      digitalWrite(BUILTIN_LED, lastToggleState ? HIGH : LOW);
      lastToggleState = !lastToggleState;
      lastToggleMs = currentMs;
    }
  }

  void blinkMedium() {
    static bool lastToggleState = false;
    static unsigned long lastToggleMs = 0ul;

    auto currentMs = millis();
    if (currentMs > lastToggleMs + 500ul) {
      digitalWrite(BUILTIN_LED, lastToggleState ? HIGH : LOW);
      lastToggleState = !lastToggleState;
      lastToggleMs = currentMs;
    }
  }
};
