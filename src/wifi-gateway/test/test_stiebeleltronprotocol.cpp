
#include "../StiebelEltronProtocolImpl.h"
#include <vector>

struct NodeBaseMock {
  int logLevel(const char* category) { return 0; }
  void log(const char* category, const char* message) {}
};

struct CanMock {
  std::function<void()> _readyHandler;
  std::function<void(const CanMessage& message)> _messageHandler;
  std::vector<CanMessage> _sentMessages;

  void onReady(std::function<void()> readyHandler) {
    _readyHandler = readyHandler;
  }

  void onMessage(std::function<void(const CanMessage& message)> messageHandler) {
    _messageHandler = messageHandler;
  }

  void sendCanMessage(const CanMessage& message) {
    _sentMessages.push_back(message);
  }
};

int main() {
    using StiebelEltronProtocol = impl::StiebelEltronProtocol<NodeBaseMock, CanMock>;

    uint32_t expectedCanId = 0x69Fu; // (DeviceType::Display << 7) | DISPLAY_ADDRESSES[1u]

    NodeBaseMock system;
    CanMock can;
    StiebelEltronProtocol protocol {system, can, 1u};
    std::vector<ResponseData> responses;
    protocol.onResponse([&](ResponseData const& response){
        responses.push_back(response);
    });
    std::vector<WriteData> writes;
    protocol.onWrite([&](WriteData const& write){
        writes.push_back(write);
    });
    protocol.setup();

    can._readyHandler();
    assert(can._sentMessages.back().id == expectedCanId);
    assert(can._sentMessages.back().len == 7u);
    assert(can._sentMessages.back().data[0] == 0xD6u); // (DeviceType::Display << 4) | MsgType::Register
    assert(can._sentMessages.back().data[1] == 0x1Fu); // DISPLAY_ADDRESSES[1u]

    protocol.request({DeviceId{DeviceType::Sensor, 1u}, 0x1234u});
    assert(can._sentMessages.back().id == expectedCanId);
    assert(can._sentMessages.back().len == 7u);
    assert(can._sentMessages.back().data[0] == 0x81u); // (DeviceType::Sensor << 4) | MsgType::Request
    assert(can._sentMessages.back().data[1] == 0x01u); // 1u
    assert(can._sentMessages.back().data[3] == 0x12u); // 0x1234u >> 8 & 0xFF
    assert(can._sentMessages.back().data[4] == 0x34u); // 0x1234u      & 0xFF

    protocol.write({DeviceId{DeviceType::HeatingCircuit, 3u}, 0x1234u, 0x5678u});
    assert(can._sentMessages.back().id == expectedCanId);
    assert(can._sentMessages.back().len == 7u);
    assert(can._sentMessages.back().data[0] == 0x60u); // (DeviceType::HeatingCircuit << 4) | MsgType::Write
    assert(can._sentMessages.back().data[1] == 0x03u); // 1u
    assert(can._sentMessages.back().data[3] == 0x12u); // 0x1234u >> 8 & 0xFF
    assert(can._sentMessages.back().data[4] == 0x34u); // 0x1234u      & 0xFF
    assert(can._sentMessages.back().data[5] == 0x56u); // 0x5678u >> 8 & 0xFF
    assert(can._sentMessages.back().data[6] == 0x78u); // 0x5678u      & 0xFF

    // Request message from display number 1 to system
    can._messageHandler({0x69Eu, false, false, 7u, {0x31u, 0x00u, 0xFAu, 0x12u, 0x34u, 0x00u, 0x00u}});
    assert(responses.empty());
    assert(writes.empty());

    // Register message from display number 2
    can._messageHandler({0x69Eu, false, false, 7u, {0xD6u, 0x1Eu, 0xFDu, 0x01u, 0x00u, 0x00u, 0x00u}});
    assert(responses.empty());
    assert(writes.empty());

    // Response message from system to display number 2
    can._messageHandler({0x180u, false, false, 7u, {0xD2u, 0x1Fu, 0xFAu, 0x12u, 0x34u, 0x56u, 0x78u}});
    assert(responses.back().sourceId == DeviceId(DeviceType::System, 0u));
    assert(responses.back().valueId == 0x1234u);
    assert(responses.back().value == 0x5678u);

    // Write message from heating circuit 1 to system
    can._messageHandler({0x301u, false, false, 7u, {0x30u, 0x00u, 0xFAu, 0x23u, 0x45u, 0x67u, 0x89u}});
    assert(writes.back().targetId == DeviceId(DeviceType::System, 0u));
    assert(writes.back().valueId == 0x2345u);
    assert(writes.back().value == 0x6789u);

    return 0;
}