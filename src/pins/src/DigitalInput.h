#ifndef PINS_DIGITALINPUT_H_
#define PINS_DIGITALINPUT_H_

#include <Arduino.h>
#include "Gpios.h"

namespace pins {

enum struct InputMode {
  Normal,
  PullUp
};

class DigitalInput final {
  PinNumber _pin;
  SignalMode _signalMode;
  bool _defaultValue;

  bool _lastValue;
  unsigned long _lastChangeMs;

public:
  DigitalInput(bool defaultValue = false) : _pin(pins::gpios::NO_PIN), _defaultValue(defaultValue), _lastChangeMs(millis()) {}

  DigitalInput(PinNumber pin, InputMode inputMode = InputMode::Normal, SignalMode signalMode = SignalMode::Normal) : _pin(pin), _signalMode(signalMode), _lastChangeMs(millis()) {
    pinMode(_pin, inputMode == InputMode::PullUp ? INPUT_PULLUP : INPUT);
    read();
  }

  bool read() {
    if (_pin == pins::gpios::NO_PIN) {
      return _defaultValue;
    }
    
    bool value = digitalRead(_pin) == (_signalMode == SignalMode::Inverted ? LOW : HIGH);
    if (value != _lastValue) {
        _lastChangeMs = millis();
    }

    _lastValue = value;
    return value;
  }

  operator bool() {
    return read();
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
