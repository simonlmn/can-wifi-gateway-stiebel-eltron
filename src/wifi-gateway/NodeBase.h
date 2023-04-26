#pragma once

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <strings_en.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Arduino_JSON.h>
#include "Config.h"
#include "Logger.h"
#include <map>
#include <functional>

// Enable measurement of chip's VCC
ADC_MODE(ADC_VCC);

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

  std::map<String, IConfigurable*> _configurables;

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
    : _otaPassword(otaPassword), _wifiManager(), _diagnostics(), _configurables(), _logLevels(), _logger(), _setup(), _loop(), _stopped(false) {}

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

  void addConfigurable(String category, IConfigurable* configurable) {
    _configurables[category] = configurable;
    restoreConfiguration(category);
  }

  bool configure(String category, ConfigParser const& config) {
    auto configurable = _configurables.find(category);
    if (configurable == _configurables.end()) {
      return false;
    }
    if (config.parse(configurable->second)) {
      persistConfiguration(category);
      return true;
    } else {
      return false;
    }
  }

  void getConfig(String category, std::function<void(const char*, const char*)> writer) const {
    auto configurable = _configurables.find(category);
    if (configurable == _configurables.end()) {
      return;
    }

    configurable->second->getConfig(writer);
  }

  void getAllConfig(std::function<void(const char*, const char*, const char*)> writer) const {
    for (auto &&configurable : _configurables) {
      configurable.second->getConfig([&] (const char* name, const char* value) {
        writer(configurable.first.c_str(), name, value);
      });  
    }
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

  void restoreConfiguration(String category) {
    auto configurable = _configurables.find(category);
    if (configurable == _configurables.end()) {
      log("node", format("restore config failed: '%s' is not configurable.", category.c_str()));
      return;
    }

    ConfigParser parser = readConfigFile(format("/config/%s", category.c_str()));
    if (parser.parse(configurable->second)) {
      log("node", format("restored config for '%s'.", category.c_str()));
    } else {
      log("node", format("failed to restore config for '%s'.", category.c_str()));
    }
  }

  void persistConfiguration(String category) {
    auto configurable = _configurables.find(category);
    if (configurable == _configurables.end()) {
      log("node", format("persist config failed: '%s' is not configurable.", category.c_str()));
      return;
    }

    writeConfigFile(format("/config/%s", category.c_str()), configurable->second);
  }
};
