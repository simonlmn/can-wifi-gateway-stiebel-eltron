#include "SerialProtocol.h"

#if defined(ARDUINO_AVR_NANO)
#include <stddef.h>
#else
#include <cstddef>
#include <algorithm>
using std::min;
#endif

static const char BEGIN_CONTROL_MESSAGE = '#';
static const char BEGIN_CONTROL_RESPONSE_MESSAGE = '>';
static const char BEGIN_NORMAL_MESSAGE = '+';
static const char TERMINATOR[] = "\r\n";

static const char CONTROL_ACKNOWLEDGE = '=';
static const char CONTROL_ERROR = '!';
static const char CONTROL_TIMEOUT = 'T';
static const char CONTROL_DEBUG = 'D';

const char* errorCodeDescription(SerialErrorCode errorCode) {
    switch (errorCode) {
        case SerialErrorCode::InvalidMessageTermination: return "Invalid or no message termination";
        case SerialErrorCode::InvalidMessageStart: return "Invalid message start indicator";
        case SerialErrorCode::InvalidMessageSize: return "Invalid message size";
        case SerialErrorCode::InvalidControlMessage: return "Unknown control message";
        case SerialErrorCode::InvalidTimeoutControl: return "Unknown timeout control operation";
        case SerialErrorCode::InvalidSequenceCounter: return "Invalid sequence counter";
        case SerialErrorCode::ResendLimitExceeded: return "Resend limit exceeded";
        default: return "Unknown error code";
    }
}

SerialProtocol::SerialProtocol(SerialReceiveCallback processReceived, SerialErrorCallback handleError) :
    _processReceived(processReceived),
    _handleError(handleError) {}

void SerialProtocol::setup() {
    Serial.begin(115200);
    Serial.setTimeout(2);
}

void SerialProtocol::reset() {
    _rxBufferSize = 0u;
    _lastTxQueueIndex = 0u;
    _currentTxSequenceNumber = MAX_SEQUENCE_NUMBER;
    _currentRxSequenceNumber = 0u;
    _currentUnacknowledgedTxQueueIndex = 0u;
    for (auto& message : _txQueue) {
        message = QueuedMessage{};
    }
    
    Serial.flush();
    size_t available = Serial.available();
    while (available-- > 0) {
        Serial.read();
        yield();
    }
}

void SerialProtocol::loop() {
    receive();

    if (_timeoutEnabled && txMessageQueued()) {
        QueuedMessage& currentMessage = currentUnacknowledgedMessage();
        if ((currentMessage.sendTime + _timeout) < millis()) {
            if (currentMessage.retries > 0u) {
                send(currentMessage);
                currentMessage.retries -= 1u;
            } else {
                sendError(SerialErrorCode::ResendLimitExceeded);
                acknowledgeCurrentUnacknowledgedMessage();
            }
        }
    }
}

bool SerialProtocol::canQueue() const {
    return _txQueue[nextTxQueueIndex()].acknowledged;
}

bool SerialProtocol::canSend(size_t queueIndex) const {
    return _currentUnacknowledgedTxQueueIndex == queueIndex;
}

bool SerialProtocol::queue(const char* fmt, ...) {
    if (!canQueue()) {
        return false;
    }

    QueuedMessage& message = _txQueue[incrementLastTxQueueIndex()];
    message.sequenceNumber = incrementTxSequenceNumber();
    message.acknowledged = false;
    message.sendTime = 0u;
    message.retries = _resendLimit;

    va_list args;
    va_start(args, fmt);
    message.payloadSize = vsnprintf(message.payload, PAYLOAD_MAX_SIZE, fmt, args);
    va_end(args);

    if (canSend(_lastTxQueueIndex)) {
        send(message);
    }

    return true;
}

bool queueCanMessage(SerialProtocol& serial, const char* direction, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    return serial.queue("CAN%s %08lX %u %02X %02X %02X %02X %02X %02X %02X %02X",
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

bool queueCanTxMessage(SerialProtocol& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    return queueCanMessage(serial, "TX", id, ext, rtr, length, data);
}

bool queueCanRxMessage(SerialProtocol& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]) {
    return queueCanMessage(serial, "RX", id, ext, rtr, length, data);
}

void SerialProtocol::send(QueuedMessage& message) {
    message.acknowledged = false;
    message.sendTime = millis();

    Serial.write(BEGIN_NORMAL_MESSAGE);
    Serial.write('A' + message.sequenceNumber);
    Serial.write(' ');
    Serial.write(message.payload, message.payloadSize);
    Serial.write(TERMINATOR);
}

void SerialProtocol::sendAcknowledge(uint8_t sequenceNumber) {
    Serial.write(BEGIN_CONTROL_MESSAGE);
    Serial.write(CONTROL_ACKNOWLEDGE);
    Serial.write('A' + sequenceNumber);
    Serial.write(TERMINATOR);
}

void SerialProtocol::sendError(SerialErrorCode errorCode) {
    Serial.write(BEGIN_CONTROL_MESSAGE);
    Serial.write(CONTROL_ERROR);
    Serial.write(static_cast<char>(errorCode));
    Serial.write(TERMINATOR);
}

void SerialProtocol::sendControlResponse(const char forControl, const char* fmt, ...) {
    char payload[PAYLOAD_MAX_SIZE];

    va_list args;
    va_start(args, fmt);
    size_t payloadSize = vsnprintf(payload, PAYLOAD_MAX_SIZE, fmt, args);
    va_end(args);

    Serial.write(BEGIN_CONTROL_RESPONSE_MESSAGE);
    Serial.write(forControl);
    Serial.write(payload, payloadSize);
    Serial.write(TERMINATOR);
}

const SerialProtocol::QueuedMessage& SerialProtocol::currentUnacknowledgedMessage() const {
    return _txQueue[_currentUnacknowledgedTxQueueIndex];
}

SerialProtocol::QueuedMessage& SerialProtocol::currentUnacknowledgedMessage() {
    return _txQueue[_currentUnacknowledgedTxQueueIndex];
}

void SerialProtocol::acknowledgeCurrentUnacknowledgedMessage() {
    currentUnacknowledgedMessage().acknowledged = true;
    incrementCurrentUnacknowledgedTxQueueIndex();
}

bool SerialProtocol::txMessageQueued() const {
    return !currentUnacknowledgedMessage().acknowledged;
}

size_t SerialProtocol::nextTxQueueIndex() const {
    return (_lastTxQueueIndex + 1u) % TX_QUEUE_SIZE;
}

size_t SerialProtocol::incrementLastTxQueueIndex() {
    _lastTxQueueIndex = nextTxQueueIndex();

    // Move oldest index along if already acknowledged (e.g. as it is the case in the beginning).
    while (!txMessageQueued() && _currentUnacknowledgedTxQueueIndex != _lastTxQueueIndex) {
        acknowledgeCurrentUnacknowledgedMessage();
    }
    return _lastTxQueueIndex;
}

size_t SerialProtocol::incrementCurrentUnacknowledgedTxQueueIndex() {
    if (_currentUnacknowledgedTxQueueIndex == _lastTxQueueIndex) { // do not increment beyond last message
        return _currentUnacknowledgedTxQueueIndex;
    } else {
        return _currentUnacknowledgedTxQueueIndex = (_currentUnacknowledgedTxQueueIndex + 1u) % TX_QUEUE_SIZE;
    }
}

uint8_t SerialProtocol::incrementTxSequenceNumber() {
    return _currentTxSequenceNumber = wrapSequenceNumber(_currentTxSequenceNumber + 1u);
}

uint8_t SerialProtocol::incrementRxSequenceNumber() {
    return _currentRxSequenceNumber = wrapSequenceNumber(_currentRxSequenceNumber + 1u);
}

uint8_t SerialProtocol::wrapSequenceNumber(uint8_t sequenceNumber) {
    return (sequenceNumber > MAX_SEQUENCE_NUMBER) ? 0u : sequenceNumber;
}

void SerialProtocol::receive() {
    auto bytesRead = Serial.readBytes(_rxBuffer + _rxBufferSize, BUFFER_MAX_SIZE - _rxBufferSize);
    auto newRxBufferSize = _rxBufferSize + bytesRead;

    char* newLine = reinterpret_cast<char*>(memchr(_rxBuffer, '\n', newRxBufferSize));
    if (newLine != nullptr) {
        if ((newLine != _rxBuffer) && *(newLine - 1) == '\r') {
            *(newLine - 1) = '\0';

            handleRxMessage(_rxBuffer);
            
            _rxBufferSize = (_rxBuffer + newRxBufferSize) - (newLine + 1);
            memmove(_rxBuffer, newLine + 1, _rxBufferSize);
        } else {
            sendError(SerialErrorCode::InvalidMessageTermination);
            _rxBufferSize = 0u;
        }
    } else if (newRxBufferSize >= BUFFER_MAX_SIZE) {
        sendError(SerialErrorCode::InvalidMessageTermination);
        _rxBufferSize = 0u;
    } else {
        _rxBufferSize = newRxBufferSize;
    }
}

void SerialProtocol::handleRxMessage(const char* rxMessage) {
    size_t rxMessageLength = strlen(rxMessage);
    switch (rxMessage[0]) {
        case BEGIN_NORMAL_MESSAGE:
            handleNormalMessage(rxMessage, rxMessageLength);
            break;
        case BEGIN_CONTROL_MESSAGE:
            handleControlMessage(rxMessage, rxMessageLength);
            break;
        case BEGIN_CONTROL_RESPONSE_MESSAGE:
            // we don't handle it, as it is used for interactive testing/debugging only for now.
            break;
        default:
            sendError(SerialErrorCode::InvalidMessageStart);
            break;
    }
}

void SerialProtocol::handleNormalMessage(const char* rxMessage, size_t rxMessageLength) {
    if (rxMessageLength >= BEGIN_SIZE + SEQUENCE_COUNTER_SIZE) {
        if (rxMessage[2] != ' ') {
            sendError(SerialErrorCode::InvalidSequenceCounter);
        }
        uint8_t sequenceNumber = uint8_t(rxMessage[1] - 'A');
        if (sequenceNumber == _currentRxSequenceNumber) {
            sendAcknowledge(incrementRxSequenceNumber());
            _processReceived(rxMessage + BEGIN_SIZE + SEQUENCE_COUNTER_SIZE, *this);
        } else {
            sendAcknowledge(_currentRxSequenceNumber); // Send last ack again to trigger a re-send
        }
    } else {
        sendError(SerialErrorCode::InvalidMessageSize);
    }
}

void SerialProtocol::handleControlMessage(const char* rxMessage, size_t rxMessageLength) {
    if (rxMessageLength == CONTROL_MESSAGE_SIZE) {
        switch (rxMessage[1]) {
            case CONTROL_ACKNOWLEDGE:
                handleAcknowledge(uint8_t(rxMessage[2] - 'A'));
                break;
            case CONTROL_ERROR:
                _handleError(static_cast<SerialErrorCode>(rxMessage[2]), *this);
                break;
            case CONTROL_TIMEOUT:
                handleTimeoutControl(rxMessage[2]);
                break;
            case CONTROL_DEBUG:
                sendControlResponse(CONTROL_DEBUG, "%u %u %u %u %u %u", _currentTxSequenceNumber, _currentRxSequenceNumber, _lastTxQueueIndex, _currentUnacknowledgedTxQueueIndex, _txQueue[_currentUnacknowledgedTxQueueIndex].acknowledged, _txQueue[_currentUnacknowledgedTxQueueIndex].sendTime);
                break;
            default:
                sendError(SerialErrorCode::InvalidControlMessage);
                break;
        }
    } else {
        sendError(SerialErrorCode::InvalidMessageSize);
    }
}

void SerialProtocol::handleAcknowledge(uint8_t sequenceNumber) {
    QueuedMessage& currentMessage = currentUnacknowledgedMessage();
    if (!currentMessage.acknowledged && wrapSequenceNumber(currentMessage.sequenceNumber + 1u) == sequenceNumber) {
        acknowledgeCurrentUnacknowledgedMessage();
        QueuedMessage& nextMessage = currentUnacknowledgedMessage();
        if (!nextMessage.acknowledged && canSend(_currentUnacknowledgedTxQueueIndex)) {
            send(nextMessage);
        }
    } else {
        // ignore
    }
}

void SerialProtocol::handleTimeoutControl(const char operation) {
    switch (operation) {
        case '+':
            _timeoutEnabled = true;
            sendControlResponse(CONTROL_TIMEOUT, "%c", operation);
            break;
        case '-':
            _timeoutEnabled = false;
            sendControlResponse(CONTROL_TIMEOUT, "%c", operation);
            break;
        default:
            sendError(SerialErrorCode::InvalidTimeoutControl);
            break;
    }
}