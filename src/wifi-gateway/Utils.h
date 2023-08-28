#pragma once

#ifdef TEST_ENV
#include <cstdint>
#include <cstring>
#include <cstdio>

class __FlashStringHelper;
#define F(str) reinterpret_cast<const __FlashStringHelper*>(str)
#define PGM_P const char*
#define strlen_P strlen
#define memcpy_P memcpy
#define vsnprintf_P vsnprintf

char* itoa (int val, char *s, int radix) { radix == 10 ? sprintf(s, "%d", val) : sprintf(s, "%X", val); return s; }
char* ltoa (long val, char *s, int radix) { radix == 10 ? sprintf(s, "%ld", val) : sprintf(s, "%lX", val); return s; }
char* utoa (unsigned int val, char *s, int radix) { radix == 10 ? sprintf(s, "%u", val) : sprintf(s, "%X", val); return s; }
char* ultoa (unsigned long val, char *s, int radix) { radix == 10 ? sprintf(s, "%lu", val) : sprintf(s, "%lX", val); return s;}
#endif

#include <algorithm>
#include <map>

class IntervalTimer {
  unsigned long _intervalDurationMs;
  unsigned long _lastIntervalTimeMs;
public:
  IntervalTimer(unsigned long intervalDurationMs) : _intervalDurationMs(intervalDurationMs), _lastIntervalTimeMs(millis()) {}

  bool elapsed() const {
    return (_lastIntervalTimeMs + _intervalDurationMs) < millis(); 
  }

  void restart() { _lastIntervalTimeMs = millis(); }
};

template<typename T>
struct Maybe {
  bool hasValue;
  T value;

  Maybe() : hasValue(false), value() {}
  Maybe(T value) : hasValue(true), value(value) {}
  Maybe(bool hasValue, T value) : hasValue(hasValue), value(value) {}
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

  size_t copy(char* dest, size_t maxLength, size_t offset = 0u) const {
    const size_t length = len(offset);
    size_t lengthToCopy = std::min(length, maxLength);
    
    memcpy(dest, _string + offset, lengthToCopy);
    dest[lengthToCopy] = '\0';

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

  size_t copy(char* dest, size_t maxLength, size_t offset = 0u) const {
    const size_t length = len(offset);
    size_t lengthToCopy = std::min(length, maxLength);
    
    memcpy_P(dest, (PGM_P)_string + offset, lengthToCopy);
    dest[lengthToCopy] = '\0';

    return length;
  }
  
  size_t len(size_t offset = 0u) const { return strlen_P((PGM_P)_string + offset); }
};

static char NUMBER_STRING_BUFFER[2 + 8 * sizeof(long)];

const char* toConstStr(char value)
{
  NUMBER_STRING_BUFFER[0] = value;
  NUMBER_STRING_BUFFER[1] = '\0';
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(unsigned char value, unsigned char base)
{
  utoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(short value, unsigned char base)
{
  itoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(unsigned short value, unsigned char base)
{
  utoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(int value, unsigned char base)
{
  itoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(unsigned int value, unsigned char base)
{
  utoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(long value, unsigned char base)
{
  ltoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

const char* toConstStr(unsigned long value, unsigned char base)
{
  ultoa(value, NUMBER_STRING_BUFFER, base);
  return NUMBER_STRING_BUFFER;
}

struct str_less_than
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

template<typename T>
using ConstStrMap = std::map<const char*, T, str_less_than>;
