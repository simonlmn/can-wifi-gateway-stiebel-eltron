#pragma once

#ifdef TEST_ENV
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#endif

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
