#ifndef IOT_CORE_BUFFER_H_
#define IOT_CORE_BUFFER_H_

#include "Utils.h"

namespace iot_core {

/**
 * Class template for a plain char buffer, which can be used to create e.g.
 * messages to be sent via serial or network.
 */
template<size_t BUFFER_SIZE = 512u>
class Buffer final {
  char _buffer[BUFFER_SIZE + 1u] = {}; // +1 for null-termination
  size_t _size = 0u;
  bool _overrun = false;

public:
  Buffer() {}

  const char* c_str() const {
    return _buffer;
  }

  const uint8_t* data() const {
    return (uint8_t*)(&_buffer[0]);
  }

  size_t size() const {
    return _size;
  }

  bool overrun() const {
    return _overrun;
  }

  void clear() {
    _size = 0u;
    _overrun = false;
  }

  template<typename X>
  size_t write(X text) {
    return write(iot_core::str(text));
  }

  template<typename U>
  size_t write(iot_core::ConstString<U> text) {
    size_t maxLength = BUFFER_SIZE - _size;
    size_t textLength = text.copy(_buffer + _size, maxLength, 0u);

    if (maxLength < textLength) {
      _overrun = true;
      _size += maxLength;
      return maxLength;
    } else {
      _size += textLength;
      return textLength;
    }
  }

  size_t write(char c) {
    if (_size >= BUFFER_SIZE) {
      _overrun = true;
      return 0u;
    }
    _buffer[_size] = c;
    _size += 1u;
    _buffer[_size] = '\0';
    return 1u;
  }
};

}

#endif
