#include <ACAN2515.h>

// CAN interface
static const int MCP2515_CS_PIN  = 5;
static const int MCP2515_INT_PIN = 255;
static const uint32_t CAN_QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL ; // 8 MHz
static bool canAvailable = false;

ACAN2515 can (MCP2515_CS_PIN, SPI, MCP2515_INT_PIN);

void setup() {
  Serial.begin(57600);
  SPI.begin();

  Serial.println("READY");
}

void loop() {
  readSerial(); yield();
  
  if (canAvailable) {
    can.poll(); yield();

    CANMessage frame;
    while (can.receive(frame)) {
      Serial.print("CANRX ");
      Serial.print(frame.id | (frame.ext << 31) | (frame.rtr << 30));
      Serial.print(" ");
      Serial.print(frame.len);
      Serial.print(" ");
      for (size_t i = 0u; i < frame.len; ++i) {
        Serial.print(frame.data[i], DEC);
        Serial.print(" ");
      }
      Serial.println();
  
      yield();
    }
  }  
}

const size_t RECEIVE_BUFFER_SIZE = 256;
char receiveBuffer[RECEIVE_BUFFER_SIZE];
size_t receiveIndex = 0u;

void readSerial() {
  auto bytesAvailable = Serial.available();
  if (bytesAvailable <= 0) {
    return;
  }

  while (bytesAvailable > 0) {
    auto receivedByte = Serial.read();
    if (receivedByte == '\n' && receiveIndex > 0 && receiveBuffer[receiveIndex - 1] == '\r') {
      // \r\n received, process line
      receiveBuffer[receiveIndex - 1] = '\0';
      String line (receiveBuffer);
      
      processReceivedLine(line);

      receiveIndex = 0;
    } else {
      receiveBuffer[receiveIndex] = receivedByte;
      receiveIndex += 1;
    }

    receiveIndex = receiveIndex % RECEIVE_BUFFER_SIZE;
    
    bytesAvailable -= 1;
  }
}

void processReceivedLine(const String& line) {
  auto type = line.substring(0, 5);
  if (type == "SETUP") {
    auto bitrateEndIndex = line.indexOf(' ', 6);
    auto bitrate = line.substring(6, bitrateEndIndex).toInt();

    ACAN2515Settings settings (CAN_QUARTZ_FREQUENCY, bitrate);
    
    auto modeEndIndex = line.indexOf(' ', bitrateEndIndex + 1);
    auto mode = line.substring(bitrateEndIndex + 1, modeEndIndex);

    if (mode.equals("NormalMode")) {
      settings.mRequestedMode = ACAN2515Settings::NormalMode;
    } else if (mode.equals("LoopBackMode")) {
      settings.mRequestedMode = ACAN2515Settings::LoopBackMode;
    } else if (mode.equals("SleepMode")) {
      settings.mRequestedMode = ACAN2515Settings::SleepMode;
    } else if (mode.equals("ListenOnlyMode")) {
      settings.mRequestedMode = ACAN2515Settings::ListenOnlyMode;
    } else {
      settings.mRequestedMode = ACAN2515Settings::LoopBackMode;
      // TODO abort and report error?
    }

    if (canAvailable) {
      can.end();
      canAvailable = false;
    }
    
    const uint16_t errorCode = can.begin(settings, NULL);
    canAvailable = errorCode == 0;
  
    if (canAvailable) {
      Serial.print("SETUP OK ");
      Serial.print(line.substring(6));
      Serial.print(": mRequestedMode="); Serial.print(settings.mRequestedMode);
      Serial.print(" mBitRatePrescaler="); Serial.print(settings.mBitRatePrescaler);
      Serial.print(" mPropagationSegment="); Serial.print(settings.mPropagationSegment);
      Serial.print(" mPhaseSegment1="); Serial.print(settings.mPhaseSegment1);
      Serial.print(" mPhaseSegment2="); Serial.print(settings.mPhaseSegment2);
      Serial.print(" mSJW="); Serial.print(settings.mSJW);
      Serial.print(" mTripleSampling="); Serial.print(settings.mTripleSampling);
      Serial.print(" actualBitRate="); Serial.print((unsigned long)settings.actualBitRate());
      Serial.print(" exactBitRate="); Serial.print(settings.exactBitRate());
      Serial.print(" samplePointFromBitStart="); Serial.print((unsigned long)settings.samplePointFromBitStart());
      Serial.println();
    } else {
      Serial.print("SETUP ERROR ");
      Serial.print(line.substring(6));
      Serial.print(": 0x");
      Serial.println(errorCode, HEX);
    }
  } else if (type == "CANTX") {
    if (!canAvailable) {
      Serial.print("CANTX ENOAV ");
      Serial.print(line.substring(6));
      Serial.println();
    }
    
    auto idEndIndex = line.indexOf(' ', 6);
    auto id = line.substring(6, idEndIndex).toInt();
    auto lenEndIndex = line.indexOf(' ', idEndIndex + 1);
    auto len = line.substring(idEndIndex + 1, lenEndIndex).toInt();
    CANMessage message;
    message.id = id & 0x1FFFFFFFu;
    message.ext = (id & 0x80000000u) != 0;
    message.rtr = (id & 0x40000000u) != 0;
    message.len = len;

    auto prevDataEndIndex = lenEndIndex;
    for (size_t i = 0; i < message.len; ++i) {
      auto dataEndIndex = line.indexOf(' ', prevDataEndIndex + 1);
      message.data[i] = line.substring(prevDataEndIndex + 1, dataEndIndex).toInt();
      prevDataEndIndex = dataEndIndex;
    }

    if (can.tryToSend(message)) {
      Serial.print("CANTX OK ");
      Serial.print(line.substring(6));
      Serial.println();
    } else {
      Serial.print("CANTX ESEND ");
      Serial.print(line.substring(6));
      Serial.println();
    }
  } else {
    Serial.print("ERROR ");
    Serial.print(line);
    Serial.println();
  }
}
