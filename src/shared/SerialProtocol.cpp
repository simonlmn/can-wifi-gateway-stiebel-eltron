#include "SerialProtocol.h"

#if defined(ARDUINO_AVR_NANO)
#include <stddef.h>
#else
#include <cstddef>
#include <algorithm>
using std::min;
#endif

SerialProtocol::SerialProtocol(SerialReceiveCallback processReceivedLine) : _processReceivedLine(processReceivedLine) {}

void SerialProtocol::setup() {
    Serial.begin(115200);
    Serial.setTimeout(2);
}

void SerialProtocol::reset() {
    _serialRxMessageEnd = 0u;
    Serial.flush();
    size_t available = Serial.available();
    while (available-- > 0) {
        Serial.read();
        yield();
    }
}

void SerialProtocol::send(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    size_t actualLength = vsnprintf(SERIAL_TX_MESSAGE, SERIAL_MESSAGE_SIZE, fmt, args);
    va_end(args);
    actualLength = min(actualLength, SERIAL_MESSAGE_SIZE - 2);
    SERIAL_TX_MESSAGE[actualLength] = '\r';
    SERIAL_TX_MESSAGE[actualLength + 1] = '\n';
    Serial.write(SERIAL_TX_MESSAGE, actualLength + 2);
}

void SerialProtocol::receive() {
    auto bytesRead = Serial.readBytes(SERIAL_RX_MESSAGE + _serialRxMessageEnd, SERIAL_MESSAGE_SIZE - _serialRxMessageEnd);
    auto newRxMessageEnd = _serialRxMessageEnd + bytesRead;

    char* newLine = reinterpret_cast<char*>(memchr(SERIAL_RX_MESSAGE + _serialRxMessageEnd, '\n', bytesRead));
    if (newLine != nullptr) {
        if (newLine != SERIAL_RX_MESSAGE && *(newLine - 1) == '\r') {
            *(newLine - 1) = '\0';
            
            _processReceivedLine(SERIAL_RX_MESSAGE, *this);

            size_t remainingCount = (SERIAL_RX_MESSAGE + newRxMessageEnd) - (newLine + 1);
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

void sendCanMessage(SerialProtocol& serial, const char* direction, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    serial.send("CAN%s %08lX %u %02X %02X %02X %02X %02X %02X %02X %02X",
        direction,
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
      );
}

void sendTxMessage(SerialProtocol& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    sendCanMessage(serial, "TX", id, ext, rtr, length, data);
}

void sendRxMessage(SerialProtocol& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    sendCanMessage(serial, "RX", id, ext, rtr, length, data);
}
