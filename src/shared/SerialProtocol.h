#pragma once

#include <functional>
#include <algorithm>

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

class SerialProtocol {
    static const size_t SERIAL_MESSAGE_SIZE = 96u; 
    char SERIAL_TX_MESSAGE[SERIAL_MESSAGE_SIZE];
    char SERIAL_RX_MESSAGE[SERIAL_MESSAGE_SIZE];
    size_t _serialRxMessageEnd = 0u;

    std::function<void(const char* line)> _processReceivedLine;

public:
    SerialProtocol(std::function<void(const char* line)> processReceivedLine) : _processReceivedLine(processReceivedLine) {}

    void setup() {
        Serial.begin(57600);
        Serial.setTimeout(2);
    }

    void reset() {
        _serialRxMessageEnd = 0u;
        Serial.flush();
        size_t available = Serial.available();
        while (available-- > 0) {
            Serial.read();
            yield();
        }
    }

    void send(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        size_t actualLength = vsnprintf(SERIAL_TX_MESSAGE, SERIAL_MESSAGE_SIZE, fmt, args);
        va_end(args);
        auto actualLength = std::min(actualLength, SERIAL_MESSAGE_SIZE - 2);
        SERIAL_TX_MESSAGE[actualLength] = '\r';
        SERIAL_TX_MESSAGE[actualLength + 1] = '\n';
        Serial.write(SERIAL_TX_MESSAGE, actualLength + 1);
    }

    void receive() {
        auto bytesRead = Serial.read(SERIAL_RX_MESSAGE + _serialRxMessageEnd, SERIAL_MESSAGE_SIZE - _serialRxMessageEnd);
        auto newRxMessageEnd = _serialRxMessageEnd + bytesRead;

        char* newLine = reinterpret_cast<char*>(memchr(SERIAL_RX_MESSAGE + _serialRxMessageEnd, '\n', bytesRead));
        if (newLine != nullptr) {
            if (newLine != SERIAL_RX_MESSAGE && *(newLine - 1) == '\r') {
                *(newLine - 1) = '\0';
                
                _processReceivedLine(SERIAL_RX_MESSAGE);

                size_t remainingCount = (SERIAL_RX_MESSAGE + newRxMessageEnd) - newLine;
                memmove(SERIAL_RX_MESSAGE, newLine + 1, remainingCount);
                _serialRxMessageEnd = remainingCount;
            } else {
                send("ERROR invalid line terminator");
                _serialRxMessageEnd = 0u;
            }
        } else if (newRxMessageEnd >= SERIAL_MESSAGE_SIZE) {
            send("ERROR no line terminator");
            _serialRxMessageEnd = 0u;
        } else {
            _serialRxMessageEnd = newRxMessageEnd;
        }
    }
};
