#ifndef IOT_CORE_JSONDIAGNOSTICSCOLLECTOR_H_
#define IOT_CORE_JSONDIAGNOSTICSCOLLECTOR_H_

#include "Utils.h"

namespace iot_core {

template<typename JsonWriter>
class JsonDiagnosticsCollector final : public iot_core::IDiagnosticsCollector {
private:
  JsonWriter& _writer;
  bool _sectionOpen = false;
  bool _hasValue = false;

public:
  JsonDiagnosticsCollector(JsonWriter& writer) : _writer(writer) {
    _writer.objectOpen();
  };

  ~JsonDiagnosticsCollector() {
    if (_sectionOpen) {
      _writer.objectClose();  
    }
    _writer.objectClose();
  }

  virtual void addSection(const char* name) override {
    if (_sectionOpen) {
      _writer.objectClose();
      _writer.separator();
    }

    _writer.propertyStart(name);
    _writer.objectOpen();
    _sectionOpen = true;
    _hasValue = false;
  }

  virtual void addValue(const char* name, const char* value) {
    if (_hasValue) {
      _writer.separator();
    }

    _writer.propertyString(name, value);
    _hasValue = true;
  }
};

template<typename JsonWriter>
JsonDiagnosticsCollector<JsonWriter> makeJsonDiagnosticsCollector(JsonWriter& writer) { return {writer}; }

}

#endif
