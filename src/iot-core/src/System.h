#ifndef IOT_CORE_SYSTEM_H_
#define IOT_CORE_SYSTEM_H_

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "pins/DigitalInput.h"
#include "pins/DigitalOutput.h"
#include "Interfaces.h"
#include "IDateTimeSource.h"
#include "Config.h"
#include "Logger.h"
#include "DateTime.h"
#include "Utils.h"
#include <vector>

// Enable measurement of chip's VCC
ADC_MODE(ADC_VCC);

namespace iot_core {

class System final : public ISystem, public IApplicationContainer {
  static const unsigned long FACTORY_RESET_TRIGGER_TIME = 5000ul; // 5 seconds
  static const unsigned long DISCONNECTED_RESET_TIMEOUT = 300000ul; // 5 minutes
  
  char _chipId[9];
  bool _stopped = false;
  Time _uptime = {};
  unsigned long _disconnectedSinceMs = 1u;
  ConnectionStatus _status = ConnectionStatus::Disconnected;
  const IDateTimeSource* _dateTimeSource = &NO_DATE_TIME_SOURCE;
  Logger _logger;
  WiFiManager _wifiManager {};
  std::vector<IApplicationComponent*> _components {};

  const char* _otaPassword;
  pins::DigitalOutput& _statusLedPin;
  pins::DigitalInput& _otaEnablePin;
  pins::DigitalInput& _updatePin;
  pins::DigitalInput& _factoryResetPin;

  TimingStatistics<20> _yieldTiming {};
  ConstStrMap<TimingStatistics<10>> _componentTiming {};

public:
  System(const char* otaPassword, pins::DigitalOutput& statusLedPin, pins::DigitalInput& otaEnablePin, pins::DigitalInput& updatePin, pins::DigitalInput& factoryResetPin)
    : _logger(_uptime),
    _otaPassword(otaPassword),
    _statusLedPin(statusLedPin),
    _otaEnablePin(otaEnablePin),
    _updatePin(updatePin),
    _factoryResetPin(factoryResetPin)
  {
    str(format("%x", ESP.getChipId())).copy(_chipId, 8);
  }

  const char* id() const override {
    return _chipId;
  }

  void addComponent(IApplicationComponent* component) override {
    _components.emplace_back(component);
    _componentTiming[component->name()] = {};
  }

  void setup() {
#ifdef DEVELOPMENT_MODE
    _logger.log(LogLevel::Warning, "sys", "DEVELOPMENT MODE");
#endif
    _statusLedPin = true;
    LittleFS.begin();

    _wifiManager.setConfigPortalBlocking(false);
    _wifiManager.setWiFiAutoReconnect(true);
    bool connected = _wifiManager.autoConnect();

    if (_otaEnablePin) {
      setupOTA();
    }
    
    _logger.log(LogLevel::Info, "sys", F("Internal setup done."));

    for (auto component : _components) {
      restoreConfiguration(component);
      component->setup(connected);
    }

    _logger.log(LogLevel::Info, "sys", F("All setup done."));
    
    _statusLedPin = false;
  }

  void loop() {
    _yieldTiming.start();

    _uptime.update();

    lyield();

    if (_factoryResetPin && _factoryResetPin.hasNotChangedFor(FACTORY_RESET_TRIGGER_TIME)) {
      factoryReset();
    }
    
    if (connected()) {
      _status = ConnectionStatus::Connected;      
      if (_disconnectedSinceMs > 0) {
        _logger.log(LogLevel::Info, "sys", format(F("Reconnected after %u ms."), _uptime.millis() - _disconnectedSinceMs));
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
        loopComponents();
      }
    } else {
      _status = ConnectionStatus::Disconnected;
      if (_disconnectedSinceMs == 0 || _uptime.millis() < _disconnectedSinceMs /* detect millis() wrap-around */) {
        _disconnectedSinceMs = _uptime.millis();
        _logger.log(LogLevel::Warning, "sys", F("Disconnected."));
        _status = ConnectionStatus::Disconnecting;
      }

      if (_wifiManager.getWiFiIsSaved() && _uptime.millis() > _disconnectedSinceMs + DISCONNECTED_RESET_TIMEOUT) {
        reset();
      }
      
      if (_stopped) {
        blinkMedium();
      } else {
        blinkSlow();
      
        loopComponents();
      }
    }

    _yieldTiming.stop();
  }

  void lyield() override {
    _yieldTiming.stop();
    yield();
    _wifiManager.process();
    if (_otaEnablePin) {
      ArduinoOTA.handle();
    }
    yield();
    _yieldTiming.start();
  }

  void reset() override {
    ESP.restart();
  }

  void stop() override {
    if (_stopped) {
      return;
    }
    
    _stopped = true;

    _logger.log(LogLevel::Info, "sys", F("STOP!"));
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

  DateTime const& currentDateTime() const override {
    return _dateTimeSource->currentDateTime();
  }

  void setDateTimeSource(const IDateTimeSource* dateTimeSource) {
    _dateTimeSource = dateTimeSource;
  }

  bool configure(const char* category, IConfigParser const& config) override {
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

  bool configureAll(IConfigParser const& config) override {
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
    collector.beginSection("system");
    collector.addValue("chipId", id());
    collector.addValue("flashChipId", format("%x", ESP.getFlashChipId()));
    collector.addValue("sketchMD5", ESP.getSketchMD5().c_str());
    collector.addValue("coreVersion", ESP.getCoreVersion().c_str());
    collector.addValue("sdkVersion", ESP.getSdkVersion());
    collector.addValue("cpuFreq", format("%u", ESP.getCpuFreqMHz()));
    collector.addValue("chipVcc", format("%1.2f", ESP.getVcc() / 1000.0));
    collector.addValue("resetReason", ESP.getResetReason().c_str());
    collector.addValue("uptime", _uptime.format());
    collector.addValue("freeHeap", format("%u", ESP.getFreeHeap()));
    collector.addValue("heapFragmentation", format("%u", ESP.getHeapFragmentation()));
    collector.addValue("maxFreeBlockSize", format("%u", ESP.getMaxFreeBlockSize()));
    collector.addValue("wifiRssi", format("%i", WiFi.RSSI()));
    collector.addValue("ip", WiFi.localIP().toString().c_str());

    collector.beginSection("timing");

    collector.beginSection("yield");
    collector.addValue("count", convert<size_t>::toString(_yieldTiming.count(), 10));
    collector.addValue("avg", convert<unsigned long>::toString(_yieldTiming.avg(), 10));
    collector.addValue("min", convert<unsigned long>::toString(_yieldTiming.min(), 10));
    collector.addValue("max", convert<unsigned long>::toString(_yieldTiming.max(), 10));
    collector.endSection();

    for (auto& [componentName, timing] : _componentTiming) {
      collector.beginSection(componentName);
      collector.addValue("count", convert<size_t>::toString(timing.count(), 10));
      collector.addValue("avg", convert<unsigned long>::toString(timing.avg(), 10));
      collector.addValue("min", convert<unsigned long>::toString(timing.min(), 10));
      collector.addValue("max", convert<unsigned long>::toString(timing.max(), 10));
      collector.endSection();
    }

    collector.endSection();
    collector.endSection();

    for (auto component : _components) {
      collector.beginSection(component->name());
      component->getDiagnostics(collector);
      collector.endSection();
    }
  }

private:
  void loopComponents() {
    for (auto component : _components) {
      auto& timing = _componentTiming[component->name()];
      timing.start();
      component->loop(_status);
      timing.stop();
      lyield();
    }
  }

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

  void setupOTA() {
    ArduinoOTA.setPassword(_otaPassword);
    ArduinoOTA.onStart([this] () { stop(); LittleFS.end(); _logger.log(LogLevel::Info, "sys", F("Starting OTA update...")); _statusLedPin = true; });
    ArduinoOTA.onEnd([this] () { _statusLedPin = false; _logger.log(LogLevel::Info, "sys", F("OTA update finished.")); });
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
      _logger.log(LogLevel::Info, "sys", format(F("Restored config for '%s'."), configurable->name()));
    } else {
      _logger.log(LogLevel::Error, "sys", format(F("failed to restore config for '%s'."), configurable->name()));
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

}

#endif
