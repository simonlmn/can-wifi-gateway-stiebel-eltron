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
  iot_core::Logger _logger;
  StiebelEltronProtocol& _protocol;
  const IConversionService& _conversion;
  DateTimeFields _dateTimeFields;
  iot_core::DateTime _currentDateTime;

  struct ValueDefinitionsConfig {
    DeviceId timeSourceId = SYSTEM_ID;
    ValueId dayId = 0x0122;
    ValueId monthId = 0x0123;
    ValueId yearId = 0x0124;
    ValueId hourId = 0x0125;
    ValueId minuteId = 0x0126;
  } _config;

public:
  DateTimeSource(iot_core::Logger logger, StiebelEltronProtocol& protocol, const IConversionService& conversion)
    : _logger(logger),
    _protocol(protocol),
    _conversion(conversion),
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
  unsigned long _requestDateTimeFieldIntervalMs = 10000;
  unsigned long _dateTimeFieldAgeThresholdMs = 30000;
  unsigned long _lastRequestDateTimeFields = 0;
  
  void requestDateTimeFields() {
    auto currentMs = millis();
    if (currentMs > (_lastRequestDateTimeFields + _requestDateTimeFieldIntervalMs)) {
      if (currentMs > _dateTimeFields.minute.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), _config.timeSourceId, _config.minuteId });
      }

      if (currentMs > _dateTimeFields.hour.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), _config.timeSourceId, _config.hourId });
      }

      if (currentMs > _dateTimeFields.day.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), _config.timeSourceId, _config.dayId });
      }

      if (currentMs > _dateTimeFields.month.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), _config.timeSourceId, _config.monthId });
      }

      if (currentMs > _dateTimeFields.year.lastUpdateMs + _dateTimeFieldAgeThresholdMs) {
        _protocol.request({ deviceId(), _config.timeSourceId, _config.yearId });
      }

      _lastRequestDateTimeFields = currentMs;
    }
  }

  void processData(ValueId valueId, uint16_t value) {
    if (isDateTimeField(valueId)) {
      bool availableBefore = available();
      updateDateTimeField(valueId, value);
      if (!availableBefore && available()) {
        _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Date and time acquired: %s"), _currentDateTime.toString()));
      }
    }
  }

  bool isDateTimeField(ValueId valueId) {
    return valueId == _config.dayId
        || valueId == _config.monthId
        || valueId == _config.yearId
        || valueId == _config.hourId
        || valueId == _config.minuteId;
  }

  toolbox::Maybe<int32_t> decode(ValueId id, uint16_t value) const {
    return _conversion.getConversion(id).codec().decode(value);
  }

  void updateDateTimeField(ValueId valueId, uint16_t value) {
    if (valueId == _config.yearId) {
      auto year = decode(valueId, value);
      if (year) {
        _dateTimeFields.year.value = year.get();
        _dateTimeFields.year.lastUpdateMs = millis();
        _dateTimeFields.availableFields |= 1;
      }
    } else if (valueId == _config.monthId) {
      auto month = decode(valueId, value);
      if (month) {
        _dateTimeFields.month.value = month.get();
        _dateTimeFields.month.lastUpdateMs = millis();
        _dateTimeFields.availableFields |= 2;
      }
    } else if (valueId == _config.dayId) {
      auto day = decode(valueId, value);
      if (day) {
        _dateTimeFields.day.value = day.get();
        _dateTimeFields.day.lastUpdateMs = millis();
        _dateTimeFields.availableFields |= 4;
      }
    } else if (valueId == _config.hourId) {
      auto hour = decode(valueId, value);
      if (hour) {
        _dateTimeFields.hour.value = hour.get();
        _dateTimeFields.hour.lastUpdateMs = millis();
        _dateTimeFields.availableFields |= 8;
      }
    } else if (valueId == _config.minuteId) {
      auto minute = decode(valueId, value);
      if (minute && (minute.get() != _dateTimeFields.minute.value || _dateTimeFields.minute.lastUpdateMs == 0)) {
        _dateTimeFields.minute.value = minute.get();
        _dateTimeFields.minute.lastUpdateMs = millis();
        _dateTimeFields.availableFields |= 16;
      }
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
