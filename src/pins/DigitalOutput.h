#ifndef PINS_DIGITALOUTPUT_H_
#define PINS_DIGITALOUTPUT_H_

#include <Arduino.h>
#include "Gpios.h"

namespace pins {

class DigitalOutput final {
  PinNumber _pin;
  SignalMode _signalMode;

  bool _lastValue;
  unsigned long _lastChangeMs;

public:
  DigitalOutput() : _pin(pins::gpios::NO_PIN) {}

  DigitalOutput(PinNumber pin, bool initialValue = false, SignalMode signalMode = SignalMode::Normal) : _pin(pin), _signalMode(signalMode), _lastChangeMs(millis()) {
    pinMode(_pin, OUTPUT);
    write(initialValue);
  }

  void write(bool value) {
    if (_pin == pins::gpios::NO_PIN) {
      return;
    }

    if (value != _lastValue) {
        _lastChangeMs = millis();
        _lastValue = value;
    }

    value = (_signalMode == SignalMode::Inverted) ? !value : value;
    
    digitalWrite(_pin, value ? HIGH : LOW);
  }

  DigitalOutput& operator=(bool value) {
    write(value);
    return *this;
  }

  operator bool() const {
    return lastValue();
  }

  bool lastValue() const {
    return _lastValue;
  }

  void trigger(bool value, unsigned long durationMs) {
    write(value);
    delay(durationMs);
    write(!value);
  }

  void toggleIfUnchangedFor(unsigned long durationMs) {
    if (hasNotChangedFor(durationMs)) {
      write(!_lastValue);
    }
  }

  unsigned long lastChange() const {
    return _lastChangeMs;
  }

  bool hasChangedSince(unsigned long timestampMs) const {
    return _lastChangeMs > timestampMs;
  }

  bool hasNotChangedSince(unsigned long timestampMs) const {
    return _lastChangeMs <= timestampMs;
  }

  bool hasNotChangedFor(unsigned long durationMs) const {
    return (millis() - _lastChangeMs) > durationMs;
  }
};

}

#endif
