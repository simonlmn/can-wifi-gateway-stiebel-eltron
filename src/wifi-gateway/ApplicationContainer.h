#pragma once

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <strings_en.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "src/shared/Pins.h"
#include "Config.h"
#include "Logger.h"
#include <vector>
#include <functional>

// Enable measurement of chip's VCC
ADC_MODE(ADC_VCC);

enum struct ConnectionStatus {
  Disconnected,
  Reconnected,
  Connected,
  Disconnecting,
};

class ISystem {
public:
  virtual void reset() = 0;
  virtual void stop() = 0;
  virtual void factoryReset() = 0;
  virtual ConnectionStatus connectionStatus() const = 0;
  virtual bool connected() const = 0;
  virtual Logger& logger() = 0;
  virtual void lyield() = 0;
};

class IDiagnosticsCollector {
public:
  virtual void addSection(const char* name) = 0;
  virtual void addValue(const char* name, const char* value) = 0;
};

class IDiagnosticsProvider {
public:
  virtual void getDiagnostics(IDiagnosticsCollector& collector) const = 0;
};

class IApplicationContainer : public IDiagnosticsProvider {
public:
  virtual bool configure(const char* category, ConfigParser const& config) = 0;
  virtual void getConfig(const char* category, std::function<void(const char*, const char*)> writer) const = 0;
  virtual bool configureAll(ConfigParser const& config) = 0;
  virtual void getAllConfig(std::function<void(const char*, const char*)> writer) const = 0;
};

class IApplicationComponent : public IConfigurable, public IDiagnosticsProvider {
public:
  virtual const char* name() const = 0;
  virtual void setup(bool connected) = 0;
  virtual void loop(ConnectionStatus status) = 0;
};

class System final : public ISystem, public IApplicationContainer {
  Logger _logger {};
  WiFiManager _wifiManager {};
  std::vector<IApplicationComponent*> _components {};

  const char* _otaPassword;
  DigitalOutput& _statusLedPin;
  DigitalInput& _otaEnablePin;
  DigitalInput& _updatePin;
  DigitalInput& _factoryResetPin;

  bool _stopped = false;

  unsigned long _epoch = 0u;
  unsigned long _lastMillis = 0u;
  unsigned long _disconnectedSinceMs = 1u;
  unsigned long _disconnectedResetTimeoutMs = 1u * 60u * 1000u; // 1 minute

  ConnectionStatus _status = ConnectionStatus::Disconnected;

public:
  System(const char* otaPassword, DigitalOutput& statusLedPin, DigitalInput& otaEnablePin, DigitalInput& updatePin, DigitalInput& factoryResetPin)
    : _otaPassword(otaPassword),
    _statusLedPin(statusLedPin),
    _otaEnablePin(otaEnablePin),
    _updatePin(updatePin),
    _factoryResetPin(factoryResetPin)
  {}

  void addComponent(IApplicationComponent* component) {
    _components.emplace_back(component);
  }

  void setup() {
#ifdef DEVELOPMENT_MODE
    _logger.log("sys", "DEVELOPMENT MODE");
#endif
    _statusLedPin = true;
    LittleFS.begin();

    _wifiManager.setConfigPortalBlocking(false);
    bool connected = _wifiManager.autoConnect();

    if (_otaEnablePin) {
      setupOTA();
    }
    
    _logger.log("sys", "internal setup done.");

    for (auto component : _components) {
      restoreConfiguration(component);
      component->setup(connected);
    }

    _logger.log("sys", "all setup done.");
    
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
      _status = ConnectionStatus::Connected;      
      if (_disconnectedSinceMs > 0) {
        _logger.log("sys", format(F("reconnected after %u ms."), currentMs - _disconnectedSinceMs));
        _disconnectedSinceMs = 0;
        _status = ConnectionStatus::Reconnected;
      }

      if (_stopped) {
        blinkMedium();
      } else {
#ifdef DEVELOPMENT_MODE
        blinkFast();
#else
        _statusLedPin = false;
#endif
        for (auto component : _components) {
          component->loop(_status);
          lyield();
        }
      }
    } else {
      _status = ConnectionStatus::Disconnected;
      if (_disconnectedSinceMs == 0 || currentMs < _disconnectedSinceMs /* detect millis() wrap-around */) {
        _disconnectedSinceMs = currentMs;
        _logger.log("sys", "disconnected.");
        _status = ConnectionStatus::Disconnecting;
      }

      if (currentMs > _disconnectedSinceMs + _disconnectedResetTimeoutMs) {
        reset();
      }
      
      if (_stopped) {
        blinkMedium();
      } else {
        blinkSlow();
      
        for (auto component : _components) {
          component->loop(_status);
          lyield();
        }
      }
    }
  }

  void lyield() override {
    yield();
    _wifiManager.process();
    if (_otaEnablePin) {
      ArduinoOTA.handle();
    }
    yield();
  }

  void reset() override {
    ESP.restart();
  }

  void stop() override {
    if (_stopped) {
      return;
    }
    
    _stopped = true;

    _logger.log("sys", "STOP!");
  }

  void factoryReset() override {
    LittleFS.format();
    _wifiManager.erase(true);
    reset();
  }

  bool connected() const override {
    return WiFi.status() == WL_CONNECTED;
  }

  ConnectionStatus connectionStatus() const override {
    return _status;
  }

  bool configure(const char* category, ConfigParser const& config) override {
    auto component = findComponentByName(category);
    if (component == nullptr) {
      return false;
    }
    if (config.parse([&] (char* name, const char* value) { return component->configure(name, value); })) {
      persistConfiguration(component);
      return true;
    } else {
      return false;
    }
  }

  void getConfig(const char* category, std::function<void(const char*, const char*)> writer) const override {
    auto component = findComponentByName(category);
    if (component == nullptr) {
      return;
    }

    component->getConfig(writer);
  }

  bool configureAll(ConfigParser const& config) override {
    if (config.parse([this] (char* path, const char* value) {
      auto categoryEnd = strchr(path, '.');
      if (categoryEnd == nullptr) {
        return false;
      }
      *categoryEnd = '\0';
      auto category = path;
            
      auto component = findComponentByName(category);
      *categoryEnd = '.';

      if (component == nullptr) {
        return false;
      } else {
        auto name = categoryEnd + 1;
        return component->configure(name, value);
      }
    })) {
      persistAllConfigurations();
      return true;
    } else {
      return false;
    }
  }

  void getAllConfig(std::function<void(const char*, const char*)> writer) const override {
    for (auto component : _components) {
      component->getConfig([&] (const char* name, const char* value) {
        writer(format("%s.%s", component->name(), name), value);
      });  
    }
  }

  Logger& logger() override { return _logger; }

  void getDiagnostics(IDiagnosticsCollector& collector) const override {
    collector.addSection("system");
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

    for (auto component : _components) {
      collector.addSection(component->name());
      component->getDiagnostics(collector);
    }
  }

private:
  IApplicationComponent* findComponentByName(const char* name) {
    for (auto component : _components) {
      if (strcmp(component->name(), name) == 0) {
        return component;
      }
    }
    return nullptr;
  }

  IApplicationComponent const* findComponentByName(const char* name) const {
    for (auto component : _components) {
      if (strcmp(component->name(), name) == 0) {
        return component;
      }
    }
    return nullptr;
  }

  void updateTimeAndEpoch() {
    auto currentMs = millis();
    if (currentMs < _lastMillis) {
      _epoch += 1;
    }
    _lastMillis = currentMs;
  }
  
  void setupOTA() {
    ArduinoOTA.setPassword(_otaPassword);
    ArduinoOTA.onStart([this] () { stop(); LittleFS.end(); _logger.log("sys", "starting OTA update..."); _statusLedPin = true; });
    ArduinoOTA.onEnd([this] () { _statusLedPin = false; _logger.log("sys", "OTA update finished."); });
    ArduinoOTA.onProgress([this] (unsigned int /*progress*/, unsigned int /*total*/) { _statusLedPin.trigger(true, 10); });
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

  void restoreConfiguration(IConfigurable* configurable) {
    ConfigParser parser = readConfigFile(format("/config/%s", configurable->name()));
    if (parser.parse([&] (char* name, const char* value) { return configurable->configure(name, value); })) {
      _logger.log("sys", format("restored config for '%s'.", configurable->name()));
    } else {
      _logger.log("sys", format("failed to restore config for '%s'.", configurable->name()));
    }
  }

  void persistConfiguration(IConfigurable* configurable) {
    writeConfigFile(format("/config/%s", configurable->name()), configurable);
  }

  void persistAllConfigurations() {
    for (auto component : _components) {
      persistConfiguration(component);
    }
  }
};
