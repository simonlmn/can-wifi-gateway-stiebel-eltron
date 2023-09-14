#ifndef IOT_CORE_UTILS_H_
#define IOT_CORE_UTILS_H_

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <type_traits>

namespace iot_core {

class IntervalTimer final {
  unsigned long _intervalDurationMs;
  unsigned long _lastIntervalTimeMs;
public:
  IntervalTimer(unsigned long intervalDurationMs) : _intervalDurationMs(intervalDurationMs), _lastIntervalTimeMs(millis()) {}

  bool elapsed() const {
    return (_lastIntervalTimeMs + _intervalDurationMs) < millis(); 
  }

  void restart() { _lastIntervalTimeMs = millis(); }
};


template<size_t SAMPLES>
class TimingStatistics final {
  unsigned long _samples[SAMPLES] = {};
  bool _hasSamples = false;
  size_t _oldestSampleIndex = 0u;
  size_t _newestSampleIndex = 0u;

  unsigned long _startTime = 0u;

  void newSample(unsigned long value) {
    if (_hasSamples) {
      _newestSampleIndex = (_newestSampleIndex + 1) % SAMPLES;
      if (_newestSampleIndex == _oldestSampleIndex) {
        _oldestSampleIndex = (_oldestSampleIndex + 1) % SAMPLES;
      }
    }

    _samples[_newestSampleIndex] = value;

    _hasSamples = true;
  }
  
public:
  void start() {
    _startTime = micros();
  }

  void stop() {
    newSample(micros() - _startTime);
  }

  unsigned long min() const {
    size_t numberOfSamples = count();
    unsigned long result = _samples[_oldestSampleIndex];
    for (size_t i = 1u; i < numberOfSamples; ++i) {
      result = std::min(_samples[(_oldestSampleIndex + i) % SAMPLES], result);
    }
    return result;
  }

  unsigned long max() const {
    size_t numberOfSamples = count();
    unsigned long result = _samples[_oldestSampleIndex];
    for (size_t i = 1u; i < numberOfSamples; ++i) {
      result = std::max(_samples[(_oldestSampleIndex + i) % SAMPLES], result);
    }
    return result;
  }

  unsigned long avg() const {
    size_t numberOfSamples = count();
    if (numberOfSamples == 0u) {
      return 0u;
    }

    unsigned long result = _samples[_oldestSampleIndex] / numberOfSamples;
    for (size_t i = 1u; i < numberOfSamples; ++i) {
      result += _samples[(_oldestSampleIndex + i) % SAMPLES] / numberOfSamples;
    }
    return result;
  }

  size_t count() const { return _hasSamples ? (_newestSampleIndex >= _oldestSampleIndex ? (_newestSampleIndex - _oldestSampleIndex + 1) : (SAMPLES - _oldestSampleIndex + _newestSampleIndex + 1)) : 0u; }

  template<typename F>
  auto wrap(F f) {
    return [this,f] () {
      this->start();
      f();
      this->stop();
    };
  }
};


template<typename T>
struct Maybe final {
  bool hasValue;
  T value;

  Maybe() : hasValue(false), value() {}
  Maybe(T value) : hasValue(true), value(value) {}
  Maybe(bool hasValue, T value) : hasValue(hasValue), value(value) {}

  template<typename F>
  Maybe<typename std::invoke_result<F, T>::type> then(F f) const {
    if (hasValue) {
      return f(value);
    } else {
      return {};
    }
  }

  template<typename F>
  T otherwise(F f) const {
    if (hasValue) {
      return value;
    } else {
      return f();
    }
  }
};

Maybe<long int> asInteger(const char* value) {
  char* endptr;
  long int integer = strtol(value, &endptr, 10);

  return { endptr != value, integer };
}

const char* EMPTY_STRING = "";

static const unsigned short DEFAULT_FORMAT_BUFFER_SIZE = 128u;
static char DEFAULT_FORMAT_BUFFER[DEFAULT_FORMAT_BUFFER_SIZE + 1];

char* format(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(DEFAULT_FORMAT_BUFFER, DEFAULT_FORMAT_BUFFER_SIZE + 1, fmt, args);
  va_end(args);
  if (DEFAULT_FORMAT_BUFFER_SIZE > 0) {
    DEFAULT_FORMAT_BUFFER[DEFAULT_FORMAT_BUFFER_SIZE] = '\0';
  }
  return DEFAULT_FORMAT_BUFFER;
}

char* format(const __FlashStringHelper* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf_P(DEFAULT_FORMAT_BUFFER, DEFAULT_FORMAT_BUFFER_SIZE + 1, (PGM_P)fmt, args);
  va_end(args);
  if (DEFAULT_FORMAT_BUFFER_SIZE > 0) {
    DEFAULT_FORMAT_BUFFER[DEFAULT_FORMAT_BUFFER_SIZE] = '\0';
  }
  return DEFAULT_FORMAT_BUFFER;
}

template<typename T>
class ConstString {
};

template<>
class ConstString<char> final {
public:
  typedef const char* Type;

private:
  Type _string;

public:
  ConstString(Type string) : _string(string) {}

  size_t copy(char* dest, size_t maxLength, size_t offset = 0u, size_t* destLength = nullptr) const {
    const size_t length = len(offset);
    size_t lengthToCopy = std::min(length, maxLength);
    
    memcpy(dest, _string + offset, lengthToCopy);
    dest[lengthToCopy] = '\0';

    if (destLength != nullptr) {
      *destLength += lengthToCopy;
    }

    return length;
  }
  
  size_t len(size_t offset = 0u) const { return strlen(_string + offset); }
};

template<>
class ConstString<__FlashStringHelper> final {
public:
  typedef const __FlashStringHelper* Type;

private:
  Type _string;

public:
  ConstString(Type string) : _string(string) {}

  size_t copy(char* dest, size_t maxLength, size_t offset = 0u, size_t* destLength = nullptr) const {
    const size_t length = len(offset);
    size_t lengthToCopy = std::min(length, maxLength);
    
    memcpy_P(dest, (PGM_P)_string + offset, lengthToCopy);
    dest[lengthToCopy] = '\0';

    if (destLength != nullptr) {
      *destLength += lengthToCopy;
    }

    return length;
  }
  
  size_t len(size_t offset = 0u) const { return strlen_P((PGM_P)_string + offset); }
};

template<typename T>
using remove_const_ptr = typename std::remove_const<typename std::remove_pointer<T>::type>::type;

template<typename T>
ConstString<remove_const_ptr<T>> str(T string) {
  return {string};
}

template<typename T>
struct convert final {
  static const char* toString(T value) { return ""; }
  static T fromString(const char* value) { return {}; }
};

static char NUMBER_STRING_BUFFER[2 + 8 * sizeof(long)];

template<>
struct convert<char> final {
  static const char* toString(char value) {
    NUMBER_STRING_BUFFER[0] = value;
    NUMBER_STRING_BUFFER[1] = '\0';
    return NUMBER_STRING_BUFFER;
  }

  static char fromString(const char* value) {
    return value[0];
  }
};

template<>
struct convert<unsigned char> final {
  static const char* toString(unsigned char value, int base) {
    utoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static unsigned char fromString(const char* value, char** endptr, int base) {
    return static_cast<unsigned char>(strtoul(value, endptr, base));
  }
};

template<>
struct convert<short> final {
  static const char* toString(short value, int base) {
    itoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static short fromString(const char* value, char** endptr, int base) {
    return static_cast<short>(strtol(value, endptr, base));
  }
};

template<>
struct convert<unsigned short> final {
  static const char* toString(unsigned short value, int base) {
    utoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static unsigned short fromString(const char* value, char** endptr, int base) {
    return static_cast<unsigned short>(strtoul(value, endptr, base));
  }
};

template<>
struct convert<int> final {
  static const char* toString(int value, int base) {
    itoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static int fromString(const char* value, char** endptr, int base) {
    return static_cast<int>(strtol(value, endptr, base));
  }
};

template<>
struct convert<unsigned int> final {
  static const char* toString(unsigned int value, int base) {
    utoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static unsigned int fromString(const char* value, char** endptr, int base) {
    return static_cast<unsigned int>(strtoul(value, endptr, base));
  }
};

template<>
struct convert<long> final {
  static const char* toString(long value, int base) {
    ltoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static long fromString(const char* value, char** endptr, int base) {
    return static_cast<long>(strtol(value, endptr, base));
  }
};

template<>
struct convert<unsigned long> final {
  static const char* toString(unsigned long value, int base) {
    ultoa(value, NUMBER_STRING_BUFFER, base);
    return NUMBER_STRING_BUFFER;
  }

  static unsigned long fromString(const char* value, char** endptr, int base) {
    return static_cast<unsigned long>(strtoul(value, endptr, base));
  }
};

enum struct BoolFormat {
  Logic,
  Numeric,
  Io
};

template<>
struct convert<bool> final {
  static const char* toString(bool value, BoolFormat format = BoolFormat::Logic) {
    switch (format) {
      default:
      case BoolFormat::Logic: return value ? ("true") : ("false");
      case BoolFormat::Numeric: return value ? ("1") : ("0");
      case BoolFormat::Io: return value ? ("HIGH") : ("LOW");
    }
  }

  static bool fromString(const char* value, bool defaultValue, const char** endptr = nullptr, BoolFormat format = BoolFormat::Logic) {
    const char* start = value;
    while (*start == ' ') { start += 1; }
    
    switch (format) {
      default:
      case BoolFormat::Logic:
        if (strcmp(start, ("true")) == 0) {
          if (endptr != nullptr) { *endptr = start + 4; }
          return true;
        } else if (strcmp(start, ("false")) == 0) {
          if (endptr != nullptr) { *endptr = start + 5; }
          return false;
        } else {
          if (endptr != nullptr) { *endptr = value; }
          return defaultValue;
        }
        break;
      case BoolFormat::Numeric:
        if (strcmp(start, ("1")) == 0) {
          if (endptr != nullptr) { *endptr = start + 1; }
          return true;
        } else if (strcmp(start, ("0")) == 0) {
          if (endptr != nullptr) { *endptr = start + 1; }
          return false;
        } else {
          if (endptr != nullptr) { *endptr = value; }
          return defaultValue;
        }
        break;
      case BoolFormat::Io:
        if (strcmp(start, ("HIGH")) == 0) {
          if (endptr != nullptr) { *endptr = start + 4; }
          return true;
        } else if (strcmp(start, ("LOW")) == 0) {
          if (endptr != nullptr) { *endptr = start + 3; }
          return false;
        } else {
          if (endptr != nullptr) { *endptr = value; }
          return defaultValue;
        }
        break;;
    }
  }
};

struct str_less_than
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

template<typename T>
using ConstStrMap = std::map<const char*, T, str_less_than>;

using ConstStrSet = std::set<const char*, str_less_than>;

/**
 * Turn a dynamically/shared allocated string into a static string with indefinite lifetime.
 * 
 * Note: As this allocates memory on the heap which won't be free'd again, use this only for
 * strings which truly need to live forever, e.g. for keys in a ConstStrMap.
 */
const char* make_static(const char* string) {
  static ConstStrSet strings {};
  auto entry = strings.find(string);
  if (entry == strings.end()) {
    size_t length = strlen(string);
    char* staticString = new char[length + 1];
    str(string).copy(staticString, length);
    strings.insert(staticString);
    return staticString;
  } else {
    return *entry;
  }
}

}

#endif
