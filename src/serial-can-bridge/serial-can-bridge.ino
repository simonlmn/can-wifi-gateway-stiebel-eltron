#include <ACAN2515.h>
#include "src/shared/SerialProtocol.h"

// CAN interface
static const int MCP2515_CS_PIN  = 5;
static const int MCP2515_INT_PIN = 255;
static const uint32_t CAN_QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL ; // 8 MHz
static bool canAvailable = false;

ACAN2515 can (MCP2515_CS_PIN, SPI, MCP2515_INT_PIN);

SerialProtocol serial { processReceivedLine };

void processReceivedLine(const char* line) {
  const char* start = line;
  char* end = nullptr;

  if (strncmp(start, "SETUP ", 6) == 0) {
    auto bitrate = strtol(start + 6, &end, 16);
    if (end == start) {
      serial.send("SETUP ENVAL %s", start + 6);
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
      serial.send("SETUP ENVAL %s", start);
      return;
    }

    if (canAvailable) {
      can.end();
      canAvailable = false;
    }
    
    const uint16_t errorCode = can.begin(settings, NULL);
    canAvailable = errorCode == 0;
  
    if (canAvailable) {
      serial.send("SETUP OK RM=%u BRP=%u PRS=%u PS1=%u PS2=%u SJW=%u TRS=%u ABR=%u SPS=%u",
        settings.mRequestedMode,
        settings.mBitRatePrescaler,
        settings.mPropagationSegment,
        settings.mPhaseSegment1,
        settings.mPhaseSegment2,
        settings.mSJW,
        settings.mTripleSampling,
        (unsigned long)settings.actualBitRate(),
        settings.exactBitRate(),
        (unsigned long)settings.samplePointFromBitStart()
      );
      return;
    } else {
      serial.send("SETUP E%04X: %s", errorCode, line + 6);
      return;
    }
  } else if (strncmp(start, "CANTX ", 6) == 0) {
    if (!canAvailable) {
      serial.send("CANTX ENOAV %s", start + 6);
      return;
    }
    
    auto id = strtol(start + 6, &end, 16);
    if (end == start) {
      serial.send("CANTX ENVAL %s", start + 6);
      return;
    }
    start = end;

    auto len = strtol(start, &end, 10);
    if (end == start) {
      serial.send("CANTX ENVAL %s", start);
      return;
    }
    start = end;

    CANMessage message;
    message.id = id & 0x1FFFFFFFu;
    message.ext = (id & 0x80000000u) != 0;
    message.rtr = (id & 0x40000000u) != 0;
    message.len = len;

    for (size_t i = 0; i < message.len; ++i) {
      message.data[i] = strtol(start, &end, 16);
      if (end == start) {
        serial.send("CANTX ENVAL %s", start);
        return;
      }
      start = end;
    }

    if (can.tryToSend(message)) {
      serial.send("CANTX OK %s", line + 6);
      return;
    } else {
      serial.send("CANTX ESEND %s", line + 6);
      return;
    }
  } else {
    serial.send("ERROR %s", line);
    return;
  }
}

void setup() {
  serial.setup();
  SPI.begin();
  serial.send("READY");
}

void loop() {
  serial.receive();
  
  if (canAvailable) {
    can.poll();

    CANMessage frame;
    while (can.receive(frame)) {
      yield();
      serial.send("CANRX %08X %u %02X %02X %02X %02X %02X %02X %02X %02X",
        frame.id | (frame.ext << 31) | (frame.rtr << 30),
        frame.len,
        frame.data[0],
        frame.data[1],
        frame.data[2],
        frame.data[3],
        frame.data[4],
        frame.data[5],
        frame.data[6],
        frame.data[7]
      );
    }
  }  
}
