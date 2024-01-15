// OTA password
#ifndef OTA_PASSWORD
#warning Compiling in default OTA_PASSWORD
#define OTA_PASSWORD "fzDL9RKdPAhAhFu7"
#endif

// Feature flags
//#define DEVELOPMENT_MODE
#define MQTT_SUPPORT

#include <gpiobj.h>
#include "src/iot-core/System.h"
#include "Version.h"
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
gpiobj::DigitalInput otaEnablePin {true};
gpiobj::DigitalInput writeEnablePin {true};
gpiobj::DigitalInput txEnablePin {true};
gpiobj::DigitalInput debugModePin {true};
#else
gpiobj::DigitalInput otaEnablePin { gpiobj::gpios::esp8266::nodemcu::D2, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
gpiobj::DigitalInput writeEnablePin { gpiobj::gpios::esp8266::nodemcu::D5, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
gpiobj::DigitalInput txEnablePin { gpiobj::gpios::esp8266::nodemcu::D6, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
gpiobj::DigitalInput debugModePin { gpiobj::gpios::esp8266::nodemcu::D7, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
#endif
// Buttons
gpiobj::DigitalInput updatePin { gpiobj::gpios::esp8266::nodemcu::D3, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted }; // not used yet
gpiobj::DigitalInput factoryResetPin { gpiobj::gpios::esp8266::nodemcu::D8, gpiobj::InputMode::Normal, gpiobj::SignalMode::Normal };

// Control lines
gpiobj::DigitalOutput canResetPin { gpiobj::gpios::esp8266::nodemcu::D1, false, gpiobj::SignalMode::Inverted };
gpiobj::DigitalOutput builtinLed { LED_BUILTIN, false, gpiobj::SignalMode::Inverted };
}

iot_core::System sys { "can-wifi-gw", VERSION, OTA_PASSWORD, io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin, io::debugModePin };
SerialCan can { sys, io::canResetPin, io::txEnablePin };
StiebelEltronProtocol protocol { sys, can };
DateTimeSource timeSource { sys.logger(), protocol };
DataAccess access { sys, protocol, io::writeEnablePin };
RestApi api { sys, sys, access, protocol };
#ifdef MQTT_SUPPORT
MqttClient mqtt { sys, access };
#endif

void setup() {
  sys.setDateTimeSource(&timeSource);

  sys.logger().log("ios", iot_core::format(F("ota=%u write=%u tx=%u debug=%u"),
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
