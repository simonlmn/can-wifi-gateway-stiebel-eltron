#ifndef YATEST_MOCKS_H_
#define YATEST_MOCKS_H_

#include <cstdint>
#include <cstring>
#include <cstdio>

class __FlashStringHelper;
#define F(str) reinterpret_cast<const __FlashStringHelper*>(str)
#define PGM_P const char*
#define PSTR(str) str
#define strlen_P strlen
#define memcpy_P memcpy
#define snprintf_P snprintf
#define vsnprintf_P vsnprintf

char* itoa (int val, char *s, int radix) { radix == 10 ? sprintf(s, "%d", val) : sprintf(s, "%X", val); return s; }
char* ltoa (long val, char *s, int radix) { radix == 10 ? sprintf(s, "%ld", val) : sprintf(s, "%lX", val); return s; }
char* utoa (unsigned int val, char *s, int radix) { radix == 10 ? sprintf(s, "%u", val) : sprintf(s, "%X", val); return s; }
char* ultoa (unsigned long val, char *s, int radix) { radix == 10 ? sprintf(s, "%lu", val) : sprintf(s, "%lX", val); return s;}

static unsigned long _mock_millis = 0u;
unsigned long millis() {
  return _mock_millis;
}

static unsigned long _mock_micros = 0u;
unsigned long micros() {
  return _mock_micros;
}

void advanceTimeMs(unsigned long millis) {
  _mock_millis += millis;
  _mock_micros += millis * 1000;
}

void advanceTimeUs(unsigned long micros) {
  _mock_micros += micros;
  _mock_millis += micros / 1000;
}

#endif
