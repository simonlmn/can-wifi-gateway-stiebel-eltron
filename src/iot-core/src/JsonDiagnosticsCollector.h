#ifndef IOT_CORE_JSONDIAGNOSTICSCOLLECTOR_H_
#define IOT_CORE_JSONDIAGNOSTICSCOLLECTOR_H_

#include "Utils.h"

namespace iot_core {

template<typename JsonWriter>
class JsonDiagnosticsCollector final : public iot_core::IDiagnosticsCollector {
private:
  JsonWriter& _writer;

public:
  JsonDiagnosticsCollector(JsonWriter& writer) : _writer(writer) {
    _writer.openObject();
  };

  virtual void beginSection(const char* name) override {
    _writer.property(name);
    _writer.openObject();
  }

  virtual void addValue(const char* name, const char* value) {
    _writer.propertyString(name, value);
  }

  virtual void endSection() override {
    _writer.close();
  }
};

template<typename JsonWriter>
JsonDiagnosticsCollector<JsonWriter> makeJsonDiagnosticsCollector(JsonWriter& writer) { return {writer}; }

}

#endif
