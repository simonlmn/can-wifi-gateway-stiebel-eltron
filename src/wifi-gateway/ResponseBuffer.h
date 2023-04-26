#pragma once

#include "Utils.h"

template<typename T, size_t BUFFER_SIZE = 1024u>
class ResponseBuffer final {
  template<typename X>
  using remove_const_ptr = typename std::remove_const<typename std::remove_pointer<X>::type>::type;

  T& _server;
  char _buffer[BUFFER_SIZE + 1u]; // +1 for null-termination
  size_t _size;

public:
  explicit ResponseBuffer(T& server) : _server(server), _buffer(), _size(0u) {}

  size_t size() {
    return _size;
  }

  void clear() {
    _size = 0u;
  }

  template<typename TText>
  bool begin(int code, const TText* contentType) {
    bool ok = _server.chunkedResponseModeStart(code, contentType);
    clear();
    return ok;
  }

  void flush() {
    if (_size == 0u) {
      return;
    }

    _server.sendContent(_buffer, _size);
    clear();
  }

  void end() {
    flush();
    _server.chunkedResponseFinalize();
    clear();
  }

  template<typename X>
  void plainText(X text) { plainText(ConstString<remove_const_ptr<X>>{text}); }

  template<typename U>
  void plainText(ConstString<U> text) {
    size_t offset = 0u;
    do {
      size_t maxLength = BUFFER_SIZE - _size;
      size_t textLength = text.copy(_buffer + _size, maxLength, offset);
      
      if (maxLength < textLength) {
        _size += maxLength;
        offset += maxLength;
        flush();
      } else {
        _size += textLength;
        break;
      }
    } while (true);
  }

  void plainChar(char c) {
    if (_size >= BUFFER_SIZE) {
      flush();
    }
    _buffer[_size] = c;
    _size += 1u;
  }

  void jsonObjectOpen() {
    plainChar('{');
  }

  void jsonObjectClose() {
    plainChar('}');
  }

  void jsonListOpen() {
    plainChar('[');
  }

  void jsonListClose() {
    plainChar(']');
  }

  template<typename X>
  void jsonPropertyStart(X name) { jsonPropertyStart(ConstString<remove_const_ptr<X>>{name}); }
  
  template<typename U>
  void jsonPropertyStart(ConstString<U> name) {
    jsonString(name);
    plainChar(':');
  }

  template<typename X, typename Y>
  void jsonPropertyRaw(X name, Y value) { jsonPropertyRaw(ConstString<remove_const_ptr<X>>{name}, ConstString<remove_const_ptr<Y>>{value}); }
  
  template<typename U, typename V>
  void jsonPropertyRaw(ConstString<U> name, ConstString<V> value) {
    jsonPropertyStart(name);
    plainText(value);
  }

  template<typename X, typename Y>
  void jsonPropertyString(X name, Y value) { jsonPropertyString(ConstString<remove_const_ptr<X>>{name}, ConstString<remove_const_ptr<Y>>{value}); }
  
  template<typename U, typename V>
  void jsonPropertyString(ConstString<U> name, ConstString<V> value) {
    jsonPropertyStart(name);
    jsonString(value);
  }

  template<typename X>
  void jsonString(X value) { jsonString(ConstString<remove_const_ptr<X>>{value}); }
  
  template<typename V>
  void jsonString(ConstString<V> value) {
    plainChar('"');
    plainText(value);
    plainChar('"');
  }

  void jsonSeparator() {
    plainChar(',');
  }
};
