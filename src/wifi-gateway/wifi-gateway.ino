// OTA password
#ifndef OTA_PASSWORD
#warning Compiling in default OTA_PASSWORD
#define OTA_PASSWORD "fzDL9RKdPAhAhFu7"
#endif

// Feature flags
//#define DEVELOPMENT_MODE
#define MQTT_SUPPORT

#include <toolbox.h>
#include <gpiobj.h>
#include <iot_core.h>
#include <iot_core/api/Server.h>
#include <iot_core/api/SystemApi.h>
#include "AppVersion.h"
#include "SerialCan.h"
#include "StiebelEltronProtocol.h"
#include "ValueDefinitions.h"
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

iot_core::System sys { "can-wifi-gw", APP_VERSION, OTA_PASSWORD, io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin, io::debugModePin };
iot_core::api::Server api { sys };
iot_core::api::SystemApi systemApi { sys, sys };

SerialCan can { sys, io::canResetPin, io::txEnablePin };
StiebelEltronProtocol protocol { sys, can };
ConversionRepository conversions { sys };
DefinitionRepository definitions { sys, conversions };
ConversionService conversionService { conversions, definitions };
DateTimeSource timeSource { sys.logger("dts"), protocol, conversionService };
DataAccess access { sys, protocol, definitions, io::writeEnablePin };
RestApi appApi { sys, access, protocol, conversionService, conversions, definitions };
#ifdef MQTT_SUPPORT
MqttClient mqtt { sys, access, conversionService, definitions };
#endif

void setup() {
  sys.setDateTimeSource(&timeSource);

  sys.logs().log("ios", toolbox::format(F("ota=%u write=%u tx=%u debug=%u"),
    io::otaEnablePin.read(),
    io::writeEnablePin.read(),
    io::txEnablePin.read(),
    io::debugModePin.read()
  ));

  sys.addComponent(&api);
  sys.addComponent(&can);
  sys.addComponent(&protocol);
  sys.addComponent(&conversions);
  sys.addComponent(&definitions);
  sys.addComponent(&timeSource);
  sys.addComponent(&access);
#ifdef MQTT_SUPPORT
  sys.addComponent(&mqtt);
#endif

  api.addProvider(&systemApi);
  api.addProvider(&appApi);

  sys.setup();
}

void loop() {
  sys.loop();
}
