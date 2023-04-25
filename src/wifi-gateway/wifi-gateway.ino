#include "NodeBase.h"
#include "SerialCan.h"
#include "StiebelEltronProtocol.h"
#include "DateTimeSource.h"
#include "DataAccess.h"
#include "RestApi.h"

NodeBase node ("fzDL9RKdPAhAhFu7");

SerialCan can (node, 5);

StiebelEltronProtocol protocol (node, can, 0u);

DateTimeSource timeSource (node, protocol);

DataAccess access (node, protocol, timeSource);

RestApi api (node, access);

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
}

void loop() {
  node.loop();
}
