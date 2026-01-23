#include <ACAN2515.h>
#include <serial_transport.h>

// CAN interface
static const int MCP2515_CS_PIN  = 5;
static const int MCP2515_INT_PIN = 255;
static const uint32_t CAN_QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL ; // 8 MHz
static bool canAvailable = false;

bool doCanSetup = false; // Flag to defer CAN setup to main loop
uint32_t canBitRate = 20000ul;
ACAN2515Settings::RequestedMode canMode = ACAN2515Settings::ListenOnlyMode;
ACAN2515 can (MCP2515_CS_PIN, SPI, MCP2515_INT_PIN);

// Serial transport interface
serial_transport::Endpoint serial { serial_transport::EndpointRole::SERVER, Serial };

// Status LED
uint8_t HEARTBEAT_LED_PIN = 8;
unsigned long _ledToggleTime = 0;
unsigned long _ledToggleInterval = 500;

void teardownCan() {
  if (!canAvailable) {
    return;
  }

  can.end();
  canAvailable = false;

  _ledToggleInterval = 500;
}

void setupCan() {
  teardownCan();

  ACAN2515Settings settings (CAN_QUARTZ_FREQUENCY, canBitRate);
  settings.mRequestedMode = canMode;
  const uint16_t errorCode = can.begin(settings, NULL);
  canAvailable = errorCode == 0;

  if (canAvailable) {
    serial.queue("SETUP OK %u %u %u %u %u %u %u %lu %lu",
      settings.mRequestedMode,
      settings.mBitRatePrescaler,
      settings.mPropagationSegment,
      settings.mPhaseSegment1,
      settings.mPhaseSegment2,
      settings.mSJW,
      settings.mTripleSampling,
      (unsigned long)settings.actualBitRate(),
      (unsigned long)settings.samplePointFromBitStart()
    );

    _ledToggleInterval = 1000;
  } else {
    serial.queue("SETUP E%04X", errorCode);

    _ledToggleInterval = 250;
  }
}

void processReceived(const char* message, serial_transport::Endpoint& serial) {
  const char* start = message;
  char* end = nullptr;

  if (strncmp(start, "SETUP ", 6) == 0) {
    auto bitrate = strtol(start + 6, &end, 16);
    if (end == start) {
      serial.queue("SETUP ENVAL");
      return;
    }
    start = end;

    ACAN2515Settings::RequestedMode newCanMode = ACAN2515Settings::NormalMode;
    if (strcmp(start, " NOR") == 0) {
      newCanMode = ACAN2515Settings::NormalMode;
    } else if (strcmp(start, " LOP") == 0) {
      newCanMode = ACAN2515Settings::LoopBackMode;
    } else if (strcmp(start, " SLP") == 0) {
      newCanMode = ACAN2515Settings::SleepMode;
    } else if (strcmp(start, " LIS") == 0) {
      newCanMode = ACAN2515Settings::ListenOnlyMode;
    } else {
      serial.queue("SETUP ENVAL");
      return;
    }

    canBitRate = bitrate;
    canMode = newCanMode;

    doCanSetup = true; // Trigger CAN setup in main loop
  } else if (strncmp(start, "CANTX ", 6) == 0) {
    if (!canAvailable) {
      serial.queue("CANTX ENOAV");
      return;
    }
    
    auto id = strtol(start + 6, &end, 16);
    if (end == start) {
      serial.queue("CANTX ENVAL");
      return;
    }
    start = end;

    auto len = strtol(start, &end, 10);
    if (end == start) {
      serial.queue("CANTX ENVAL");
      return;
    }
    start = end;

    CANMessage frame;
    frame.id = id & 0x1FFFFFFFu;
    frame.ext = (id & 0x80000000u) != 0;
    frame.rtr = (id & 0x40000000u) != 0;
    frame.len = len;

    for (size_t i = 0; i < frame.len; ++i) {
      frame.data[i] = strtol(start, &end, 16);
      if (end == start) {
        serial.queue("CANTX ENVAL");
        return;
      }
      start = end;
    }

    if (can.tryToSend(frame)) {
      serial.queue("CANTX OK");
      return;
    } else {
      serial.queue("CANTX ESEND");
      return;
    }
  } else {
    serial.queue("ERROR UNK");
    return;
  }
}

void connectionStateChanged(serial_transport::ConnectionState state, serial_transport::Endpoint& serial) {
  if (state == serial_transport::ConnectionState::CONNECTED) {
    serial.queue("READY");
  } else {
    teardownCan();
  }
}

void setup() {
  pinMode(HEARTBEAT_LED_PIN, OUTPUT);
  digitalWrite(HEARTBEAT_LED_PIN, HIGH);

  serial.setReceiveCallback(&processReceived);
  serial.setStateCallback(&connectionStateChanged);
  serial.setup();
  SPI.begin();

  digitalWrite(HEARTBEAT_LED_PIN, LOW);
}

void loop() {
  serial.loop();

  if (doCanSetup) {
    doCanSetup = false;
    setupCan();
  }
    
  if (canAvailable) {
    can.poll();

    CANMessage frame;
    while (serial.canQueue() && can.receive(frame)) {
      queueCanRxMessage(serial, frame.id, frame.ext, frame.rtr, frame.len, frame.data);
    }
  }

  if (millis() - _ledToggleTime >= _ledToggleInterval) {
    _ledToggleTime = millis();
    digitalWrite(HEARTBEAT_LED_PIN, HIGH - digitalRead(HEARTBEAT_LED_PIN));
  }
}
