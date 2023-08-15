#pragma once

#include <Arduino.h>

using PinNumber = int;

namespace pins {
constexpr PinNumber NO_PIN = NOT_A_PIN;
constexpr PinNumber GPIO__0 = 0;
constexpr PinNumber GPIO__1 = 1;
constexpr PinNumber GPIO__2 = 2;
constexpr PinNumber GPIO__3 = 3;
constexpr PinNumber GPIO__4 = 4;
constexpr PinNumber GPIO__5 = 5;
constexpr PinNumber GPIO__6 = 6;
constexpr PinNumber GPIO__7 = 6;
constexpr PinNumber GPIO__8 = 6;
constexpr PinNumber GPIO__9 = 6;
constexpr PinNumber GPIO_10 = 10;
constexpr PinNumber GPIO_11 = 11;
constexpr PinNumber GPIO_12 = 12;
constexpr PinNumber GPIO_13 = 13;
constexpr PinNumber GPIO_14 = 14;
constexpr PinNumber GPIO_15 = 15;
constexpr PinNumber GPIO_16 = 16;
}

namespace pins::esp8266::nodemcu {
constexpr PinNumber D0 = GPIO_16;
constexpr PinNumber D1 = GPIO__5;
constexpr PinNumber D2 = GPIO__4;
constexpr PinNumber D3 = GPIO__0;
constexpr PinNumber D4 = GPIO__2;
constexpr PinNumber D5 = GPIO_14;
constexpr PinNumber D6 = GPIO_12;
constexpr PinNumber D7 = GPIO_13;
constexpr PinNumber D8 = GPIO_15;
}

void optionalPinMode(PinNumber pin, int mode) {
  if (pin == pins::NO_PIN) {
    return;
  }

  pinMode(pin, mode);
}

int optionalDigitalRead(PinNumber pin, int defaultValue=LOW) {
  if (pin == pins::NO_PIN) {
    return defaultValue;
  }

  return digitalRead(pin);
}

enum struct InputMode {
  Normal,
  PullUp
};

enum struct SignalMode {
  Normal,
  Inverted
};

class DigitalInput final {
  PinNumber _pin;
  SignalMode _signalMode;
  bool _defaultValue;

  bool _lastValue;
  unsigned long _lastChangeMs;

public:
  DigitalInput(bool defaultValue = false) : _pin(pins::NO_PIN), _defaultValue(defaultValue), _lastChangeMs(millis()) {}

  DigitalInput(PinNumber pin, InputMode inputMode = InputMode::Normal, SignalMode signalMode = SignalMode::Normal) : _pin(pin), _signalMode(signalMode), _lastChangeMs(millis()) {
    pinMode(_pin, inputMode == InputMode::PullUp ? INPUT_PULLUP : INPUT);
    read();
  }

  bool read() {
    if (_pin == pins::NO_PIN) {
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

class DigitalOutput final {
  PinNumber _pin;
  SignalMode _signalMode;

  bool _lastValue;
  unsigned long _lastChangeMs;

public:
  DigitalOutput() : _pin(pins::NO_PIN) {}

  DigitalOutput(PinNumber pin, bool initialValue = false, SignalMode signalMode = SignalMode::Normal) : _pin(pin), _signalMode(signalMode), _lastChangeMs(millis()) {
    pinMode(_pin, OUTPUT);
    write(initialValue);
  }

  void write(bool value) {
    if (_pin == pins::NO_PIN) {
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
