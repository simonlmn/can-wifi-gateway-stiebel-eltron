#include <Arduino.h>
#include <SPI.h>
#include <ACAN2515.h>
#include <toolbox.h>
#include <serial_transport.h>

// CAN interface
static const int MCP2515_CS_PIN  = 5;
static const int MCP2515_INT_PIN = 255;
static const uint32_t CAN_QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL ; // 8 MHz
ACAN2515 can (MCP2515_CS_PIN, SPI, MCP2515_INT_PIN);
static bool canAvailable = false;

// Serial transport interface
serial_transport::Endpoint serial { serial_transport::EndpointRole::SERVER, Serial };

// Status LED
unsigned long HEARTBEAT_INTERVAL_DEFAULT = 500;
unsigned long HEARTBEAT_INTERVAL_OK = 1000;
unsigned long HEARTBEAT_INTERVAL_ERROR = 250;
uint8_t HEARTBEAT_LED_PIN = 8;
unsigned long _ledToggleTime = 0;
unsigned long _ledToggleInterval = 500;

void teardownCan() {
  if (!canAvailable) {
    return;
  }

  can.end();
  canAvailable = false;

  _ledToggleInterval = HEARTBEAT_INTERVAL_DEFAULT;
}

void setupCan(uint32_t canBitRate, ACAN2515Settings::RequestedMode canMode) {
  teardownCan();

  ACAN2515Settings settings (CAN_QUARTZ_FREQUENCY, canBitRate);
  settings.mRequestedMode = canMode;
  settings.mReceiveBufferSize = 16;
  settings.mTransmitBuffer0Size = 8;
  const uint16_t errorCode = can.begin(settings, NULL);
  
  canAvailable = errorCode == 0;

  if (canAvailable) {
    serial.queue(toolbox::format(F("SETUP OK %u %u %u %u %u %u %u %lu %lu"),
      settings.mRequestedMode,
      settings.mBitRatePrescaler,
      settings.mPropagationSegment,
      settings.mPhaseSegment1,
      settings.mPhaseSegment2,
      settings.mSJW,
      settings.mTripleSampling,
      (unsigned long)settings.actualBitRate(),
      (unsigned long)settings.samplePointFromBitStart()
    ));

    _ledToggleInterval = HEARTBEAT_INTERVAL_OK;
  } else {
    serial.queue(toolbox::format(F("SETUP E%04X"), errorCode));

    _ledToggleInterval = HEARTBEAT_INTERVAL_ERROR;
  }
}

void processReceived(const uint8_t* payload, uint8_t payloadLen, serial_transport::Endpoint& serial) {
  toolbox::strref part { reinterpret_cast<const char*>(payload), payloadLen };
  toolbox::strref next;

  if (part.startsWith(F("SETUP "))) {
    toolbox::Maybe<uint32_t> bitrate = toolbox::convert<uint32_t>::fromString(part.skip(6), &next, 16);
    if (!bitrate.available()) {
      serial.queue(F("SETUP ENVAL BITRATE"));
      return;
    }
    part = next;

    ACAN2515Settings::RequestedMode canMode = ACAN2515Settings::NormalMode;
    if (part == F(" NOR")) {
      canMode = ACAN2515Settings::NormalMode;
    } else if (part == F(" LOP")) {
      canMode = ACAN2515Settings::LoopBackMode;
    } else if (part == F(" SLP")) {
      canMode = ACAN2515Settings::SleepMode;
    } else if (part == F(" LIS")) {
      canMode = ACAN2515Settings::ListenOnlyMode;
    } else {
      serial.queue(F("SETUP ENVAL MODE"));
      return;
    }

    setupCan(bitrate.get(), canMode);
  } else if (part.startsWith(F("CANTX "))) {
    if (!canAvailable) {
      serial.queue(F("CANTX ENOAV"));
      return;
    }
    
    toolbox::Maybe<uint32_t> id = toolbox::convert<uint32_t>::fromString(part.skip(6), &next, 16);
    if (!id.available()) {
      serial.queue(F("CANTX ENVAL ID"));
      return;
    }
    part = next;

    toolbox::Maybe<uint8_t> len = toolbox::convert<uint8_t>::fromString(part, &next, 10);
    if (!len.available() || len.get() > 8) {
      serial.queue(F("CANTX ENVAL LEN"));
      return;
    }
    part = next;

    CANMessage frame;
    frame.id = id.get() & 0x1FFFFFFFul;
    frame.ext = (id.get() & 0x80000000ul) != 0;
    frame.rtr = (id.get() & 0x40000000ul) != 0;
    frame.len = len.get();

    for (size_t i = 0; i < frame.len; ++i) {
      toolbox::Maybe<uint8_t> byte = toolbox::convert<uint8_t>::fromString(part, &next, 16);
      if (!byte.available()) {
        serial.queue(F("CANTX ENVAL DATA"));
        return;
      }
      frame.data[i] = byte.get();
      part = next;
    }

    if (can.tryToSend(frame)) {
      serial.queue(F("CANTX OK"));
      return;
    } else {
      serial.queue(F("CANTX ESEND"));
      return;
    }
  } else {
    serial.queue(F("ERROR UNK"));
    return;
  }
}

void queueCanRxMessage(uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
  serial.queue(toolbox::format(F("CANRX %08lX %u %02X %02X %02X %02X %02X %02X %02X %02X"),
    id | (uint32_t(ext) << 31) | (uint32_t(rtr) << 30),
    length,
    data[0],
    data[1],
    data[2],
    data[3],
    data[4],
    data[5],
    data[6],
    data[7]
  ));
}

void connectionStateChanged(serial_transport::ConnectionState state, serial_transport::Endpoint& serial) {
  if (state == serial_transport::ConnectionState::CONNECTED) {
    serial.queue(F("READY"));
  } else {
    teardownCan();
  }
}

void setup() {
  toolbox::initFormatBuffers(65u, 2u);
  
  pinMode(HEARTBEAT_LED_PIN, OUTPUT);
  digitalWrite(HEARTBEAT_LED_PIN, HIGH);

  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
  SPI.begin();

  serial.setReceiveCallback(&processReceived);
  serial.setStateCallback(&connectionStateChanged);
  serial.setup();

  digitalWrite(HEARTBEAT_LED_PIN, LOW);
}

int _heartbeatLedState = LOW;

void loop() {
  serial.loop();

  if (canAvailable) {
    can.poll();

    CANMessage frame;
    while (serial.canQueue() && can.receive(frame)) {
      queueCanRxMessage(frame.id, frame.ext, frame.rtr, frame.len, frame.data);
    }
  }

  if (millis() - _ledToggleTime >= _ledToggleInterval) {
    _ledToggleTime = millis();
    _heartbeatLedState = HIGH - _heartbeatLedState;
    digitalWrite(HEARTBEAT_LED_PIN, _heartbeatLedState);
  }
}
