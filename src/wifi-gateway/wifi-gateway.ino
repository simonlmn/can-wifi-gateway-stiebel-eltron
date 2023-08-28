//#define DEVELOPMENT_MODE

#include "ApplicationContainer.h"
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

System sys { "fzDL9RKdPAhAhFu7", io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin };
SerialCan can { sys, io::canResetPin, io::txEnablePin };
StiebelEltronProtocol protocol { sys, can };
DateTimeSource timeSource { sys, protocol };
DataAccess access { sys, protocol, timeSource, io::writeEnablePin };
RestApi api { sys, sys, access, protocol };

void setup() {
  sys.logger().log("ios", format("ota=%u write=%u tx=%u debug=%u",
    io::otaEnablePin.read(),
    io::writeEnablePin.read(),
    io::txEnablePin.read(),
    io::debugModePin.read()
    ));

  sys.addComponent(&can);
  sys.addComponent(&protocol);
  sys.addComponent(&timeSource);
  sys.addComponent(&access);
  sys.addComponent(&api);

  sys.setup();
}

void loop() {
  sys.loop();
}
