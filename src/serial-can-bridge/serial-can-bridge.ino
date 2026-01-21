#include <ACAN2515.h>
#include <serial_transport.h>

// CAN interface
static const int MCP2515_CS_PIN  = 5;
static const int MCP2515_INT_PIN = 255;
static const uint32_t CAN_QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL ; // 8 MHz
static bool canAvailable = false;

ACAN2515 can (MCP2515_CS_PIN, SPI, MCP2515_INT_PIN);

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

    ACAN2515Settings settings (CAN_QUARTZ_FREQUENCY, bitrate);
    
    if (strcmp(start, " NOR") == 0) {
      settings.mRequestedMode = ACAN2515Settings::NormalMode;
    } else if (strcmp(start, " LOP") == 0) {
      settings.mRequestedMode = ACAN2515Settings::LoopBackMode;
    } else if (strcmp(start, " SLP") == 0) {
      settings.mRequestedMode = ACAN2515Settings::SleepMode;
    } else if (strcmp(start, " LIS") == 0) {
      settings.mRequestedMode = ACAN2515Settings::ListenOnlyMode;
    } else {
      serial.queue("SETUP ENVAL");
      return;
    }

    if (canAvailable) {
      can.end();
      canAvailable = false;
    }
    
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
      return;
    } else {
      serial.queue("SETUP E%04X", errorCode);
      return;
    }
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
    // Don't echo potentially corrupt data - just report unknown command
    serial.queue("ERROR UNK");
    return;
  }
}

void handleError(serial_transport::ErrorCode errorCode, char detail, serial_transport::Endpoint& serial) {
  serial.queue("ERROR %c %c", static_cast<char>(errorCode), detail);
}

serial_transport::Endpoint serial { processReceived, handleError };

void setup() {
  serial.setup();
  SPI.begin();
  serial.queue("READY");
}

void loop() {
  serial.loop();
  
  if (canAvailable) {
    can.poll();

    CANMessage frame;
    while (serial.canQueue() && can.receive(frame)) {
      queueCanRxMessage(serial, frame.id, frame.ext, frame.rtr, frame.len, frame.data);
    }
  }
}
