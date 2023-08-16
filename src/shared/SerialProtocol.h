#pragma once

#include <Arduino.h>

class SerialProtocol;

#if defined(ARDUINO_AVR_NANO)
using SerialReceiveCallback = void (*)(const char* line, SerialProtocol& serial);
#else
#include <functional>
using SerialReceiveCallback = std::function<void(const char* line, SerialProtocol& serial)>;
#endif

// Serial messages:
// READY\r\n
// SETUP FFFFFFFF NOR|LOP|SLP|LIS\r\n
// SETUP OK RM=9 BRP=64 PRS=8 PS1=8 PS2=8 SJW=4 TRS=1 ABR=1234567890 EBR=1 SPS=1234567890\r\n
// SETUP EFFFF: FFFFFFFF NOR|LOP|SLP|LIS\r\n
// CANRX FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
// CANTX FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
// CANTX OK FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
// CANTX ENVAL FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
// CANTX ESEND FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
// CANTX ENOAV FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
// ERROR\r\n

class SerialProtocol final {
private:
    static const size_t SERIAL_MESSAGE_SIZE = 96u; 
    char SERIAL_TX_MESSAGE[SERIAL_MESSAGE_SIZE];
    char SERIAL_RX_MESSAGE[SERIAL_MESSAGE_SIZE];
    size_t _serialRxMessageEnd = 0u;

    SerialReceiveCallback _processReceivedLine;

public:
    SerialProtocol(SerialReceiveCallback processReceivedLine);

    void setup();
    void reset();
    void send(const char* fmt, ...);
    void receive();
};

// Helper functions to send CAN messages from both sides

void sendTxMessage(SerialProtocol& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]);

void sendRxMessage(SerialProtocol& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]);
