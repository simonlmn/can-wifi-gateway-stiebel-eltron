#ifndef DATETIMESOURCE_H_
#define DATETIMESOURCE_H_

#include <iot_core/IDateTimeSource.h>
#include <iot_core/Logger.h>
#include "StiebelEltronProtocol.h"
#include "ValueDefinitions.h"

struct DateTimeField {
  const ValueDefinition* definition;
  uint8_t value;
  unsigned long lastUpdateMs;

  DateTimeField() : definition(NULL), value(0), lastUpdateMs(0) {}
};

struct DateTimeFields {
  DateTimeField year;
  DateTimeField month;
  DateTimeField day;
  DateTimeField hour;
  DateTimeField minute;
  uint8_t availableFields;

  DateTimeFields() : year(), month(), day(), hour(), minute(), availableFields(0) {}
};

class DateTimeSource final : public iot_core::IDateTimeSource, public iot_core::IApplicationComponent, public IStiebelEltronDevice {
private:
  iot_core::Logger& _logger;
  StiebelEltronProtocol& _protocol;
  
  DateTimeFields _dateTimeFields;
  iot_core::DateTime _currentDateTime;

public:
  DateTimeSource(iot_core::Logger& logger, StiebelEltronProtocol& protocol)
    : _logger(logger),
    _protocol(protocol),
    _dateTimeFields(),
    _currentDateTime() {}

  const char* name() const override {
    return "dts";
  }

  const char* description() const override {
    return "Date Time Source";
  }

  bool configure(const char* /*name*/, const char* /*value*/) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> /*writer*/) const override {
  }

  void setup(bool /*connected*/) override {
    _dateTimeFields.year.definition = &getDefinition(DATETIME_YEAR_ID);
    _dateTimeFields.month.definition = &getDefinition(DATETIME_MONTH_ID);
    _dateTimeFields.day.definition = &getDefinition(DATETIME_DAY_ID);
    _dateTimeFields.hour.definition = &getDefinition(DATETIME_HOUR_ID);
    _dateTimeFields.minute.definition = &getDefinition(DATETIME_MINUTE_ID);
    
    _protocol.addDevice(this);
    _protocol.onResponse([this] (ResponseData data) { processData(data.valueId, data.value); });
    _protocol.onWrite([this] (WriteData data) { processData(data.valueId, data.value); });
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    if (!_protocol.ready()) {
      return;
    }

    updateDateTime();
    requestDateTimeFields();
  }

  void getDiagnostics(iot_core::IDiagnosticsCollector& /*collector*/) const override {
  }

  const DeviceId& deviceId() const override {
    static const DeviceId id {DeviceType::X0E, 1};
    return id;
  }

  void request(const RequestData& data) override {
    // no data to be requested from here
  }

  void write(const WriteData& data) override {
    // we capture any writes with a global listener already
  }

  void receive(const ResponseData& data) override {
    // we capture any responses with a global listener already
  }

  bool available() const override {
    return _currentDateTime.isSet();
  }

  const iot_core::DateTime& currentDateTime() const override {
    return _currentDateTime;
  }
  
private:
  unsigned long _requestDateTimeFieldIntervalMs = 30000;
  unsigned long _dateTimeFieldAgeThresholdMs = 30000;
  unsigned long _lastRequestDateTimeFields = 0;
  
  void requestDateTimeFields() {
    auto currentMs = millis();
    if (currentMs > (_lastRequestDateTimeFields + _requestDateTimeFieldIntervalMs)) {
      if (currentMs > _dateTimeFields.minute.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), SYSTEM_ID, DATETIME_MINUTE_ID });
      }

      if (currentMs > _dateTimeFields.hour.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), SYSTEM_ID, DATETIME_HOUR_ID });
      }

      if (currentMs > _dateTimeFields.day.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), SYSTEM_ID, DATETIME_DAY_ID });
      }

      if (currentMs > _dateTimeFields.month.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), SYSTEM_ID, DATETIME_MONTH_ID });
      }

      if (currentMs > _dateTimeFields.year.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), SYSTEM_ID, DATETIME_YEAR_ID });
      }

      _lastRequestDateTimeFields = currentMs;
    }
  }

  void processData(ValueId valueId, uint16_t value) {
    if (isDateTimeField(valueId)) {
      bool availableBefore = available();
      updateDateTimeField(valueId, value);
      if (!availableBefore && available()) {
        _logger.log(iot_core::LogLevel::Info, name(), iot_core::format(F("Date and time acquired: %s"), _currentDateTime.toString()));
      }
    }
  }

  bool isDateTimeField(ValueId valueId) {
    return valueId == DATETIME_DAY_ID
        || valueId == DATETIME_MONTH_ID
        || valueId == DATETIME_YEAR_ID
        || valueId == DATETIME_HOUR_ID
        || valueId == DATETIME_MINUTE_ID;
  }

  void updateDateTimeField(ValueId valueId, uint16_t value) {
    switch (valueId) {
      case DATETIME_YEAR_ID: {
          iot_core::Maybe<long int> year = iot_core::asInteger(_dateTimeFields.year.definition->fromRaw(value));
          if (year.hasValue) {
            _dateTimeFields.year.value = year.value;
            _dateTimeFields.year.lastUpdateMs = millis();
            _dateTimeFields.availableFields |= 1;
          }
        }
        break;
      case DATETIME_MONTH_ID: {
          iot_core::Maybe<long int> month = iot_core::asInteger(_dateTimeFields.month.definition->fromRaw(value));
          if (month.hasValue) {
            _dateTimeFields.month.value = month.value;
            _dateTimeFields.month.lastUpdateMs = millis();
            _dateTimeFields.availableFields |= 2;
          }
        }
        break;
      case DATETIME_DAY_ID: {
          iot_core::Maybe<long int> day = iot_core::asInteger(_dateTimeFields.day.definition->fromRaw(value));
          if (day.hasValue) {
            _dateTimeFields.day.value = day.value;
            _dateTimeFields.day.lastUpdateMs = millis();
            _dateTimeFields.availableFields |= 4;
          }
        }
        break;
      case DATETIME_HOUR_ID: {
          iot_core::Maybe<long int> hour = iot_core::asInteger(_dateTimeFields.hour.definition->fromRaw(value));
          if (hour.hasValue) {
            _dateTimeFields.hour.value = hour.value;
            _dateTimeFields.hour.lastUpdateMs = millis();
            _dateTimeFields.availableFields |= 8;
          }
        }
        break;
      case DATETIME_MINUTE_ID: {
          iot_core::Maybe<long int> minute = iot_core::asInteger(_dateTimeFields.minute.definition->fromRaw(value));
          if (minute.hasValue && (minute.value != _dateTimeFields.minute.value || _dateTimeFields.minute.lastUpdateMs == 0)) {
            _dateTimeFields.minute.value = minute.value;
            _dateTimeFields.minute.lastUpdateMs = millis();
            _dateTimeFields.availableFields |= 16;
          }
        }
        break;
    }

    updateDateTime();
  }

  void updateDateTime() {
    if (_dateTimeFields.availableFields != 0x1Fu) {
      return;
    }

    auto currentMs = millis();
    unsigned long timeSinceMinuteUpdateMs = 0;
    if (currentMs >= _dateTimeFields.minute.lastUpdateMs) {
      timeSinceMinuteUpdateMs = currentMs - _dateTimeFields.minute.lastUpdateMs;
    } else {
      timeSinceMinuteUpdateMs = (ULONG_MAX - _dateTimeFields.minute.lastUpdateMs) + currentMs;
    }
    
    _currentDateTime.ms = timeSinceMinuteUpdateMs % 1000;
    _currentDateTime.second = (timeSinceMinuteUpdateMs / 1000) % 60;
    auto minuteAdjustment = (timeSinceMinuteUpdateMs / 60000);
    _currentDateTime.minute = (_dateTimeFields.minute.value + minuteAdjustment) % 60;
    auto hourAdjustment = (_dateTimeFields.minute.value + minuteAdjustment) / 60;
    _currentDateTime.hour = (_dateTimeFields.hour.value + hourAdjustment) % 24;
    auto dayAdjustment = (_dateTimeFields.hour.value + hourAdjustment) / 24;

    _currentDateTime.day = _dateTimeFields.day.value;
    _currentDateTime.month = _dateTimeFields.month.value;
    _currentDateTime.year = 2000 + _dateTimeFields.year.value;

    uint8_t leapYear = isLeapYear(_currentDateTime.year) ? 1 : 0;

    while (dayAdjustment > 0) {
      static const uint8_t DAYS_IN_MONTH[][12] = {
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }, // non leap year
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } // leap year
      };

      auto daysInMonth = DAYS_IN_MONTH[leapYear][_currentDateTime.month - 1];
      auto remainingDaysInMonth = daysInMonth - _currentDateTime.day;
      auto daysToAdd = dayAdjustment < remainingDaysInMonth ? dayAdjustment : remainingDaysInMonth;
      
      if (daysToAdd == 0) {
        daysToAdd = 1;
      }

      auto newDay = (_currentDateTime.day % daysInMonth) + daysToAdd;

      if (newDay < _currentDateTime.day) {
        auto newMonth = (_currentDateTime.month % 12) + 1;

        if (newMonth < _currentDateTime.month) {
          _currentDateTime.year = _currentDateTime.year + 1;
          leapYear = isLeapYear(_currentDateTime.year) ? 1 : 0;
        }

        _currentDateTime.month = newMonth;
      }
      
      _currentDateTime.day = newDay;

      dayAdjustment -= daysToAdd;
    }
  }

  bool isLeapYear(uint16_t year) {
    return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
  }
};

#endif
