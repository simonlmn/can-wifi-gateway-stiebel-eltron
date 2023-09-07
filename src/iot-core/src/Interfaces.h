#ifndef IOT_CORE_INTERFACES_H_
#define IOT_CORE_INTERFACES_H_

#include "Logger.h"
#include "DateTime.h"
#include "VersionInfo.h"
#include <functional>

namespace iot_core {

enum struct ConnectionStatus {
  Disconnected,
  Reconnected,
  Connected,
  Disconnecting,
};

class ISystem {
public:
  virtual const char* id() const = 0;
  virtual void reset() = 0;
  virtual void stop() = 0;
  virtual void factoryReset() = 0;
  virtual ConnectionStatus connectionStatus() const = 0;
  virtual bool connected() const = 0;
  virtual Logger& logger() = 0;
  virtual void lyield() = 0;
  virtual DateTime const& currentDateTime() const = 0;
  virtual void schedule(std::function<void()> function) = 0;
};

class IDiagnosticsCollector {
public:
  virtual void beginSection(const char* name) = 0;
  virtual void addValue(const char* name, const char* value) = 0;
  virtual void endSection() = 0;
};

class IDiagnosticsProvider {
public:
  virtual void getDiagnostics(IDiagnosticsCollector& collector) const = 0;
};

struct IConfigurable {
  virtual const char* name() const = 0;
  virtual bool configure(const char* name, const char* value) = 0;
  virtual void getConfig(std::function<void(const char*, const char*)> writer) const = 0;
};

class IConfigParser {
public:
  virtual bool parse(std::function<bool(char* name, const char* value)> processEntry) const = 0;
};

class IApplicationComponent : public IConfigurable, public IDiagnosticsProvider {
public:
  virtual const char* name() const = 0;
  virtual void setup(bool connected) = 0;
  virtual void loop(ConnectionStatus status) = 0;
};

class IApplicationContainer : public IDiagnosticsProvider {
public:
  virtual const VersionInfo& version() const = 0;
  virtual void addComponent(IApplicationComponent* component) = 0;
  virtual bool configure(const char* category, IConfigParser const& config) = 0;
  virtual void getConfig(const char* category, std::function<void(const char*, const char*)> writer) const = 0;
  virtual bool configureAll(IConfigParser const& config) = 0;
  virtual void getAllConfig(std::function<void(const char*, const char*)> writer) const = 0;
};

}

#endif
