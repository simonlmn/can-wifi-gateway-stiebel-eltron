#ifndef SERIAL_PROTOCOL_ENDPOINT_H_
#define SERIAL_PROTOCOL_ENDPOINT_H_

#include <Arduino.h>

namespace serial_protocol {

class Endpoint;

enum struct ErrorCode : char {
    InvalidMessageTermination = 'E',
    InvalidMessageStart = 'H',
    InvalidMessageSize = 'S',
    InvalidControlMessage = 'C',
    InvalidTimeoutControl = 'T',
    InvalidSequenceCounter = 'Q',
    ResendLimitExceeded = 'R'
};

const char* describe(ErrorCode errorCode);

#if defined(ARDUINO_AVR_NANO)
using ReceiveCallback = void (*)(const char* rxMessage, Endpoint& serial);
using ErrorCallback = void (*)(ErrorCode errorCode, Endpoint& serial);
#else
#include <functional>
using ReceiveCallback = std::function<void(const char* rxMessage, Endpoint& serial)>;
using ErrorCallback = std::function<void(ErrorCode errorCode, Endpoint& serial)>;
#endif

/**
 * ASCII based protocol to communicate reliably over a serial connection without hardware flow control.
 * 
 * Control messages:
 * #=[A-Z]
 * #![A-Z]
 * #T-
 * #T+
 * #D 
 * 
 * Normal messages:
 * +[A-Z] _____________________________________________________
 * 
 * CAN module specific messages:
 * +[A-Z] READY\r\n
 * +[A-Z] SETUP FFFFFFFF NOR|LOP|SLP|LIS\r\n
 * +[A-Z] SETUP OK 9 64 8 8 8 4 1 1234567890 1 1234567890\r\n
 * +[A-Z] SETUP EFFFF: FFFFFFFF NOR|LOP|SLP|LIS\r\n
 * +[A-Z] CANRX FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
 * +[A-Z] CANTX FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
 * +[A-Z] CANTX OK FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
 * +[A-Z] CANTX ENVAL FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
 * +[A-Z] CANTX ESEND FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
 * +[A-Z] CANTX ENOAV FFFFFFFF 8 01 02 03 04 05 06 07 08 \r\n
 * +[A-Z] ERROR _______________________________________________\r\n
 */
class Endpoint final {
private:
    static const unsigned long DEFAULT_BAUD_RATE = 115200u;

    static const unsigned long DEFAULT_TIMEOUT = 2000u;
    static const uint8_t DEFAULT_RESEND_LIMIT = 4u;

    static const size_t BUFFER_MAX_SIZE = 64u;
    static const size_t BEGIN_SIZE = 1u;
    static const size_t TERMINATOR_SIZE = 2u;
    static const size_t SEQUENCE_COUNTER_SIZE = 2u;
    static const size_t PAYLOAD_MAX_SIZE = BUFFER_MAX_SIZE - BEGIN_SIZE - TERMINATOR_SIZE - SEQUENCE_COUNTER_SIZE;
    static const size_t CONTROL_MESSAGE_SIZE = BEGIN_SIZE + 2u;

    struct QueuedMessage {
        char payload[PAYLOAD_MAX_SIZE] = {};
        size_t payloadSize = 0u;
        uint8_t sequenceNumber = 0u;
        unsigned long sendTime = 0u;
        uint8_t retries = 0u;
        bool acknowledged = true;
    };

    static const size_t TX_QUEUE_SIZE = 6u;
    QueuedMessage _txQueue[TX_QUEUE_SIZE] = {};
    size_t _lastTxQueueIndex = 0u;
    size_t _currentUnacknowledgedTxQueueIndex = 0u; // can point to an acknowledged message, which means no messages are unacknowledged

    char _rxBuffer[BUFFER_MAX_SIZE] = {};
    size_t _rxBufferSize = 0u;

    static const uint8_t MAX_SEQUENCE_NUMBER = 25u;

    uint8_t _currentTxSequenceNumber = MAX_SEQUENCE_NUMBER;
    uint8_t _currentRxSequenceNumber = 0u;

    bool _timeoutEnabled = true;
    unsigned long _timeout = DEFAULT_TIMEOUT;
    uint8_t _resendLimit = DEFAULT_RESEND_LIMIT;

    ReceiveCallback _processReceived;
    ErrorCallback _handleError;

    void send(QueuedMessage& message);
    void sendAcknowledge(uint8_t sequenceNumber);
    void sendError(ErrorCode errorCode);
    void sendControlResponse(const char forControl, const char* fmt, ...);
    const QueuedMessage& currentUnacknowledgedMessage() const;
    QueuedMessage& currentUnacknowledgedMessage();
    void acknowledgeCurrentUnacknowledgedMessage();
    bool txMessageQueued() const;
    size_t nextTxQueueIndex() const;
    size_t incrementLastTxQueueIndex();
    size_t incrementCurrentUnacknowledgedTxQueueIndex();
    uint8_t incrementTxSequenceNumber();
    uint8_t incrementRxSequenceNumber();
    uint8_t wrapSequenceNumber(uint8_t sequenceNumber);
    void receive();
    void handleNormalMessage(const char* rxMessage, size_t rxMessageLength);
    void handleControlMessage(const char* rxMessage, size_t rxMessageLength);
    void handleRxMessage(const char* rxMessage);
    void handleAcknowledge(uint8_t sequenceNumber);
    void handleTimeoutControl(const char operation);

public:
    Endpoint(ReceiveCallback processReceived, ErrorCallback handleError);

    void setup(unsigned long baud = DEFAULT_BAUD_RATE);
    void reset();
    void loop();
    bool canQueue() const;
    bool canSend(size_t queueIndex) const;
    bool queue(const char* fmt, ...);
};

// Helper functions to send CAN messages from both sides

bool queueCanTxMessage(Endpoint& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]);

bool queueCanRxMessage(Endpoint& serial, uint32_t id, bool ext, bool rtr, uint8_t length, const uint8_t (&data)[8]);

}

#endif
