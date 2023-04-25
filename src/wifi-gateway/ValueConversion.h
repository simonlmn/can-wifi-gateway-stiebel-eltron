#pragma once

#ifdef TEST_ENV
#include <cstdint>
#include <cstdio>
#include <cstring>
#endif

#include "Utils.h"

/*
   From https://www.stiebel-eltron.de/content/dam/ste/cdbassets/historic/bedienungs-_u_installationsanleitungen/ISG_Modbus__b89c1c53-6d34-4243-a630-b42cf0633361.pdf

   Data type | Value range       | Multiplier (to encode value) | Signed
       2     | -3276.8 to 3276.7 | 10                           | Yes
       6     | 0 to 65535        | 1                            | No
       7     | -327.68 to 327.67 | 100                          | Yes
       8     | 0 to 255          | 1                            | No
*/

const char* getRawValueAsDecString(uint16_t rawValue) {
  static char buffer[6]; // 65535
  snprintf(buffer, 6, "%05u", rawValue);
  return buffer;
}

const char* getRawValueAsHexString(uint16_t rawValue) {
  static char buffer[7]; // 0xFFFF
  snprintf(buffer, 7, "0x%04X", rawValue);
  return buffer;
}

enum struct Unit : uint8_t {
  Unknown = 0,
  None,
  Percent,
  RelativeHumidity,
  Hertz,
  Celsius,
  Kelvin,
  Bar,
  KelvinPerMinute,
  LiterPerMinute,
  CubicmeterPerHour,
  Years,
  Year,
  Months,
  Month,
  Days,
  Day,
  Weekday,
  Hours,
  ThousandHours,
  Hour,
  Minutes,
  Minute,
  Seconds,
  Second,
  Watt,
  KiloWatt,
  WattHour,
  KiloWattHour,
  MegaWattHour,
  Ampere,
  Volt
};

const char* getUnitSymbol(Unit unit) {
  switch (unit) {
    case Unit::Unknown: return "?";
    case Unit::None: return "";
    case Unit::Percent: return "%";
    case Unit::RelativeHumidity: return "% RH";
    case Unit::Hertz: return "Hz";
    case Unit::Celsius: return "°C";
    case Unit::Kelvin: return "K";
    case Unit::Bar: return "bar";
    case Unit::KelvinPerMinute: return "K/min";
    case Unit::LiterPerMinute: return "l/min";
    case Unit::CubicmeterPerHour: return "m^3/h";
    case Unit::Years: return "years";
    case Unit::Year: return "";
    case Unit::Months: return "months";
    case Unit::Month: return "";
    case Unit::Days: return "days";
    case Unit::Day: return "";
    case Unit::Weekday: return "";
    case Unit::Hours: return "h";
    case Unit::ThousandHours: return "1000h";
    case Unit::Hour: return "";
    case Unit::Minutes: return "min";
    case Unit::Minute: return "";
    case Unit::Seconds: return "s";
    case Unit::Second: return "";
    case Unit::Watt: return "W";
    case Unit::KiloWatt: return "kW";
    case Unit::WattHour: return "Wh";
    case Unit::KiloWattHour: return "kWh";
    case Unit::MegaWattHour: return "MWh";
    case Unit::Ampere: return "A";
    case Unit::Volt: return "V";
    default: return "?";
  }
}

struct Codec { // Contract
  static int32_t decode(uint16_t value) {
    return value;
  }

  // should only return 0 - UINT16_MAX or UINT16_MAX + n for errors
  static uint32_t encode(int32_t value) {
    return value;
  }

  static bool isError(uint32_t rawValue) {
    return rawValue > UINT16_MAX;
  }
};

struct Unsigned8BitCodec {
  static int32_t decode(uint16_t value) {
    return value >> 8;
  }
  static uint32_t encode(int32_t value) {
    return value << 8;
  }
};

struct Unsigned16BitCodec {
  static int32_t decode(uint16_t value) {
    return value;
  }
  static uint32_t encode(int32_t value) {
    return value;
  }
};

struct Signed16BitCodec {
  static int32_t decode(uint16_t value) {
    return value > 0x8000u ? (int32_t)value - 0x10000 : (int32_t)value;
  }
  static uint32_t encode(int32_t value) {
    return (value < 0 ? value + 0x10000 : value);
  }
};

/*
   Note: the returned string is only guaranteed to be valid until the next
   invocation of the converter, as it may reuse the same buffer over again.
*/
struct ValueConverter { // Contract
  static const int32_t ERROR_RAW_VALUE = UINT16_MAX + 1;
  static const char* ERROR_STRING_VALUE;
  
  // one shared buffer to reduce memory usage, size must accommodate largest use-case
  static const size_t maxLength = 256u;
  static char buffer[maxLength + 1];
  
  static char* getBuffer() {
    buffer[0] = '\0';
    return buffer;
  }

  // should return a string representation which can be directly used as a JSON value
  static const char* fromRaw(int32_t value) {
    return ERROR_STRING_VALUE;
  }
  
  static int32_t toRaw(const char* value, const char* end = nullptr) {
    return ERROR_RAW_VALUE;
  }

  static const char* description() {
    return "null";
  }
};

const char* ValueConverter::ERROR_STRING_VALUE = "null";
char ValueConverter::buffer[ValueConverter::maxLength + 1];

template<typename Codec, typename Converter>
struct ValueConversion {
  static const char* fromRaw(uint16_t value) {
    return Converter::fromRaw(Codec::decode(value));
  }
  
  static uint32_t toRaw(const char* value, const char* end = nullptr) {
    return Codec::encode(Converter::toRaw(value, end));
  }
};

// decimalPoint should be in the range -+6
template<int8_t decimalPoint = 0>
struct NumericValueConverter {
  static const char* fromRaw(int32_t value) {
    const size_t maxLength = ValueConverter::maxLength;
    char* buffer = ValueConverter::getBuffer();
    
    if (value == 0) {
      return "0";
    }
    
    snprintf(buffer, maxLength, "%i", value);
    
    if (decimalPoint < 0) {
      // insert point, e.g. for -1:
      // "1" -> "0.1"
      // "-1" -> "-0.1"
      // "65568" -> "6556.8"
      // "-32768" -> "-3276.8"

      // insert point, e.g. for -6:
      // "1" -> "0.000001"
      // "-1" -> "-0.000001"
      // "65568" -> "0.065568"
      // "-32768" -> "-0.032768"
      
      auto numberOfDecimals = -decimalPoint; // e.g. -6 -> 6
      ssize_t lengthOfNumber = strlen(buffer); // e.g. "-12" -> 3
      ssize_t remainingLength = maxLength - lengthOfNumber;
      
      if (remainingLength < (numberOfDecimals + 1)) {
        return "NaN";
      } else {
        auto signLength = value < 0 ? 1 : 0;
        ssize_t numberOfDigits = lengthOfNumber - signLength;
        ssize_t numberOfIntegerDigits = numberOfDigits - numberOfDecimals;
        
        if (numberOfIntegerDigits <= 0) {
          // Add zero padding and point
          auto numberOfZeroesAndPoint = -numberOfIntegerDigits + 1 + 1;
          for (int i = lengthOfNumber - 1; i >= signLength; --i) {
            buffer[numberOfZeroesAndPoint + i] = buffer[i];
          }
          for (int i = 0; i < numberOfZeroesAndPoint; ++i) {
            buffer[signLength + i] = '0';
          }
          buffer[signLength + 1] = '.';
          buffer[signLength + numberOfZeroesAndPoint + numberOfDigits] = '\0';
        } else {
          // Insert point
          for (int i = 0; i < numberOfDecimals; ++i) {
            buffer[signLength + numberOfDigits - i] = buffer[signLength + numberOfDigits - i - 1];
          }
          buffer[signLength + numberOfDigits - numberOfDecimals] = '.';
          buffer[signLength + numberOfDigits + 1] = '\0';
        }
      }
    } else if (decimalPoint > 0) {
      // append zeroes, e.g. for +6:
      // "1" -> "1000000"
      // "-1" -> "-1000000"
      // "65568" -> "65568000000"
      // "-32768" -> "-32768000000"
      
      auto numberOfZeroesToAppend = decimalPoint;
      ssize_t lengthOfNumber = strlen(buffer);
      ssize_t remainingLength = maxLength - lengthOfNumber;
      
      if (remainingLength < numberOfZeroesToAppend) {
        return value < 0 ? "-Infinity" : "Infinity";
      } else {
        while (numberOfZeroesToAppend) {
          buffer[lengthOfNumber] = '0';
          lengthOfNumber += 1;
          numberOfZeroesToAppend -= 1;
        }
        buffer[lengthOfNumber] = '\0';
      }
    }

    return buffer;
  }
  
  static int32_t toRaw(const char* value, const char* end = nullptr) {
    const size_t maxLength = ValueConverter::maxLength;
    char* buffer = ValueConverter::getBuffer();

    ssize_t length = end == nullptr ? strlen(value) : (end - value);

    if (length > maxLength) {
      return ValueConverter::ERROR_RAW_VALUE;
    }

    strncpy(buffer, value, length + 1);

    if (decimalPoint < 0) {
      ssize_t decimalPointPos = length + decimalPoint - 1;
      if (memchr(value, '.', length) == nullptr) { // the value does not have a decimal point (=integer)
        length += -decimalPoint + 1; // extend it by the decimal point and as many zeroes as necesary
        decimalPointPos = length + decimalPoint - 1;
        buffer[decimalPointPos] = '.';
        for (int i = 0; i < -decimalPoint; ++i) {
          buffer[decimalPointPos + i + 1] = '0';
        }
        buffer[decimalPointPos + -decimalPoint + 1] = '\0';
      }
      
      if (decimalPointPos <= 0 || buffer[decimalPointPos] != '.') { // value not correct
          return ValueConverter::ERROR_RAW_VALUE;
      }

      for (int i = decimalPointPos; i < length; ++i) {
          buffer[i] = buffer[i + 1];
      }
    } else {
      ssize_t truncatePos = length - decimalPoint;
      if (truncatePos <= 0) { // value not correct
          return ValueConverter::ERROR_RAW_VALUE;
      }

      buffer[truncatePos] = '\0';
    }

    buffer[length] = '\0';
    return strtol(buffer, nullptr, 10);
  }
  static const char* description() {
    return "{}";
  }
};

template<typename Bitfield>
struct BitfieldConverter {
  static const char* fromRaw(int32_t value) {
    const size_t maxLength = ValueConverter::maxLength - 2; // - "{}"
    char* buffer = ValueConverter::getBuffer();

    size_t currentPosition = 0;

    buffer[currentPosition++] = '{';

    bool hasAtLeastOne = false;
    for (uint8_t i = 0; i < 16u; ++i) {
      size_t valueNameLength = strlen(Bitfield::VALUES[i]);
      if (valueNameLength > 0) {
        size_t fieldLength = valueNameLength + 2 /* length of quotes */ + 6 /* length of ":false" or ":true" */ + (hasAtLeastOne ? 1 : 0) /* length of "," separator, if needed */;
  
        if (currentPosition + fieldLength > maxLength) {
          // we cannot add more, so we just return what we have so far even if it means we lose data...
          return buffer;
        }
        
        if (hasAtLeastOne) {
          buffer[currentPosition++] = ',';
        }

        buffer[currentPosition++] = '"';
        strcpy(buffer + currentPosition, Bitfield::VALUES[i]);
        currentPosition += valueNameLength;
        buffer[currentPosition++] = '"';
        
        buffer[currentPosition++] = ':';
        
        int32_t fieldMask = 1 << i;
        if ((value & fieldMask) != 0) {
          strcpy(buffer + currentPosition, "true");
          currentPosition += 4;
        } else {
          strcpy(buffer + currentPosition, "false");
          currentPosition += 5;
        }
  
        hasAtLeastOne = true;
      }
    }

    buffer[currentPosition++] = '}';
    buffer[currentPosition++] = '\0';
    
    return buffer;
  }

  static int32_t toRaw(const char* value, const char* end = nullptr) {
    const size_t maxLength = ValueConverter::maxLength;
    char* buffer = ValueConverter::getBuffer();
    
    auto length = end == nullptr ? strlen(value) : (end - value);
    if (length > maxLength || length < 2) {
      return ValueConverter::ERROR_RAW_VALUE;
    }

    uint16_t result = 0;

    strncpy(buffer, value + 1, length - 2);
    buffer[length - 2] = '\0';
    
    char* fieldValue = strtok(buffer, ",");
    while (fieldValue != NULL) {
      result |= bitmaskOf(fieldValue);
      fieldValue = strtok(NULL, ",");
    }

    return result;
  }

  static uint16_t bitmaskOf(const char* fieldValue) {
    size_t length = strlen(fieldValue);
    if (length < 8) { // must be at least ".":(true|false)
      return 0;
    }

    if (strcmp(fieldValue + (length - 4), "true") == 0) {
      for (uint8_t i = 0; i < 16u; ++i) {
        size_t valueLength = strlen(Bitfield::VALUES[i]);
        if (strncmp(fieldValue + 1, Bitfield::VALUES[i], valueLength) == 0 && fieldValue[1 + valueLength] == '"') {
          return 1 << i;
        }
      }
    }

    return 0;
  }
  static const char* description() {
    return fromRaw(0);
  }
};

struct BooleanConverter {
  static const char* fromRaw(int32_t value) {
    if (value == 0) {
      return "false";
    } else if (value == 1) {
      return "true";
    } else {
      return ValueConverter::ERROR_STRING_VALUE;
    }
  }
  static int32_t toRaw(const char* value, const char* end = nullptr) {
    if (strncmp(value, "true", end == nullptr ? 6 : (end - value)) == 0) return 1;
    if (strncmp(value, "false", end == nullptr ? 7 : (end - value)) == 0) return 0;
    return ValueConverter::ERROR_RAW_VALUE;
  }
  static const char* description() {
    return "[false,true]";
  }
};

struct IVUOperatingStatusBitfield {
  static const char* VALUES[16];
};
const char* IVUOperatingStatusBitfield::VALUES[] = {
  "AUTO", // SWITCHING PROGRAM ENABLED
  "COMPR", // COMPRESSOR
  "HEAT", // HEATING
  "COOL", // COOLING
  "DHW", // DOMESTIC HOT WATER
  "ELHEAT", // ELECTRIC REHEATING
  "SERV", // SERVICE
  "PWROFF", // POWER-OFF
  "FILTER", // FILTER
  "VENT", // UNSCHEDULED VENTILATION
  "HCPUMP", // HEATING CIRCUIT PUMP
  "DEFROS", // EVAPORATOR DEFROST
  "FILTXA", // FILTER EXTRACT AIR
  "FILTVA", // FILTER VENTILATION AIR
  "HEATUP", // HEAT-UP PROGRAM
  ""
};

struct IVUOperatingModeEnumConverter {
  static const char* fromRaw(int32_t value) {
    switch (value) {
      case 11: return "\"AUTOMATIC\"";
      case 1: return "\"STANDBY\"";
      case 3: return "\"DAY MODE\"";
      case 4: return "\"SETBACK MODE\"";
      case 5: return "\"DHW\"";
      case 14: return "\"MANUAL MODE\"";
      case 0: return "\"EMERGENCY OPERATION\"";
      default: return "null";
    }
  }
  static int32_t toRaw(const char* value, const char* end = nullptr) {
    size_t n = end == nullptr ? strlen(value) : (end - value);
    if (strncmp(value, "\"AUTOMATIC\"", n) == 0) return 11;
    if (strncmp(value, "\"STANDBY\"", n) == 0) return 1;
    if (strncmp(value, "\"DAY MODE\"", n) == 0) return 3;
    if (strncmp(value, "\"SETBACK MODE\"", n) == 0) return 4;
    if (strncmp(value, "\"DHW\"", n) == 0) return 5;
    if (strncmp(value, "\"MANUAL MODE\"", n) == 0) return 14;
    if (strncmp(value, "\"EMERGENCY OPERATION\"", n) == 0) return 0;
    return ValueConverter::ERROR_RAW_VALUE;
  }
  static const char* description() {
    return "[\"AUTOMATIC\",\"STANDBY\",\"DAY MODE\",\"SETBACK MODE\",\"DHW\",\"MANUAL MODE\",\"EMERGENCY OPERATION\"]";
  }
};

struct WeekdayEnumConverter {
  static const char* fromRaw(int32_t value) {
    switch (value) {
      case 0: return "\"Monday\"";
      case 1: return "\"Tuesday\"";
      case 2: return "\"Wednesday\"";
      case 3: return "\"Thursday\"";
      case 4: return "\"Friday\"";
      case 5: return "\"Saturday\"";
      case 6: return "\"Sunday\"";
      default: return "null";
    }
  }
  static int32_t toRaw(const char* value, const char* end = nullptr) {
    size_t n = end == nullptr ? strlen(value) : (end - value);
    if (strncmp(value, "\"Monday\"", n) == 0) return 0;
    if (strncmp(value, "\"Tuesday\"", n) == 0) return 1;
    if (strncmp(value, "\"Wednesday\"", n) == 0) return 2;
    if (strncmp(value, "\"Thursday\"", n) == 0) return 3;
    if (strncmp(value, "\"Friday\"", n) == 0) return 4;
    if (strncmp(value, "\"Saturday\"", n) == 0) return 5;
    if (strncmp(value, "\"Sunday\"", n) == 0) return 6;
    return ValueConverter::ERROR_RAW_VALUE;
  }
  static const char* description() {
    return "[\"Monday\",\"Tuesday\",\"Wednesday\",\"Thursday\",\"Friday\",\"Saturday\",\"Sunday\"]";
  }
};

struct ValvePositionEnumConverter {
  static const char* fromRaw(int32_t value) {
    switch (value) {
      case 0: return "\"DHW\"";
      case 2: return "\"HC\"";
      default: return "null";
    }
  }
  static int32_t toRaw(const char* value, const char* end = nullptr) {
    size_t n = end == nullptr ? strlen(value) : (end - value);
    if (strncmp(value, "\"DHW\"", n) == 0) return 0;
    if (strncmp(value, "\"HC\"", n) == 0) return 2;
    return ValueConverter::ERROR_RAW_VALUE;
  }
  static const char* description() {
    return "[\"DHW\",\"HC\"]";
  }
};
