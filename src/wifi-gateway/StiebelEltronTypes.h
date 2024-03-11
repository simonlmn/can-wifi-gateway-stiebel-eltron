#ifndef STIEBELELTRONTYPES_H_
#define STIEBELELTRONTYPES_H_

#ifdef TEST_ENV
#include <cstdint>
#include <cstdio>
#endif

#include <iot_core/Utils.h>

// Note: the device type gets encoded in the CAN ID on the bus, which has an influence on priorization
// of messages. In CAN arbitration, lower numbers have priority and may prevent other devices from
// sending messages.
enum struct DeviceType : uint8_t {
  System = 0x03u,
  X04 = 0x04u,
  X05 = 0x05u,
  HeatingCircuit = 0x06u,
  X07 = 0x07u,
  Sensor = 0x08u,
  X09 = 0x09u,
  X0A = 0x0Au,
  X0B = 0x0Bu,
  X0C = 0x0Cu,
  Display = 0x0Du,
  X0E = 0x0Eu,
  X0F = 0x0Fu, // largest possible type
  Any = 0xFFu
};

const char* deviceTypeToString(DeviceType type) {
  switch (type) {
    case DeviceType::System:
      return "SYS";
    case DeviceType::HeatingCircuit:
      return "HEA";
    case DeviceType::Sensor:
      return "SEN";
    case DeviceType::Display:
      return "DIS";
    case DeviceType::Any:
      return "ANY";
    default:
      static char buffer[4];
      snprintf(buffer, 4, "X%02X", static_cast<uint8_t>(type));
      return buffer;
  }
}

DeviceType deviceTypeFromString(const toolbox::strref& type) {
  if (type == "SYS") return DeviceType::System;
  if (type == "HEA") return DeviceType::HeatingCircuit;
  if (type == "SEN") return DeviceType::Sensor;
  if (type == "DIS") return DeviceType::Display;
  if (type == "ANY") return DeviceType::Any;
  if (type.length() == 3 && (type.charAt(0) == 'X')) {
    return DeviceType(iot_core::convert<uint8_t>::fromString(type.skip(1).toString().c_str(), nullptr, 16));
  }
  
  return DeviceType::Any;
}

using DeviceAddress = uint8_t;

/* Special value to indicate on a DeviceId that it is for any address.
 * This cannot be sent with the protocol and is only meant to be used
 * for internal representation of "any address", e.g. for describing
 * datapoints or representing "broadcasts".
 */
const uint8_t DEVICE_ADDR_ANY = 0x80u;

struct DeviceId {
  DeviceType type;
  DeviceAddress address;

  /**
   * Note: address can be set to DEVICE_ADDR_ANY if no specific address is known at this point.
   */
  DeviceId(DeviceType type = DeviceType::Any, DeviceAddress address = DEVICE_ADDR_ANY) : type(type), address(address) {}

  bool isAny() const {
    return type == DeviceType::Any && address == DEVICE_ADDR_ANY;
  }

  bool isExact() const {
    return type != DeviceType::Any && address != DEVICE_ADDR_ANY;
  }

  bool includes(DeviceId const& other) const {
    return (type == DeviceType::Any || type == other.type) && (address == DEVICE_ADDR_ANY || address == other.address);
  }

  const char* toString(size_t bufferIndex = 0) const {
    constexpr size_t NUM_BUFFERS = 3;
    static char buffers[NUM_BUFFERS][8]; // "XXX/XXX";
    if (bufferIndex > (NUM_BUFFERS - 1)) {
      return "---/---";
    }

    char* string = buffers[bufferIndex]; 
    snprintf(string, 8, "%s/%u", deviceTypeToString(type), address);
    if (address == DEVICE_ADDR_ANY) {
      string[4] = '*';
      string[5] = '\0';
    }
    return string;
  }

  static toolbox::Maybe<DeviceId> fromString(const toolbox::strref& string) {
    if (string.length() < 5) {
      return {};
    }
    if (string.charAt(3) != '/') {
      return {};
    }

    DeviceType type = deviceTypeFromString(string.leftmost(3));
    DeviceAddress address;
    if (string.charAt(4) == '*') {
      address = DEVICE_ADDR_ANY;
    } else {
      address = iot_core::convert<DeviceAddress>::fromString(string.skip(4).cstr(), nullptr, 10);
    }
    return DeviceId{type, address};
  }

  bool operator==(DeviceId const& other) const { return type == other.type && address == other.address; }
  bool operator!=(DeviceId const& other) const { return !(*this == other); }
  bool operator<(DeviceId const& other) const { return type < other.type || (type == other.type && address < other.address); }
};

using ValueId = uint16_t;

struct WriteData {
  DeviceId sourceId;
  DeviceId targetId;
  ValueId valueId;
  uint16_t value;

  WriteData(DeviceId sourceId, DeviceId targetId, ValueId valueId, uint16_t value) : sourceId(sourceId), targetId(targetId), valueId(valueId), value(value) {}
};

struct RequestData {
  DeviceId sourceId;
  DeviceId targetId;
  ValueId valueId;
  
  RequestData(DeviceId sourceId, DeviceId targetId, ValueId valueId) : sourceId(sourceId), targetId(targetId), valueId(valueId) {}
};

struct ResponseData {
  DeviceId sourceId;
  DeviceId targetId;
  ValueId valueId;
  uint16_t value;

  ResponseData(DeviceId sourceId, DeviceId targetId, ValueId valueId, uint16_t value) : sourceId(sourceId), targetId(targetId), valueId(valueId), value(value) {}
};

const ValueId UNKNOWN_VALUE_ID = 0u;

// "Special" single byte value IDs:
const uint8_t VALUE_ID_EXTENDED = 0xFAu; // signals that a 16-bit value ID is used for transfer
const uint8_t VALUE_ID_SYSTEM_RESET = 0xFBu;
const uint8_t VALUE_ID_CAN_ERROR = 0xFCu;
const uint8_t VALUE_ID_BUS_CONFIGURATION = 0xFDu;
const uint8_t VALUE_ID_INITIALIZATION = 0xFEu;
const uint8_t VALUE_ID_INVALID = 0xFFu;

// Constants for known IDs and addresses

const DeviceId SYSTEM_ID = {DeviceType::System, 0u};
const DeviceId HEATING_CIRCUIT_1_ID = {DeviceType::HeatingCircuit, 1u};
const DeviceId HEATING_CIRCUIT_2_ID = {DeviceType::HeatingCircuit, 2u};

/* 
 * Available "display" addresses, which can be used by devices on the bus to interact with the system.
 * The built-in control panel/display uses one.
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

#endif
