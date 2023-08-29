#ifndef PINS_GPIOS_H_
#define PINS_GPIOS_H_

#include <Arduino.h>

namespace pins {

enum struct SignalMode {
  Normal,
  Inverted
};

using PinNumber = int;

namespace gpios {
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

namespace gpios::esp8266::nodemcu {
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

}

#endif
