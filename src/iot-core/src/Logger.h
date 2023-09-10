#ifndef IOT_CORE_LOGGER_H_
#define IOT_CORE_LOGGER_H_

#include "Utils.h"
#include "DateTime.h"
#include <functional>
#include <algorithm>

namespace iot_core {

static const size_t MAX_LOG_ENTRY_LENGTH = 128u;
static const size_t LOG_BUFFER_SIZE = 4096u;
static const char LOG_ENTRY_SEPARATOR = '\n';

enum struct LogLevel : uint8_t {
  None = 0,
  Error = 1,
  Warning = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
};

const char* logLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::None: return "   ";
    case LogLevel::Error: return "ERR";
    case LogLevel::Warning: return "WRN";
    case LogLevel::Info: return "INF";
    case LogLevel::Debug: return "DBG";
    case LogLevel::Trace: return "TRC";
    default: return "???";
  }
}

LogLevel logLevelFromString(const char* level, size_t length = SIZE_MAX) {
  if (strncmp(level, "   ", length) == 0) return LogLevel::None;
  if (strncmp(level, "ERR", length) == 0) return LogLevel::Error;
  if (strncmp(level, "WRN", length) == 0) return LogLevel::Warning;
  if (strncmp(level, "INF", length) == 0) return LogLevel::Info;
  if (strncmp(level, "DBG", length) == 0) return LogLevel::Debug;
  if (strncmp(level, "TRC", length) == 0) return LogLevel::Trace;
  return LogLevel::None;
}

class Logger final {
  static const LogLevel DEFAULT_LOG_LEVEL = LogLevel::Info;
  LogLevel _initialLogLevel = DEFAULT_LOG_LEVEL;
  ConstStrMap<LogLevel> _logLevels = {};
  mutable char _logEntry[MAX_LOG_ENTRY_LENGTH + 1] = {};
  size_t _logEntryLength = 0u;
  char _logBuffer[LOG_BUFFER_SIZE] = {};
  size_t _logBufferStart = 0u;
  size_t _logBufferEnd = 0u;
  Time const& _uptime;

  template<typename T>
  void logInternal(LogLevel level, const char* category, T message) {
    beginLogEntry(level, category);
    str(message).copy(_logEntry + _logEntryLength, MAX_LOG_ENTRY_LENGTH - _logEntryLength, 0u, &_logEntryLength);
    commitLogEntry();
  }

  void beginLogEntry(LogLevel level, const char* category) {
    size_t actualLength = snprintf_P(_logEntry, MAX_LOG_ENTRY_LENGTH, PSTR("[%s|%s|%s] "), _uptime.format(), category, logLevelToString(level));
    _logEntryLength = std::min(actualLength, MAX_LOG_ENTRY_LENGTH);
  }

  void commitLogEntry() {
    if (_logEntryLength == 0u) {
      return;
    }
    _logEntry[_logEntryLength] = LOG_ENTRY_SEPARATOR;
    _logEntry[_logEntryLength + 1u] = '\0';

    for (unsigned short i = 0u; _logEntry[i] != '\0'; ++i) {
      if (_logBufferEnd == LOG_BUFFER_SIZE) {
        _logBufferEnd = 0u;
        if (_logBufferStart == 0u) {
          advanceBufferStart();
        }
      } else if (_logBufferStart == _logBufferEnd && _logBufferStart != 0u) {
        advanceBufferStart();
      }

      _logBuffer[_logBufferEnd] = _logEntry[i];
      _logBufferEnd += 1u;
    }

    _logEntryLength = 0u;
    _logEntry[_logEntryLength] = '\0';
  }

  void advanceBufferStart() {
    while (_logBuffer[_logBufferStart] != LOG_ENTRY_SEPARATOR) {
      _logBufferStart = (_logBufferStart + 1u) % LOG_BUFFER_SIZE;
    }
    _logBufferStart = (_logBufferStart + 1u) % LOG_BUFFER_SIZE;
  }

public:
  explicit Logger(Time const& uptime) : _uptime(uptime) {
    memset(_logBuffer, LOG_ENTRY_SEPARATOR, LOG_BUFFER_SIZE);
  }

  LogLevel initialLogLevel() const {
    return _initialLogLevel;
  }

  void initialLogLevel(LogLevel level) {
    _initialLogLevel = level;
  }

  LogLevel logLevel(const char* category) const {
    auto entry = _logLevels.find(category);
    if (entry == _logLevels.end()) {
      return _initialLogLevel;
    } else {
      return entry->second;
    }
  }

  const ConstStrMap<LogLevel>& logLevels() const {
    return _logLevels;
  }

  void logLevel(const char* category, LogLevel level) {
    _logLevels[category] = level;
  }

  template<typename T>
  void log(const char* category, T message) {
    logInternal(LogLevel::None, category, message);
  }

  template<typename T, std::enable_if_t<!std::is_invocable<T>::value, bool> = true>
  void log(LogLevel level, const char* category, T message) {
    if (level <= logLevel(category)) {
      logInternal(level, category, message);
    }
  };

  template<typename T, std::enable_if_t<std::is_invocable<T>::value, bool> = true>
  void log(LogLevel level, const char* category, T messageFunction) {
    if (level <= logLevel(category)) {
      logInternal(level, category, messageFunction());
    }
  };

  void output(std::function<void(const char* entry)> handler) const {
    if (_logBuffer[_logBufferStart] != LOG_ENTRY_SEPARATOR) {
      unsigned short bufferIndex = _logBufferStart;
      unsigned short entryIndex = 0u;
      do {
        bufferIndex = bufferIndex % LOG_BUFFER_SIZE;
        _logEntry[entryIndex] = _logBuffer[bufferIndex];

        if (_logEntry[entryIndex] == LOG_ENTRY_SEPARATOR) {
          _logEntry[entryIndex + 1u] = '\0';
          handler(_logEntry);
          entryIndex = 0u;
        } else {
          entryIndex += 1u;
        }

        bufferIndex = bufferIndex + 1u;
      } while (bufferIndex != _logBufferEnd);
    }
  }
};

}

#endif
