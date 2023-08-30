#ifndef IOT_CORE_CHUNKEDRESPONSE_H_
#define IOT_CORE_CHUNKEDRESPONSE_H_

#include "Utils.h"

namespace iot_core {

/**
 * Class template to send a chunked HTTP response to a client.
 * 
 * The buffer size influences how many chunks have to be sent and each chunk
 * incurs a (small) processing and transmission overhead. Thus setting the
 * size too low may reduce performance.
 */
template<typename T, size_t BUFFER_SIZE = 512u>
class ChunkedResponse final {
  T& _server;
  char _buffer[BUFFER_SIZE + 1u] = {}; // +1 for null-termination
  size_t _size = 0u;

public:
  explicit ChunkedResponse(T& server) : _server(server) {}

  size_t size() const {
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
  size_t write(X text) {
    return write(iot_core::str(text));
  }

  template<typename U>
  size_t write(iot_core::ConstString<U> text) {
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
    return text.len();
  }

  size_t write(char c) {
    if (_size >= BUFFER_SIZE) {
      flush();
    }
    _buffer[_size] = c;
    _size += 1u;
    return 1u;
  }
};

}

#endif
