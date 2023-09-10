#ifndef STIEBELELTRONPROTOCOL_H_
#define STIEBELELTRONPROTOCOL_H_

#ifdef TEST_ENV
#include <cstdint>
#include <cstdio>
#endif

#include "src/iot-core/Interfaces.h"
#include "StiebelEltronTypes.h"
#include "CanInterface.h"
#include <functional>
#include <set>

/*
 * NOTE: the protocol is actually based on the Elster-Kromschröder protocol,
 * but this here also contains the specifics for the Stiebel-Eltron systems.
 */

/* NOTE: each communication participant has an address consisting of two parts:
 *  - the type (e.g. system, heating circuit, sensor or display),
 *  - the address for differentiating multiple instances (may be 0 if there is only one, in other cases starts at 1).
 * 
 * As these have to fit into an 11-bit CAN ID, they are not simply two 8-bit values,
 * but instead are allocated 4 and 7 bits.
 * 
 * As these are also encoded into the data bytes to indicate the target of a message,
 * the bit and byte positions are also specified here.
 */
const uint8_t DEVICE_TYPE_MASK = 0x0Fu;
const uint8_t DEVICE_TYPE_CAN_ID_BIT_POS = 7u;
const uint8_t DEVICE_TYPE_DATA_BYTE_POS = 0u;
const uint8_t DEVICE_TYPE_DATA_BIT_POS = 4u;

const uint8_t DEVICE_ADDR_MASK = 0x7Fu;
const uint8_t DEVICE_ADDR_CAN_ID_BIT_POS = 0u;
const uint8_t DEVICE_ADDR_DATA_BYTE_POS = 1u;
const uint8_t DEVICE_ADDR_DATA_BIT_POS = 0u;

const uint8_t MSG_TYPE_MASK = 0x0Fu;
const uint8_t MSG_TYPE_DATA_BYTE_POS = 0u;
const uint8_t MSG_TYPE_DATA_BIT_POS = 0u;

enum struct MessageType : uint8_t {
  Write = 0x00u,
  Request = 0x01u,
  Response = 0x02u,
  Register = 0x06u
};

const char* messageTypeToString(MessageType type) {
  switch (type) {
    case MessageType::Write:
      return "WRT";
    case MessageType::Request:
      return "REQ";
    case MessageType::Response:
      return "RES";
    case MessageType::Register:
      return "REG";
    default:
      static char buffer[4];
      snprintf(buffer, 4, "X%02X", static_cast<uint8_t>(type));
      return buffer;
  }
}

uint32_t toCanId(DeviceId const& id) {
  return (static_cast<uint8_t>(id.type) << DEVICE_TYPE_CAN_ID_BIT_POS) | ((id.address & DEVICE_ADDR_MASK) << DEVICE_ADDR_CAN_ID_BIT_POS);
}

DeviceId fromCanId(uint32_t canId) {
  uint8_t type = (canId >> DEVICE_TYPE_CAN_ID_BIT_POS) & DEVICE_TYPE_MASK;
  uint8_t address = (canId >> DEVICE_ADDR_CAN_ID_BIT_POS) & DEVICE_ADDR_MASK;
  return {static_cast<DeviceType>(type), address};
}

void setTargetId(DeviceId const& id, uint8_t (&data)[8]) {
  data[DEVICE_TYPE_DATA_BYTE_POS] = ((static_cast<uint8_t>(id.type) & DEVICE_TYPE_MASK) << DEVICE_TYPE_DATA_BIT_POS) | (data[DEVICE_TYPE_DATA_BYTE_POS] & ~(DEVICE_TYPE_MASK << DEVICE_TYPE_DATA_BIT_POS));
  data[DEVICE_ADDR_DATA_BYTE_POS] = ((id.address & DEVICE_ADDR_MASK) << DEVICE_ADDR_DATA_BIT_POS) | (data[DEVICE_ADDR_DATA_BYTE_POS] & ~(DEVICE_ADDR_MASK << DEVICE_ADDR_DATA_BIT_POS));
}

void setMessageType(MessageType const& type, uint8_t (&data)[8]) {
  data[MSG_TYPE_DATA_BYTE_POS] = ((static_cast<uint8_t>(type) & MSG_TYPE_MASK) << MSG_TYPE_DATA_BIT_POS) | (data[MSG_TYPE_DATA_BYTE_POS] & ~(MSG_TYPE_MASK << MSG_TYPE_DATA_BIT_POS));
}

DeviceId getTargetId(const uint8_t (&data)[8]) {
  uint8_t type = (data[DEVICE_TYPE_DATA_BYTE_POS] >> DEVICE_TYPE_DATA_BIT_POS) & DEVICE_TYPE_MASK;
  uint8_t address = (data[DEVICE_ADDR_DATA_BYTE_POS] >> DEVICE_ADDR_DATA_BIT_POS) & DEVICE_ADDR_MASK;
  return {static_cast<DeviceType>(type), address};
}

MessageType getMessageType(const uint8_t (&data)[8]) {
  uint8_t type = (data[MSG_TYPE_DATA_BYTE_POS] >> MSG_TYPE_DATA_BIT_POS) & MSG_TYPE_MASK;
  return static_cast<MessageType>(type);
}

void setValueId(ValueId valueId, uint8_t (&data)[8]) {
  data[3] = (valueId >> 8) & 0xFFu;
  data[4] = valueId & 0xFFu;
}

ValueId getValueId(const uint8_t (&data)[8]) {
  return (data[3] << 8) | data[4];
}

void setValue(uint16_t value, uint8_t (&data)[8]) {
  data[5] = (value >> 8) & 0xFFu;
  data[6] = value & 0xFFu;
}

uint16_t getValue(const uint8_t (&data)[8]) {
  return (data[5] << 8) | data[6];
}

// Constants for known IDs and addresses

const DeviceId SYSTEM_ID = {DeviceType::System, 0u};
const DeviceId HEATING_CIRCUIT_1_ID = {DeviceType::HeatingCircuit, 1u};
const DeviceId HEATING_CIRCUIT_2_ID = {DeviceType::HeatingCircuit, 2u};

/* 
 * Available "display" addresses, which can be used by devices on the bus to interact with the system.
 * The built-in control panel/display uses one and, for example, the ISG too.
 * 
 * The built-in display uses number 4 (index = 3) by default.
 * 
 * In order to use a "display" for room temperature/humidity monitoring, it must use numer 1 or 2 (= index 0 or 1).
 */
const uint8_t DISPLAY_ADDRESSES[] = { 0x1Eu, 0x1Fu, 0x20u, 0x21u, 0x22u };

/*
 * This ID gets addressed (as a sub-target) by the system with the current date/time information and operating mode and status.
 * 
 * It is unclear why this is different from the "normal" IDs. Maybe this is some kind of "broadcast" ID, as
 * all FES units will display this information on the main screen.
 */
const uint8_t BROADCAST_DISPLAY_ADDRESS = 0x3Cu;

class StiebelEltronProtocol final : public iot_core::IApplicationComponent {
private:
  iot_core::Logger& _logger;
  iot_core::ISystem& _system;
  ICanInterface& _can;
  uint8_t _displayIndex;
  DeviceId _deviceId;
  uint32_t _canId;
  bool _ready;

  std::set<DeviceId> _otherDeviceIds;

  std::function<void(ResponseData const& data)> _responseHandler;
  std::function<void(WriteData const& data)> _writeHandler;
  
public:
  StiebelEltronProtocol(iot_core::ISystem& system, ICanInterface& can)
    : _logger(system.logger()),
    _system(system),
    _can(can),
    _displayIndex(),
    _deviceId(),
    _canId(0u),
    _ready(false),
    _otherDeviceIds(),
    _responseHandler(),
    _writeHandler()
  { }

  const char* name() const override {
    return "sep";
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "display") == 0) return setDisplayIndex(iot_core::convert<uint8_t>::fromString(value, nullptr, 10));
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("display", iot_core::convert<long>::toString(_displayIndex, 10));
  }

  bool setDisplayIndex(uint8_t displayIndex) {
    if (displayIndex >= std::size(DISPLAY_ADDRESSES)) {
      return false;
    }

    _displayIndex = displayIndex;
    _deviceId = {DeviceType::Display, DISPLAY_ADDRESSES[displayIndex]};  
    _canId = toCanId(_deviceId);

    _logger.log(name(), iot_core::format(F("Set display index '%u' (deviceId=%s, canId=%lX)"), _displayIndex, _deviceId.toString(), _canId));

    if (_ready) {
      registerDisplay();
      registerSensor();
    }

    return true;
  }
  
  void setup(bool /*connected*/) override {
    _can.onReady([this] () {
      registerDisplay();
      registerSensor();
      _ready = true;
    });

    _can.onMessage([this] (CanMessage const& message) {
      processProtocol(message);
    });
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
  }

  void getDiagnostics(iot_core::IDiagnosticsCollector& /*collector*/) const override {
  }

  bool ready() const {
    return _ready;
  }

  const DeviceId& getThisDeviceId() const {
    return _deviceId;
  }

  const std::set<DeviceId>& getOtherDeviceIds() const {
    return _otherDeviceIds;
  }

  void request(RequestData const& data) {
    if (!_ready || _canId == 0u) {
      return;
    }

    if (!data.targetId.isExact()) {
      return;
    }

    _system.lyield();

    CanMessage message;
    if (data.sourceId.isExact()) {
      message.id = toCanId(data.sourceId);
    } else {
      message.id = _canId;
    }
    message.ext = false;
    message.rtr = false;
    message.len = 7;
    setTargetId(data.targetId, message.data);
    setMessageType(MessageType::Request, message.data);
    message.data[2] = 0xFAu;
    setValueId(data.valueId, message.data);
    message.data[5] = 0x00u;
    message.data[6] = 0x00u;

    _can.sendCanMessage(message);
  }

  void write(WriteData const& data) {
    if (!_ready || _canId == 0u) {
      return;
    }

    if (!data.targetId.isExact()) {
      return;
    }

    _system.lyield();
    
    CanMessage message;
    if (data.sourceId.isExact()) {
      message.id = toCanId(data.sourceId);
    } else {
      message.id = _canId;
    }
    message.ext = false;
    message.rtr = false;
    message.len = 7;
    setTargetId(data.targetId, message.data);
    setMessageType(MessageType::Write, message.data);
    message.data[2] = 0xFAu;
    setValueId(data.valueId, message.data);
    setValue(data.value, message.data);
    
    _can.sendCanMessage(message);
  }

  void onResponse(std::function<void(ResponseData const& data)> responseHandler) {
    if (_responseHandler) {
      auto previousHandler = _responseHandler;
      _responseHandler = [=](ResponseData const& data) { previousHandler(data); responseHandler(data); };
    } else {
      _responseHandler = responseHandler;
    }
  }

  void onWrite(std::function<void(WriteData const& data)> writeHandler) {
    if (_writeHandler) {
      auto previousHandler = _writeHandler;
      _writeHandler = [=](WriteData const& data) { previousHandler(data); writeHandler(data); };
    } else {
      _writeHandler = writeHandler;
    }
  }

private:
  void registerDisplay() {
    if (!_ready || _canId == 0u) {
      return;
    }

    // TODO is that really needed? The official display on the LWZ 5S+ does not seem to do this...
    //      there are however screenshots of other models where a list of CAN bus nodes is shown.
    // TODO maybe check/listen first if the ID is alread in use?
    CanMessage message;
    message.id = _canId;
    message.ext = false;
    message.rtr = false;
    message.len = 7u;
    setTargetId(_deviceId, message.data);
    setMessageType(MessageType::Register, message.data);
    message.data[2] = 0xFDu; // TODO unclear what these values mean (they seem to be constant)
    message.data[3] = 0x09u; // TODO unclear what these values mean (they seem to be constant)
    message.data[4] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    message.data[5] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    message.data[6] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    
    _can.sendCanMessage(message);
  }

  // TODO the official display also registers as a sensor (?) but it is unclear how that is used
  //      assumption: when using it as a room temperature/humidity sensor, it sends the data from that ID?
  void registerSensor() {
    if (!_ready || _canId == 0u) {
      return;
    }

    DeviceId sensorId = {DeviceType::Sensor, _displayIndex + 1};
    uint32_t sensorCanId = toCanId(sensorId);

    CanMessage message;
    message.id = sensorCanId;
    message.ext = false;
    message.rtr = false;
    message.len = 7u;
    setTargetId(sensorId, message.data);
    setMessageType(MessageType::Register, message.data);
    message.data[2] = 0xFDu; // TODO unclear what these values mean (they seem to be constant)
    message.data[3] = 0x09u; // TODO unclear what these values mean (they seem to be constant)
    message.data[4] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    message.data[5] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    message.data[6] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    
    _can.sendCanMessage(message);
  }

  void processProtocol(CanMessage const& frame) {
    if (!frame.ext && !frame.rtr && frame.len == 7) {
      _system.lyield();

      MessageType type = getMessageType(frame.data);
      DeviceId target = getTargetId(frame.data);
      DeviceId source = fromCanId(frame.id);
      
      if (target.type == DeviceType::Display && target.address == BROADCAST_DISPLAY_ADDRESS) {
        target.address = DEVICE_ADDR_ANY;
      }

      if (target.isExact() && target != _deviceId) {
        _otherDeviceIds.insert(target);
      }
      if (source.isExact() && source != _deviceId) {
        _otherDeviceIds.insert(source);
      }

      if (type == MessageType::Register) {
        _logger.log(iot_core::LogLevel::Debug, name(), [&] () { return iot_core::format(F("%s t:%s s:%s %02X %02X %02X %02X %02X"), messageTypeToString(type), target.toString(0), source.toString(1), frame.data[2], frame.data[3], frame.data[4], frame.data[5], frame.data[6]); });
      }

      if (type == MessageType::Write || type == MessageType::Response || type == MessageType::Request) {
        uint8_t fix = frame.data[2]; // this should always be 0xFAu
        ValueId valueId = getValueId(frame.data);
        uint16_t value = getValue(frame.data);

        _logger.log(iot_core::LogLevel::Debug, name(), [&] () { return iot_core::format(F("%c%s t:%s s:%s %02X id:%04X v:%04X"), target.includes(_deviceId) ? '>' : '*', messageTypeToString(type), target.toString(0), source.toString(1), fix, valueId, value); });
        
        switch (type) {
          case MessageType::Response:
            if (_responseHandler) {
              _responseHandler({ source, target, valueId, value });
            }
            break;
          case MessageType::Write:
            if (_writeHandler) {
              _writeHandler({ source, target, valueId, value });
            }
            break;
          default:
            // do nothing
            break;
        }
      }
    }
  }
};

#endif
