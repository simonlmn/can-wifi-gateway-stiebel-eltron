#ifndef IOT_CORE_JSONDIAGNOSTICSCOLLECTOR_H_
#define IOT_CORE_JSONDIAGNOSTICSCOLLECTOR_H_

#include "Utils.h"

namespace iot_core {

template<typename JsonBuffer>
class JsonDiagnosticsCollector final : public iot_core::IDiagnosticsCollector {
private:
  JsonBuffer& _buffer;
  bool _sectionOpen = false;
  bool _hasValue = false;

public:
  JsonDiagnosticsCollector(JsonBuffer& buffer) : _buffer(buffer) {
    _buffer.jsonObjectOpen();
  };

  ~JsonDiagnosticsCollector() {
    if (_sectionOpen) {
      _buffer.jsonObjectClose();  
    }
    _buffer.jsonObjectClose();
  }

  virtual void addSection(const char* name) override {
    if (_sectionOpen) {
      _buffer.jsonObjectClose();
      _buffer.jsonSeparator();
    }

    _buffer.jsonPropertyStart(name);
    _buffer.jsonObjectOpen();
    _sectionOpen = true;
    _hasValue = false;
  }

  virtual void addValue(const char* name, const char* value) {
    if (_hasValue) {
      _buffer.jsonSeparator();
    }

    _buffer.jsonPropertyString(name, value);
    _hasValue = true;
  }
};

}

#endif
