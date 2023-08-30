#ifndef STIEBELELTRONTYPES_H_
#define STIEBELELTRONTYPES_H_

#ifdef TEST_ENV
#include <cstdint>
#include <cstdio>
#endif

#include "src/iot-core/Utils.h"

using ValueId = uint16_t;

const ValueId UNKNOWN_VALUE_ID = 0u;

enum struct DeviceType : uint8_t {
  System = 0x03u,
  HeatingCircuit = 0x06u,
  Sensor = 0x08u,
  Display = 0x0Du,
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

DeviceType deviceTypeFromString(const char* type, size_t length = SIZE_MAX) {
  if (strncmp(type, "SYS", length) == 0) return DeviceType::System;
  if (strncmp(type, "HEA", length) == 0) return DeviceType::HeatingCircuit;
  if (strncmp(type, "SEN", length) == 0) return DeviceType::Sensor;
  if (strncmp(type, "DIS", length) == 0) return DeviceType::Display;
  if (strncmp(type, "ANY", length) == 0) return DeviceType::Any;
  if (strnlen(type, length) == 3 && (type[0] == 'X')) {
    return DeviceType(iot_core::convert<uint8_t>::fromString(type + 1, nullptr, 16));
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

  const char* toString() const {
    static char string[8]; // "XXX/XXX";
    snprintf(string, 8, "%s/%u", deviceTypeToString(type), address);
    if (address == DEVICE_ADDR_ANY) {
      string[4] = '*';
      string[5] = '\0';
    }
    return string;
  }

  static iot_core::Maybe<DeviceId> fromString(const char* string, char** end = nullptr) {
    if (strnlen(string, 5) < 5) {
      return {};
    }
    if (string[3] != '/') {
      return {};
    }

    DeviceType type = deviceTypeFromString(string, 3);
    DeviceAddress address;
    if (string[4] == '*') {
      address = DEVICE_ADDR_ANY;
      if (end != nullptr) {
        *end = const_cast<char*>(&string[5]);
      }
    } else {
      address = iot_core::convert<DeviceAddress>::fromString(string + 4, end, 10);
    }
    return DeviceId{type, address};
  }

  bool operator==(DeviceId const& other) const { return type == other.type && address == other.address; }
  bool operator!=(DeviceId const& other) const { return !(*this == other); }
  bool operator<(DeviceId const& other) const { return type < other.type || (type == other.type && address < other.address); }
};

struct WriteData {
  DeviceId sourceId;
  DeviceId targetId;
  ValueId valueId;
  uint16_t value;

  WriteData(DeviceId targetId, ValueId valueId, uint16_t value) : sourceId(), targetId(targetId), valueId(valueId), value(value) {}
  WriteData(DeviceId sourceId, DeviceId targetId, ValueId valueId, uint16_t value) : sourceId(sourceId), targetId(targetId), valueId(valueId), value(value) {}
};

struct RequestData {
  DeviceId sourceId;
  DeviceId targetId;
  ValueId valueId;
  
  RequestData(DeviceId targetId, ValueId valueId) : sourceId(), targetId(targetId), valueId(valueId) {}
  RequestData(DeviceId sourceId, DeviceId targetId, ValueId valueId) : sourceId(sourceId), targetId(targetId), valueId(valueId) {}
};

struct ResponseData {
  DeviceId sourceId;
  DeviceId targetId;
  ValueId valueId;
  uint16_t value;
};

#endif
