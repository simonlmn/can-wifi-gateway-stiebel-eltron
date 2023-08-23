//#define DEVELOPMENT_MODE

#include "NodeBase.h"
#include "SerialCan.h"
#include "StiebelEltronProtocol.h"
#include "DateTimeSource.h"
#include "DataAccess.h"
#include "RestApi.h"
#include "Utils.h"
#include "src/shared/Pins.h"

namespace io {
// Switches
#ifdef DEVELOPMENT_MODE
DigitalInput otaEnablePin {true};
DigitalInput writeEnablePin {true};
DigitalInput txEnablePin {true};
DigitalInput debugModePin {true};
#else
DigitalInput otaEnablePin { pins::esp8266::nodemcu::D2, InputMode::PullUp, SignalMode::Inverted };
DigitalInput writeEnablePin { pins::esp8266::nodemcu::D5, InputMode::PullUp, SignalMode::Inverted };
DigitalInput txEnablePin { pins::esp8266::nodemcu::D6, InputMode::PullUp, SignalMode::Inverted };
DigitalInput debugModePin { pins::esp8266::nodemcu::D7, InputMode::PullUp, SignalMode::Inverted }; // not used yet
#endif
// Buttons
DigitalInput updatePin { pins::esp8266::nodemcu::D3, InputMode::PullUp, SignalMode::Inverted }; // not used yet
DigitalInput factoryResetPin { pins::esp8266::nodemcu::D8, InputMode::Normal, SignalMode::Normal };

// Control lines
DigitalOutput canResetPin { pins::esp8266::nodemcu::D1, false, SignalMode::Inverted };
DigitalOutput builtinLed { LED_BUILTIN, false, SignalMode::Inverted };
}

NodeBase node ("fzDL9RKdPAhAhFu7", io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin);
SerialCan can (node, io::canResetPin, io::txEnablePin);
StiebelEltronProtocol protocol (node, can);
DateTimeSource timeSource (node, protocol);
DataAccess access (node, protocol, timeSource, io::writeEnablePin);
RestApi api (node, access, protocol);

void setup() {
  node.init(
    [] (bool connected) {
      can.setup(); yield();
      protocol.setup(); yield();
      timeSource.setup(); yield();
      access.setup(); yield();

      if (connected) {
        api.setup(); yield();
      }
    },
    [] (ConnectionStatus status) {
      can.loop(); node.lyield();
      protocol.loop(); node.lyield();
      timeSource.loop(); node.lyield();
      access.loop(); node.lyield();

      switch (status) {
        case ConnectionStatus::Connecting:
          api.begin();
          break;
        case ConnectionStatus::Connected:
          api.loop(); node.lyield();
          break;
        case ConnectionStatus::Disconnecting:
          api.end();
          break;
      }
    });

  node.setup();

  node.log("ios", format("ota=%u write=%u tx=%u debug=%u",
    io::otaEnablePin.read(),
    io::writeEnablePin.read(),
    io::txEnablePin.read(),
    io::debugModePin.read()
    ));
}

void loop() {
  node.loop();
}
