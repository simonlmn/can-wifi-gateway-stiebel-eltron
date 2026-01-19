#ifndef STIEBELELTRONPROTOCOL_H_
#define STIEBELELTRONPROTOCOL_H_

#include <iot_core/Interfaces.h>
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

bool hasShortValueId(const uint8_t (&data)[8]) {
  return data[2] < VALUE_ID_EXTENDED;
}

void setValueId(ValueId valueId, uint8_t (&data)[8]) {
  data[2] = VALUE_ID_EXTENDED; // indicate 2-byte value ID; this should be understood even if the ID could fit in one byte.
  data[3] = (valueId >> 8) & 0xFFu;
  data[4] = valueId & 0xFFu;
}

ValueId getValueId(const uint8_t (&data)[8]) {
  if (hasShortValueId(data)) {
    return data[2];
  } else {
    return (data[3] << 8) | data[4];
  }
}

size_t getValueIndex(const uint8_t (&data)[8]) {
  return hasShortValueId(data) ? 3 : 5;
}

void setValue(uint16_t value, uint8_t (&data)[8]) {
  size_t valueIndex = getValueIndex(data);
  data[valueIndex + 0] = (value >> 8) & 0xFFu;
  data[valueIndex + 1] = value & 0xFFu; 
}

uint16_t getValue(const uint8_t (&data)[8]) {
  size_t valueIndex = getValueIndex(data);
  return (data[valueIndex + 0] << 8)
        | data[valueIndex + 1];
}

class IStiebelEltronDevice {
public:
  virtual const char* name() const = 0;
  virtual const char* description() const = 0;
  virtual const DeviceId& deviceId() const = 0;
  virtual void request(const RequestData& data) = 0;
  virtual void write(const WriteData& data) = 0;
  virtual void receive(const ResponseData& data) = 0;
};

class StiebelEltronProtocol final : public iot_core::IApplicationComponent {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  ICanInterface& _can;
  bool _ready = false;

  iot_core::ConstStrMap<IStiebelEltronDevice*> _devices {};

  std::set<DeviceId> _otherDeviceIds {};

  std::function<void(ResponseData const& data)> _responseListener {};
  std::function<void(WriteData const& data)> _writeListener {};
  std::function<void(RequestData const& data)> _requestListener {};
  
public:
  StiebelEltronProtocol(iot_core::ISystem& system, ICanInterface& can)
    : _logger(system.logger("sep")),
    _system(system),
    _can(can)
  { }

  const char* name() const override {
    return "sep";
  }

  bool configure(const char* name, const char* value) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
  }

  void setup(bool /*connected*/) override {
    _can.onReady([this] () {
      for (auto& [name, device] : _devices) {
        registerDevice(*device);
      }
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

  void addDevice(IStiebelEltronDevice* device) {
    _devices[device->name()] = device;
  }

  void removeDevice(IStiebelEltronDevice* device) {
    _devices.erase(device->name());
  }

  const iot_core::ConstStrMap<IStiebelEltronDevice*>& getDevices() const {
    return _devices;
  }

  const std::set<DeviceId>& getOtherDeviceIds() const {
    return _otherDeviceIds;
  }

  bool request(RequestData const& data) {
    if (!_ready) {
      return false;
    }

    if (!data.targetId.isExact() || !data.sourceId.isExact()) {
      _logger.log(iot_core::LogLevel::Error, toolbox::format(F("Request source and target must have exact IDs (but are '%s' and '%s')"), data.sourceId.toString(0), data.targetId.toString(1)));
      return false;
    }

    _system.lyield();

    CanMessage message;

    message.id = toCanId(data.sourceId);
    message.ext = false;
    message.rtr = false;
    message.len = 7;
    setTargetId(data.targetId, message.data);
    setMessageType(MessageType::Request, message.data);
    setValueId(data.valueId, message.data);
    message.data[5] = 0x00u;
    message.data[6] = 0x00u;

    return _can.sendCanMessage(message);
  }

  bool write(WriteData const& data) {
    if (!_ready) {
      return false;
    }

    if (!data.targetId.isExact() || !data.sourceId.isExact()) {
      _logger.log(iot_core::LogLevel::Error, toolbox::format(F("Write source and target must have exact IDs (but are '%s' and '%s')"), data.sourceId.toString(0), data.targetId.toString(1)));
      return false;
    }

    _system.lyield();
    
    CanMessage message;
    
    message.id = toCanId(data.sourceId);
    message.ext = false;
    message.rtr = false;
    message.len = 7;
    setTargetId(data.targetId, message.data);
    setMessageType(MessageType::Write, message.data);
    setValueId(data.valueId, message.data);
    setValue(data.value, message.data);
    
    return _can.sendCanMessage(message);
  }

  bool respond(ResponseData const& data) {
    if (!_ready) {
      return false;
    }

    if (!data.targetId.isExact() || !data.sourceId.isExact()) {
      _logger.log(iot_core::LogLevel::Error, toolbox::format(F("Response source and target must have exact IDs (but are '%s' and '%s')"), data.sourceId.toString(0), data.targetId.toString(1)));
      return false;
    }

    _system.lyield();
    
    CanMessage message;
    
    message.id = toCanId(data.sourceId);
    message.ext = false;
    message.rtr = false;
    message.len = 7;
    setTargetId(data.targetId, message.data);
    setMessageType(MessageType::Response, message.data);
    setValueId(data.valueId, message.data);
    setValue(data.value, message.data);
    
    return _can.sendCanMessage(message);
  }

  void onResponse(std::function<void(ResponseData const& data)> responseHandler) {
    if (_responseListener) {
      auto previousHandler = _responseListener;
      _responseListener = [=](ResponseData const& data) { previousHandler(data); responseHandler(data); };
    } else {
      _responseListener = responseHandler;
    }
  }

  void onWrite(std::function<void(WriteData const& data)> writeHandler) {
    if (_writeListener) {
      auto previousHandler = _writeListener;
      _writeListener = [=](WriteData const& data) { previousHandler(data); writeHandler(data); };
    } else {
      _writeListener = writeHandler;
    }
  }

  void onRequest(std::function<void(RequestData const& data)> requestHandler) {
    if (_requestListener) {
      auto previousHandler = _requestListener;
      _requestListener = [=](RequestData const& data) { previousHandler(data); requestHandler(data); };
    } else {
      _requestListener = requestHandler;
    }
  }

private:
  void registerDevice(const IStiebelEltronDevice& device) {
    auto id = device.deviceId();
    if (!id.isExact()) {
      return;
    }

    // TODO is that really needed? The official display on the LWZ 5S+ does not seem to do this...
    //      there are however screenshots of other models where a list of CAN bus nodes is shown.
    // TODO maybe check/listen first if the ID is alread in use?
    CanMessage message;

    message.id = toCanId(id);
    message.ext = false;
    message.rtr = false;
    message.len = 7u;
    setTargetId(id, message.data);
    setMessageType(MessageType::Register, message.data);
    message.data[2] = VALUE_ID_BUS_CONFIGURATION;
    message.data[3] = 0x09u; // TODO unclear what these values mean (they seem to be constant)
    message.data[4] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    message.data[5] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    message.data[6] = 0x00u; // TODO unclear what these values mean (they seem to be constant)
    
    _can.sendCanMessage(message);
  }

  void processProtocol(CanMessage const& frame) {
    if (frame.ext || frame.rtr) {
      return;
    }

    if (frame.len == 7) { // TODO Other implementations/variations of the "Elster" protocol also have 5 bytes only for requests
      _system.lyield();

      MessageType type = getMessageType(frame.data);
      DeviceId target = getTargetId(frame.data);
      DeviceId source = fromCanId(frame.id);
      ValueId valueId = getValueId(frame.data);
      uint16_t value = getValue(frame.data);

      if (target.type == DeviceType::Display && target.address == BROADCAST_DISPLAY_ADDRESS) {
        target.address = DEVICE_ADDR_ANY;
      }

      logMessage(frame, type, target, source, valueId, value);

      bool handled = false;

      switch (type)
      {
      case MessageType::Write:
        {
          WriteData data {source, target, valueId, value};
          handled = forwardMessage(target, data, &IStiebelEltronDevice::write);
          if (_writeListener) {
            _writeListener(data);
          }
        }
        break;
      case MessageType::Response:
        {
          ResponseData data {source, target, valueId, value};
          handled = forwardMessage(target, data, &IStiebelEltronDevice::receive);
          if (_responseListener) {
            _responseListener({ source, target, valueId, value });
          }
        }
        break;
      case MessageType::Request:
        {
          RequestData data {source, target, valueId};
          handled = forwardMessage(target, data, &IStiebelEltronDevice::request);
          if (_requestListener) {
            _requestListener({ source, target, valueId });
          }
        }
        break;
      }

      if (source.isExact()) {
        _otherDeviceIds.insert(source);
      }

      if (!handled && target.isExact()) {
        _otherDeviceIds.insert(target);
      }
    }
  }

  void logMessage(CanMessage const& frame, MessageType type, DeviceId target, DeviceId source, ValueId valueId, uint16_t value) {
    switch (type)
    {
    case MessageType::Register:
      _logger.log(iot_core::LogLevel::Debug, [&] () { return toolbox::format(F("%s t:%s s:%s %02X %02X %02X %02X %02X"), messageTypeToString(type), target.toString(0), source.toString(1), frame.data[2], frame.data[3], frame.data[4], frame.data[5], frame.data[6]); });
      break;
    case MessageType::Write:
      _logger.log(iot_core::LogLevel::Debug, [&] () { return toolbox::format(F("%s t:%s s:%s id:%04X%c v:%04X"), messageTypeToString(type), target.toString(0), source.toString(1), valueId, hasShortValueId(frame.data) ? 's' : 'l', value); });
      break;
    case MessageType::Response:
      _logger.log(iot_core::LogLevel::Debug, [&] () { return toolbox::format(F("%s t:%s s:%s id:%04X%c v:%04X"), messageTypeToString(type), target.toString(0), source.toString(1), valueId, hasShortValueId(frame.data) ? 's' : 'l', value); });
      break;
    case MessageType::Request:
      _logger.log(iot_core::LogLevel::Debug, [&] () { return toolbox::format(F("%s t:%s s:%s id:%04X%c"), messageTypeToString(type), target.toString(0), source.toString(1), valueId, hasShortValueId(frame.data) ? 's' : 'l'); });
      break;
    default:
      _logger.log(iot_core::LogLevel::Info, [&] () { return toolbox::format(F("%s t:%s s:%s %02X %02X %02X %02X %02X"), messageTypeToString(type), target.toString(0), source.toString(1), frame.data[2], frame.data[3], frame.data[4], frame.data[5], frame.data[6]); });
      break;
    }
  }

  template<typename Message>
  bool forwardMessage(DeviceId target, const Message& message, void (IStiebelEltronDevice::*handlerFunction)(const Message&)) {
    bool forwarded = false;
    for (auto& [name, device] : _devices) {
      if (target.includes(device->deviceId())) {
        std::invoke(handlerFunction, device, message);
        forwarded = true;
      }
    }
    return forwarded;
  }
};

#endif
