#ifndef IOT_CORE_DATETIME_H_
#define IOT_CORE_DATETIME_H_

#ifdef TEST_ENV
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#endif

namespace iot_core {

struct DateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t ms;
  
  DateTime() : year(0), month(0), day(0), hour(0), minute(0), second(0), ms(0) {}

  bool operator>(const DateTime& rhs) const {
    return year > rhs.year
        || (year == rhs.year && (month > rhs.month
        || (month == rhs.month && (day > rhs.day
        || (day == rhs.day && (hour > rhs.hour
        || (hour == rhs.hour && (minute > rhs.minute
        || (minute == rhs.minute && (second > rhs.second
        || (second == rhs.second && (ms > rhs.ms
        ))))))))))));
  }

  bool operator>=(const DateTime& rhs) const {
    return year > rhs.year
        || (year == rhs.year && (month > rhs.month
        || (month == rhs.month && (day > rhs.day
        || (day == rhs.day && (hour > rhs.hour
        || (hour == rhs.hour && (minute > rhs.minute
        || (minute == rhs.minute && (second > rhs.second
        || (second == rhs.second && (ms >= rhs.ms
        ))))))))))));
  }

  bool isSet() const {
    return year != 0 && month != 0 && day != 0;
  }

  void fromString(const char* str) {
    if (strlen(str) != 23u) {
      return;
    }

    year = atoi(str + 0);
    month = atoi(str + 5);
    day = atoi(str + 8);
    hour = atoi(str + 11);
    minute = atoi(str + 14);
    second = atoi(str + 17);
    ms = atoi(str + 20);
  }

  const char* toString() const {
    static char toStringBuffer[24u]; // "YYYY-MM-DDThh:mm:ss.fff\0"
    snprintf(toStringBuffer, 24u, "%04u-%02u-%02uT%02u:%02u:%02u.%03u", year, month, day, hour, minute, second, ms);
    return toStringBuffer;
  }
};

static const DateTime NO_DATETIME {};

const char* formatTime(unsigned long time, uint8_t epoch = 0u) {
  static char toStringBuffer[21u]; // "255e9w6d23h59m59s999\0"

  uint16_t millis = time % 1000u;
  uint16_t seconds = time / 1000u % 60u;
  uint16_t minutes = time / 1000u / 60u % 60u;
  uint16_t hours = time / 1000u / 60u / 60u % 24u;
  uint16_t days = time / 1000u / 60u / 60u / 24u % 7u;
  uint16_t weeks = time / 1000u / 60u / 60u / 24u / 7u;

  snprintf(toStringBuffer, 21u, "%ue%1uw%1ud%02uh%02um%02us%03u", epoch, weeks, days, hours, minutes, seconds, millis);
  return toStringBuffer;
}

struct Time {
private:
  unsigned long _millis = 0u;
  uint8_t _epoch = 0u;

public:
  unsigned long millis() const { return _millis; }
  uint8_t epoch() const { return _epoch; }

  void update() {
    auto currentMs = ::millis();
    if (currentMs < _millis) {
      _epoch += 1;
    }
    _millis = currentMs;
  }

  const char* format() const { return formatTime(_millis, _epoch); }
};

}

#endif
