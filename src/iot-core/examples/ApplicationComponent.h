#ifndef APPLICATIONCOMPONENT_H_
#define APPLICATIONCOMPONENT_H_

#include "iot-core/Interfaces.h"

/**
 * Empty implementation of the IApplicationComponent to use as a copy-paste
 * template for actual implementations.
 */
class ApplicationComponent final : public iot_core::IApplicationComponent {
private:
  iot_core::Logger& _logger;
  iot_core::ISystem& _system;

public:
  ApplicationComponent(iot_core::ISystem& system) :
    _logger(system.logger()),
    _system(system)
  {
  }

  const char* name() const override {
    return "___";
  }

  bool configure(const char* /*name*/, const char* /*value*/) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> /*writer*/) const override {
  }

  void setup(bool /*connected*/) override {
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& /*collector*/) const override {
  }
};

#endif
