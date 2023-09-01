#ifndef IOT_CORE_IDATETIMESOURCE_H_
#define IOT_CORE_IDATETIMESOURCE_H_

#include "DateTime.h"

namespace iot_core {

class IDateTimeSource {
public:
  virtual bool available() const = 0;
  virtual const DateTime& currentDateTime() const = 0;
};

class NoDateTimeSource final : public IDateTimeSource {
public:
  bool available() const { return false; }
  const DateTime& currentDateTime() const { return NO_DATETIME; }
};

static const NoDateTimeSource NO_DATE_TIME_SOURCE {};

}

#endif
