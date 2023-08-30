// Feature flags
//#define DEVELOPMENT_MODE
//#define MQTT_SUPPORT

#include "src/iot-core/System.h"
#include "src/pins/DigitalInput.h"
#include "src/pins/DigitalOutput.h"
#include "src/pins/Gpios.h"
#include "SerialCan.h"
#include "StiebelEltronProtocol.h"
#include "DateTimeSource.h"
#include "DataAccess.h"
#include "RestApi.h"
#ifdef MQTT_SUPPORT
#include "MqttClient.h"
#endif

namespace io {
// Switches
#ifdef DEVELOPMENT_MODE
pins::DigitalInput otaEnablePin {true};
pins::DigitalInput writeEnablePin {true};
pins::DigitalInput txEnablePin {true};
pins::DigitalInput debugModePin {true};
#else
pins::DigitalInput otaEnablePin { pins::gpios::esp8266::nodemcu::D2, pins::InputMode::PullUp, pins::SignalMode::Inverted };
pins::DigitalInput writeEnablePin { pins::gpios::esp8266::nodemcu::D5, pins::InputMode::PullUp, pins::SignalMode::Inverted };
pins::DigitalInput txEnablePin { pins::gpios::esp8266::nodemcu::D6, pins::InputMode::PullUp, pins::SignalMode::Inverted };
pins::DigitalInput debugModePin { pins::gpios::esp8266::nodemcu::D7, pins::InputMode::PullUp, pins::SignalMode::Inverted }; // not used yet
#endif
// Buttons
pins::DigitalInput updatePin { pins::gpios::esp8266::nodemcu::D3, pins::InputMode::PullUp, pins::SignalMode::Inverted }; // not used yet
pins::DigitalInput factoryResetPin { pins::gpios::esp8266::nodemcu::D8, pins::InputMode::Normal, pins::SignalMode::Normal };

// Control lines
pins::DigitalOutput canResetPin { pins::gpios::esp8266::nodemcu::D1, false, pins::SignalMode::Inverted };
pins::DigitalOutput builtinLed { LED_BUILTIN, false, pins::SignalMode::Inverted };
}

iot_core::System sys { "fzDL9RKdPAhAhFu7", io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin };
SerialCan can { sys, io::canResetPin, io::txEnablePin };
StiebelEltronProtocol protocol { sys, can };
DateTimeSource timeSource { sys.logger(), protocol };
DataAccess access { sys, protocol, timeSource, io::writeEnablePin };
RestApi api { sys, sys, access, protocol };
#ifdef MQTT_SUPPORT
MqttClient mqtt { sys, access };
#endif

void setup() {
  sys.setDateTimeSource(&timeSource);

  sys.logger().log("ios", iot_core::format("ota=%u write=%u tx=%u debug=%u",
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
#ifdef MQTT_SUPPORT
  sys.addComponent(&mqtt);
#endif

  sys.setup();
}

void loop() {
  sys.loop();
}
