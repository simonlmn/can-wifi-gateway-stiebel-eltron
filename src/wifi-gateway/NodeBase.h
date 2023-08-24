#pragma once

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <strings_en.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "src/shared/Pins.h"
#include "Config.h"
#include "Logger.h"
#include <map>
#include <functional>

// Enable measurement of chip's VCC
ADC_MODE(ADC_VCC);

class IDiagnosticsCollector {
public:
  virtual void addSection(const char* name) = 0;
  virtual void addValue(const char* name, const char* value) = 0;
};

enum struct ConnectionStatus {
  Disconnected,
  Connecting,
  Connected,
  Disconnecting,
};

class NodeBase {
  const char* _otaPassword;

  DigitalOutput& _statusLedPin;
  DigitalInput& _otaEnablePin;
  DigitalInput& _updatePin;
  DigitalInput& _factoryResetPin;

  WiFiManager _wifiManager;

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
  NodeBase(const char* otaPassword, DigitalOutput& statusLedPin, DigitalInput& otaEnablePin, DigitalInput& updatePin, DigitalInput& factoryResetPin)
    : _otaPassword(otaPassword),
    _statusLedPin(statusLedPin),
    _otaEnablePin(otaEnablePin),
    _updatePin(updatePin),
    _factoryResetPin(factoryResetPin),
    _wifiManager(),
    _configurables(),
    _logLevels(),
    _logger(),
    _setup(),
    _loop(),
    _stopped(false)
  {}

  void init(std::function<void(bool)> setup, std::function<void(ConnectionStatus)> loop) {
    _setup = setup;
    _loop = loop;
  }

  void setup() {
#ifdef DEVELOPMENT_MODE
    log("node", "DEVELOPMENT MODE");
#endif
    _statusLedPin = true;
    LittleFS.begin();

    _wifiManager.setConfigPortalBlocking(false);
    bool connected = _wifiManager.autoConnect();

    if (_otaEnablePin) {
      setupOTA();
    }
    
    log("node", "internal setup done.");

    _setup(connected);

    log("node", "all setup done.");
    
    _statusLedPin = false;
  }

  void loop() {
    auto currentMs = millis();

    updateTimeAndEpoch();

    lyield();

    if (_factoryResetPin && _factoryResetPin.hasNotChangedFor(5000ul)) {
      factoryReset();
    }
    
    if (connected()) {
      ConnectionStatus status = ConnectionStatus::Connected;      
      if (_disconnectedSinceMs > 0) {
        log("node", format(F("reconnected after %u ms."), currentMs - _disconnectedSinceMs));
        _disconnectedSinceMs = 0;
        status = ConnectionStatus::Connecting;
      }
      
      _statusLedPin = false;

      lyield();

#ifdef DEVELOPMENT_MODE
      blinkFast();
#endif
      
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
    if (_otaEnablePin) {
      ArduinoOTA.handle();
    }
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

  void factoryReset() {
    LittleFS.format();
    _wifiManager.erase(true);
    reset();
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
    if (config.parse([&] (char* name, const char* value) { return configurable->second->configure(name, value); })) {
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

  bool configureAll(ConfigParser const& config) {
    if (config.parse([this] (char* path, const char* value) {
      auto categoryEnd = strchr(path, '.');
      if (categoryEnd == nullptr) {
        return false;
      }
      *categoryEnd = '\0';
      auto category = path;
            
      auto configurable = _configurables.find(category);
      *categoryEnd = '.';

      if (configurable == _configurables.end()) {
        return false;
      } else {
        auto name = categoryEnd + 1;
        return configurable->second->configure(name, value);
      }
    })) {
      persistAllConfigurations();
      return true;
    } else {
      return false;
    }
  }

  void getAllConfig(std::function<void(const char*, const char*)> writer) const {
    for (auto &&configurable : _configurables) {
      configurable.second->getConfig([&] (const char* name, const char* value) {
        writer(format("%s.%s", configurable.first.c_str(), name), value);
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

  void getDiagnostics(IDiagnosticsCollector& collector) const {
    collector.addSection("node");
    collector.addValue("chipId", format("%x", ESP.getChipId()));
    collector.addValue("flashChipId", format("%x", ESP.getFlashChipId()));
    collector.addValue("sketchMD5", ESP.getSketchMD5().c_str());
    collector.addValue("coreVersion", ESP.getCoreVersion().c_str());
    collector.addValue("sdkVersion", ESP.getSdkVersion());
    collector.addValue("cpuFreq", format("%u", ESP.getCpuFreqMHz()));
    collector.addValue("chipVcc", format("%1.2f", ESP.getVcc() / 1000.0));
    collector.addValue("resetReason", ESP.getResetReason().c_str());
    collector.addValue("epoch", format("%lu", _epoch));
    collector.addValue("millis", format("%lu", millis()));
    collector.addValue("freeHeap", format("%u", ESP.getFreeHeap()));
    collector.addValue("heapFragmentation", format("%u", ESP.getHeapFragmentation()));
    collector.addValue("maxFreeBlockSize", format("%u", ESP.getMaxFreeBlockSize()));
    collector.addValue("wifiRssi", format("%i", WiFi.RSSI()));
    collector.addValue("ip", WiFi.localIP().toString().c_str());
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
    ArduinoOTA.onStart([this] () { stop(); LittleFS.end(); log("node", "starting OTA update..."); _statusLedPin = true; });
    ArduinoOTA.onEnd([this] () { _statusLedPin = false; log("node", "OTA update finished."); });
    ArduinoOTA.onProgress([this] (unsigned int progress, unsigned int total) { _statusLedPin.trigger(true, 10); });
    ArduinoOTA.begin();
  }

  void blinkSlow() {
    _statusLedPin.toggleIfUnchangedFor(1000ul);
  }

  void blinkMedium() {
    _statusLedPin.toggleIfUnchangedFor(500ul);
  }

  void blinkFast() {
    _statusLedPin.toggleIfUnchangedFor(250ul);
  }

  void restoreConfiguration(String category) {
    auto configurable = _configurables.find(category);
    if (configurable == _configurables.end()) {
      log("node", format("restore config failed: '%s' is not configurable.", category.c_str()));
      return;
    }

    ConfigParser parser = readConfigFile(format("/config/%s", category.c_str()));
    if (parser.parse([&] (char* name, const char* value) { return configurable->second->configure(name, value); })) {
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

  void persistAllConfigurations() {
    for (auto &&configurable : _configurables) {
      persistConfiguration(configurable.first);
    }
  }
};
