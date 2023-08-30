#ifndef IOT_CORE_JSONWRITER_H_
#define IOT_CORE_JSONWRITER_H_

#include "Utils.h"

namespace iot_core {

/**
 * Class template for writing JSON documents to a "Print-like" output with a
 * stream-oriented interface. It does not construct/store the whole document
 * before writing it which saves on allocating any buffer or dynamic memory.
 * 
 * The output has to support only two methods:
 *   - size_t write(char)
 *   - size_t write(iot_core::ConstString<T>)
 */
template<typename Output>
class JsonWriter final {
  Output& _output;
  bool _failed = false;

public:
  JsonWriter(Output& output) : _output(output) {}

  bool failed() const { return _failed; }

  void objectOpen() {
    _failed = _failed || (_output.write('{') != 1u);
  }

  void objectClose() {
    _failed = _failed || (_output.write('}') != 1u);
  }

  void listOpen() {
    _failed = _failed || (_output.write('[') != 1u);
  }

  void listClose() {
    _failed = _failed || (_output.write(']') != 1u);
  }

  template<typename X>
  void propertyStart(X name) { propertyStart(iot_core::str(name)); }
  
  template<typename U>
  void propertyStart(iot_core::ConstString<U> name) {
    string(name);
    _failed = _failed || (_output.write(':') != 1u);
  }

  template<typename X, typename Y>
  void propertyRaw(X name, Y value) { propertyRaw(iot_core::str(name), iot_core::str(value)); }
  
  template<typename U, typename V>
  void propertyRaw(iot_core::ConstString<U> name, iot_core::ConstString<V> value) {
    propertyStart(name);
    _failed = _failed || (_output.write(value) != value.len());
  }

  template<typename X, typename Y>
  void propertyString(X name, Y value) { propertyString(iot_core::str(name), iot_core::str(value)); }
  
  template<typename U, typename V>
  void propertyString(iot_core::ConstString<U> name, iot_core::ConstString<V> value) {
    propertyStart(name);
    string(value);
  }

  template<typename X>
  void string(X value) { string(iot_core::str(value)); }
  
  template<typename V>
  void string(iot_core::ConstString<V> value) {
    _failed = _failed || (_output.write('"') != 1u);
    _failed = _failed || (_output.write(value) != value.len());
    _failed = _failed || (_output.write('"') != 1u);
  }

  void separator() {
    _failed = _failed || (_output.write(',') != 1u);
  }
};

template<typename Output>
JsonWriter<Output> makeJsonWriter(Output& output) { return {output}; }

}

#endif
