#ifndef IOT_CORE_JSONWRITER_H_
#define IOT_CORE_JSONWRITER_H_

#include "Utils.h"

namespace iot_core {

/**
 * Class template for writing JSON documents to a "Print-like" output with a
 * stream-oriented interface. It does not construct/store the whole document
 * before writing it which saves on allocating any buffer or dynamic memory.
 * 
 * It ensures that only valid JSON documents are constructed. If an invalid
 * operation is performed (e.g. defining a property inside a list), output
 * is aborted and the writer is marked as failed (reported by failed()).
 * 
 * The output has to support only two methods:
 *   - size_t write(char)
 *   - size_t write(T)
 *     where T is whatever is passed to the functions accepting a name or
 *     value. Typically "const char*" or "const __FlashStringHelper*"
 */
template<typename Output>
class JsonWriter final {
  // JSON literals
  static const char SEPARATOR = ',';
  static const char OBJECT_BEGIN = '{';
  static const char OBJECT_END = '}';
  static const char LIST_BEGIN = '[';
  static const char LIST_END = ']';
  static const char STRING_BEGIN = '"';
  static const char STRING_END = '"';
  static const char PROPERTY_BEGIN = '"';
  static const char PROPERTY_END = '"';
  static const char PROPERTY_VALUE_SEPARATOR = ':';

  // Operations
  static const uint8_t NOTHING = 0;
  static const uint8_t INSERT_VALUE = 1 << 0;
  static const uint8_t INSERT_STRING = 1 << 1;
  static const uint8_t OPEN_LIST = 1 << 2;
  static const uint8_t OPEN_OBJECT = 1 << 3;
  static const uint8_t START_PROPERTY = 1 << 4;
  static const uint8_t CLOSE = 1 << 5;

  // States of data structures
  enum struct DataStructure : uint8_t {
    None = 0,
    EmptyObject,
    Object,
    EmptyList,
    List
  };

  static const size_t MAX_STACK = 20u;

  Output& _output;
  bool _failed = false;
  DataStructure _stack[MAX_STACK] = {DataStructure::None};
  size_t _stackIndex = 0u;
  uint8_t _allowed = INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT;

  void allow(uint8_t allowed) { _allowed = allowed; }

  bool isAllowed(uint8_t op) const { return (_allowed & op) == op; }

  bool isStackAvailable() const { return _stackIndex < MAX_STACK; }

  bool isStackEmpty() const { return _stackIndex == 0u && _stack[_stackIndex] == DataStructure::None; }

  bool push(DataStructure value) {
    if (!isStackAvailable()) {
      return false;
    }
    if (!isStackEmpty()) {
      _stackIndex += 1;
    }
    _stack[_stackIndex] = value;
    return true;
  }

  DataStructure peek() { return _stack[_stackIndex]; }

  DataStructure pop() {
    if (isStackEmpty()) {
      return DataStructure::None;
    }
    _stackIndex -= 1;
    return _stack[_stackIndex + 1];
  }

  void replace(DataStructure value) {
    _stack[_stackIndex] = value;
  }

  template<typename T>
  void evaluate(uint8_t op, T value) {
    if (_failed || !isAllowed(op)) {
      _failed = true;
      return;
    }

    switch (op) {
      case INSERT_VALUE:
        if (peek() == DataStructure::List) {
          _failed = _failed || (_output.write(SEPARATOR) != 1u);
        }
        
        _failed = _failed || (_output.write(value) != str(value).len());

        switch (peek()) {
          case DataStructure::EmptyObject:
            replace(DataStructure::Object);
          case DataStructure::Object:
            allow(START_PROPERTY | CLOSE);
            break;
          case DataStructure::EmptyList:
            replace(DataStructure::List);
          case DataStructure::List:
            allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
            break;
          case DataStructure::None:
            allow(NOTHING);
            break;
          default:
            _failed = true;
            break;
        }
        break;
      case INSERT_STRING:
        if (peek() == DataStructure::List) {
          _failed = _failed || (_output.write(SEPARATOR) != 1u);
        }

        _failed = _failed || (_output.write(STRING_BEGIN) != 1u);
        _failed = _failed || (_output.write(value) != str(value).len());
        _failed = _failed || (_output.write(STRING_END) != 1u);

        switch (peek()) {
          case DataStructure::EmptyObject:
            replace(DataStructure::Object);
          case DataStructure::Object:
            allow(START_PROPERTY | CLOSE);
            break;
          case DataStructure::EmptyList:
            replace(DataStructure::List);
          case DataStructure::List:
            allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
            break;
          case DataStructure::None:
            allow(NOTHING);
            break;
          default:
            _failed = true;
            break;
        }
        break;
      case OPEN_LIST:
        switch (peek()) {
          case DataStructure::EmptyList:
            replace(DataStructure::List);
            break;
          case DataStructure::List:
            _failed = _failed || (_output.write(SEPARATOR) != 1u);
            break;
          default:
            // nothing
            break;
        }

        _failed = _failed || (_output.write(LIST_BEGIN) != 1u);
        _failed = _failed || !push(DataStructure::EmptyList);
        allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
        break;
      case OPEN_OBJECT:
        switch (peek()) {
          case DataStructure::EmptyList:
            replace(DataStructure::List);
            break;
          case DataStructure::List:
            _failed = _failed || (_output.write(SEPARATOR) != 1u);
            break;
          default:
            // nothing
            break;
        }
        
        _failed = _failed || (_output.write(OBJECT_BEGIN) != 1u);
        _failed = _failed || !push(DataStructure::EmptyObject);
        allow(START_PROPERTY | CLOSE);
        break;
      case START_PROPERTY:
        switch (peek()) {
          case DataStructure::EmptyObject:
            replace(DataStructure::Object);
            break;
          case DataStructure::Object:
            _failed = _failed || (_output.write(SEPARATOR) != 1u);
            break;
          default:
            _failed = true;
            break;
        }
        _failed = _failed || (_output.write(PROPERTY_BEGIN) != 1u);
        _failed = _failed || (_output.write(value) != str(value).len());
        _failed = _failed || (_output.write(PROPERTY_END) != 1u);
        _failed = _failed || (_output.write(PROPERTY_VALUE_SEPARATOR) != 1u);
        allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT);
        break;
      case CLOSE:
        switch (pop()) {
          case DataStructure::EmptyObject:
          case DataStructure::Object:
            _failed = _failed || (_output.write(OBJECT_END) != 1u);
            switch (peek()) {
              case DataStructure::EmptyObject:
              case DataStructure::Object:
                allow(START_PROPERTY | CLOSE);
                break;
              case DataStructure::EmptyList:
              case DataStructure::List:
                allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
                break;
              case DataStructure::None:
                allow(NOTHING);
                break;
            }
            break;
          case DataStructure::EmptyList:
          case DataStructure::List:
            _failed = _failed || (_output.write(LIST_END) != 1u);
            switch (peek()) {
              case DataStructure::EmptyObject:
              case DataStructure::Object:
                allow(START_PROPERTY | CLOSE);
                break;
              case DataStructure::EmptyList:
              case DataStructure::List:
                allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
                break;
              case DataStructure::None:
                allow(NOTHING);
                break;
            }
            break;
          default:
            _failed = true; // should not happen
            break;
        }
        break;
      default:
        _failed = true;
        break;
    }
  }

public:
  JsonWriter(Output& output) : _output(output) {}

  ~JsonWriter() {
    end();
  }

  void end() {
    while (!failed() && peek() != DataStructure::None) {
      evaluate(CLOSE, EMPTY_STRING);
    }
  }

  bool failed() const { return _failed; }

  template<typename X>
  void plainValue(X value) {
    evaluate(INSERT_VALUE, value);
  }

  template<typename X>
  void stringValue(X value) {
    evaluate(INSERT_STRING, value);
  }

  void openList() {
    evaluate(OPEN_LIST, EMPTY_STRING);
  }

  void openObject() {
    evaluate(OPEN_OBJECT, EMPTY_STRING);
  }

  template<typename X>
  void property(X name) {
    evaluate(START_PROPERTY, name);
  }

  template<typename X, typename Y>
  void propertyPlain(X name, Y value) {
    property(name);
    plainValue(value);
  }
  
  template<typename X, typename Y>
  void propertyString(X name, Y value) {
    property(name);
    stringValue(value);
  }

  void close() {
    evaluate(CLOSE, EMPTY_STRING);
  }
};

template<typename Output>
JsonWriter<Output> makeJsonWriter(Output& output) { return {output}; }

}

#endif
