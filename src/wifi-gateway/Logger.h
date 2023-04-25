#pragma once

#ifdef TEST_ENV
#include <cstdio>
#include <cstring>
int millis() { return 12345; }
#endif

#include "Utils.h"
#include <functional>

static const unsigned short LOG_ENTRY_SIZE = 128u;
static const char LOG_ENTRY_SEPARATOR = '\n';
static const unsigned short LOG_BUFFER_SIZE = 4096u;
static char logEntry[LOG_ENTRY_SIZE + 1];

class Logger {
  char _logBuffer[LOG_BUFFER_SIZE] = {};
  unsigned short _logBufferStart = 0u;
  unsigned short _logBufferEnd = 0u;

  void advanceBufferStart() {
    while (_logBuffer[_logBufferStart] != LOG_ENTRY_SEPARATOR) {
      _logBufferStart = (_logBufferStart + 1u) % LOG_BUFFER_SIZE;
    }
    _logBufferStart = (_logBufferStart + 1u) % LOG_BUFFER_SIZE;
  }

public:
  Logger() {
    memset(_logBuffer, LOG_ENTRY_SEPARATOR, LOG_BUFFER_SIZE);
  }

  void log(const char* category, const char* message) {
    snprintf(logEntry, LOG_ENTRY_SIZE, "[%u|%s] %s%c", millis(), category, message, LOG_ENTRY_SEPARATOR);
    logEntry[LOG_ENTRY_SIZE - 1u] = LOG_ENTRY_SEPARATOR;
    logEntry[LOG_ENTRY_SIZE] = '\0';

    for (unsigned short i = 0u; logEntry[i] != '\0'; ++i) {
      if (_logBufferEnd == LOG_BUFFER_SIZE) {
        _logBufferEnd = 0u;
        if (_logBufferStart == 0u) {
          advanceBufferStart();
        }
      } else if (_logBufferStart == _logBufferEnd && _logBufferStart != 0u) {
        advanceBufferStart();
      }

      _logBuffer[_logBufferEnd] = logEntry[i];
      _logBufferEnd += 1u;
    }
  }

  void output(std::function<void(const char* entry)> handler) const {
    if (_logBuffer[_logBufferStart] != LOG_ENTRY_SEPARATOR) {
      unsigned short bufferIndex = _logBufferStart;
      unsigned short entryIndex = 0u;
      do {
        bufferIndex = bufferIndex % LOG_BUFFER_SIZE;
        logEntry[entryIndex] = _logBuffer[bufferIndex];

        if (logEntry[entryIndex] == LOG_ENTRY_SEPARATOR) {
          logEntry[entryIndex + 1u] = '\0';
          handler(logEntry);
          entryIndex = 0u;
        } else {
          entryIndex += 1u;
        }

        bufferIndex = bufferIndex + 1u;
      } while (bufferIndex != _logBufferEnd);
    }
  }
};
