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

enum struct LogLevel : uint8_t {
  Always = 0,
  Error = 1,
  Warning = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
};

const char* logLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Always: return "ALW";
    case LogLevel::Error: return "ERR";
    case LogLevel::Warning: return "WRN";
    case LogLevel::Info: return "INF";
    case LogLevel::Debug: return "DBG";
    case LogLevel::Trace: return "TRC";
    default: return "???";
  }
}

LogLevel logLevelFromString(const char* level, size_t length = SIZE_MAX) {
  if (strncmp(level, "ALW", length) == 0) return LogLevel::Always;
  if (strncmp(level, "ERR", length) == 0) return LogLevel::Error;
  if (strncmp(level, "WRN", length) == 0) return LogLevel::Warning;
  if (strncmp(level, "INF", length) == 0) return LogLevel::Info;
  if (strncmp(level, "DBG", length) == 0) return LogLevel::Debug;
  if (strncmp(level, "TRC", length) == 0) return LogLevel::Trace;
  return LogLevel::Always;
}

class Logger final {
  static const LogLevel DEFAULT_LOG_LEVEL = LogLevel::Info;
  ConstStrMap<LogLevel> _logLevels = {};
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

  LogLevel logLevel(const char* category) {
    auto entry = _logLevels.find(category);
    if (entry == _logLevels.end()) {
      return DEFAULT_LOG_LEVEL;
    } else {
      return entry->second;
    }
  }

  void logLevel(const char* category, LogLevel level) {
    _logLevels[category] = level;
  }

  void log(LogLevel level, const char* category, const char* message) {
    snprintf(logEntry, LOG_ENTRY_SIZE, "[%lu|%s|%s] %s%c", millis(), category, logLevelName(level), message, LOG_ENTRY_SEPARATOR);
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

  void log(const char* category, const char* message) {
    log(LogLevel::Always, category, message);
  }

  void logIf(LogLevel level, const char* category, const char* message) {
    if (level <= logLevel(category)) {
      log(category, message);
    }
  };

  template<typename MessageFunction>
  void logIf(LogLevel level, const char* category, MessageFunction messageFunction) {
    if (level <= logLevel(category)) {
      const char* message = messageFunction();
      log(category, message);
    }
  };

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
