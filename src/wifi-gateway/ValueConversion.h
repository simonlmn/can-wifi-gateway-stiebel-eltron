#ifndef VALUECONVERSION_H_
#define VALUECONVERSION_H_

#include <iot_core/Utils.h>
#include <iot_core/Interfaces.h>
#include <toolbox/FixedCapacityMap.h>
#include <toolbox/Repository.h>
#include <jsons.h>
#include <LittleFS.h>
#include <algorithm>
#include "StiebelEltronTypes.h"

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

class ICodec {
public:
  virtual toolbox::Maybe<int32_t> decode(uint16_t value) const = 0;
  virtual toolbox::Maybe<uint16_t> encode(const toolbox::Maybe<int32_t>& value) const = 0;
  virtual const char* describe() const = 0;
  virtual const char* key() const = 0;
};

class IConverter {
public:
  virtual void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const = 0;
  virtual toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const = 0;
  virtual const char* describe() const = 0;
  virtual const char* key() const = 0;
};

class ICustomConverter : public IConverter {
public:
  virtual const toolbox::strref& type() const = 0;
  virtual void serialize(jsons::IWriter& output) const = 0;
  virtual bool deserialize(jsons::Value& input) = 0;
};

struct NoneCodec final : public ICodec {
  static const NoneCodec INSTANCE;
  toolbox::Maybe<int32_t> decode(uint16_t value) const override {
    return {};
  }
  toolbox::Maybe<uint16_t> encode(const toolbox::Maybe<int32_t>& value) const override {
    return {};
  }
  const char* describe() const override {
    return "none";
  }
  const char* key() const override {
    return "";
  }
};
const NoneCodec NoneCodec::INSTANCE {};

struct Unsigned8BitCodec final : public ICodec {
  static const Unsigned8BitCodec INSTANCE;
  toolbox::Maybe<int32_t> decode(uint16_t value) const override {
    if ((value & 0xFFu) == 0u) {
      return value >> 8;
    } else {
      return {};
    }
  }
  toolbox::Maybe<uint16_t> encode(const toolbox::Maybe<int32_t>& value) const override {
    if (value && value.get() <= 0xFFu) {
      return uint16_t(value.get() << 8);
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return "8 bit, unsigned";
  }
  const char* key() const override {
    return "U8";
  }
};
const Unsigned8BitCodec Unsigned8BitCodec::INSTANCE {};

struct Signed8BitCodec final : public ICodec {
  static const Signed8BitCodec INSTANCE;
  toolbox::Maybe<int32_t> decode(uint16_t value) const override {
    if ((value & 0xFFu) == 0u) {
      value = value >> 8;
      return value > 0x80u ? (int32_t)value - 0x100 : (int32_t)value;
    } else {
      return {};
    }
  }
  toolbox::Maybe<uint16_t> encode(const toolbox::Maybe<int32_t>& value) const override {
    if (value && value.get() > -0x80 && value.get() <= 0x80) {
      return uint16_t(value.get() < 0 ? value.get() + 0x100 : value.get()) << 8;
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return "8 bit, signed";
  }
  const char* key() const override {
    return "S8";
  }
};
const Signed8BitCodec Signed8BitCodec::INSTANCE {};

struct Unsigned16BitCodec final : public ICodec {
  static const Unsigned16BitCodec INSTANCE;
  toolbox::Maybe<int32_t> decode(uint16_t value) const override {
    return value;
  }
  toolbox::Maybe<uint16_t> encode(const toolbox::Maybe<int32_t>& value) const override {
    if (value && value.get() >= 0 && value.get() <= 0xFFFF) {
      return uint16_t(value.get());
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return "16 bit, unsigned";
  }
  const char* key() const override {
    return "U16";
  }
};
const Unsigned16BitCodec Unsigned16BitCodec::INSTANCE {};

struct Signed16BitCodec final : public ICodec {
  static const Signed16BitCodec INSTANCE;
  toolbox::Maybe<int32_t> decode(uint16_t value) const override {
    return value > 0x8000u ? (int32_t)value - 0x10000 : (int32_t)value;
  }
  toolbox::Maybe<uint16_t> encode(const toolbox::Maybe<int32_t>& value) const override {
    if (value && value.get() > -0x8000 && value.get() <= 0x8000) {
      return uint16_t(value.get() < 0 ? value.get() + 0x10000 : value.get());
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return "16 bit, signed";
  }
  const char* key() const override {
    return "S16";
  }
};
const Signed16BitCodec Signed16BitCodec::INSTANCE {};

class NoneConverter final : public IConverter {
public:
  static const NoneConverter INSTANCE;
  void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const override {
    output.null();
  }
  toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const override {
    return {};
  }
  const char* describe() const override {
    return "none";
  }
  const char* key() const override {
    return "";
  }
};
const NoneConverter NoneConverter::INSTANCE {};

class HexStringConverter final : public IConverter {
public:
  static HexStringConverter INSTANCE;
  void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const override {
    if (value) {
      output.string(toolbox::format("%04x", value.get()));
    } else {
      output.null();
    }
  }
  toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const override {
    auto string = input.asString();
    if (string) {
      return strtol(string.get().ref(), nullptr, 16); // TODO replace with strref based conversion
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return "raw value as hex string";
  }
  const char* key() const override {
    return "raw";
  }
};
HexStringConverter HexStringConverter::INSTANCE {};

class BooleanConverter final : public IConverter {
public:
  static BooleanConverter INSTANCE;
  void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const override {
    if (value == 0) {
      output.boolean(false);
    } else if (value == 1) {
      output.boolean(true);
    } else {
      output.null();
    }
  }
  toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const override {
    auto boolean = input.asBoolean();
    if (boolean) {
      return boolean.get() ? 1 : 0;
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return "boolean (0 = false, 1 = true)";
  }
  const char* key() const override {
    return "bool";
  }
};
BooleanConverter BooleanConverter::INSTANCE {};

template<uint8_t _decimalPlaces>
class NumericValueConverter final : public IConverter {
  static const toolbox::str<5> KEY;
public:
  static const NumericValueConverter INSTANCE;
  void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const override {
    if (value) {
      output.number(toolbox::Decimal::fromFixedPoint(value.get(), _decimalPlaces));
    } else {
      output.null();
    }
  }
  toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const override {
    auto decimal = input.asDecimal();
    if (decimal) {
      return decimal.get().toFixedPoint(_decimalPlaces); // TODO implement/use toFixedPoint for int32 or check if result fits
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return _decimalPlaces == 0 ? "integer number" : toolbox::format("decimal number (scale 10^%i)", -_decimalPlaces);
  }
  const char* key() const override {
    return KEY;
  }
};
template<uint8_t _decimalPlaces>
const toolbox::str<5> NumericValueConverter<_decimalPlaces>::KEY { _decimalPlaces == 0 ? "int" : toolbox::format("10^%i", -_decimalPlaces)};
template<uint8_t _decimalPlaces>
const NumericValueConverter<_decimalPlaces> NumericValueConverter<_decimalPlaces>::INSTANCE {};

static const size_t MAX_BITFIELD_FIELDS = 16u;
static const size_t MAX_BITFIELD_NAME_LENGTH = 8u;

class Bitfield final {
  char _fields[MAX_BITFIELD_FIELDS][MAX_BITFIELD_NAME_LENGTH] = {};

public:
  Bitfield() {}

  Bitfield(std::initializer_list<toolbox::strref> fields) {
    size_t i = 0u;
    for (auto name : fields) {
      if (i >= MAX_BITFIELD_FIELDS) break;
      name.copy(_fields[i], MAX_BITFIELD_NAME_LENGTH, true);
      ++i;
    }

    for (; i < MAX_BITFIELD_FIELDS; ++i) {
      toolbox::strref(toolbox::format("#BIT%i", i)).copy(_fields[i], MAX_BITFIELD_NAME_LENGTH, true);
    }
  }

  void set(size_t i, const toolbox::strref& name) {
    name.copy(_fields[i], MAX_BITFIELD_NAME_LENGTH, true);
  }

  toolbox::strref at(size_t i) const {
    return {_fields[i]};
  }

  size_t find(const toolbox::strref& name) const {
    for (size_t i = 0; i < MAX_BITFIELD_FIELDS; ++i) {
      if (name == _fields[i]) {
        return i;
      }
    }
    return MAX_BITFIELD_FIELDS;
  }
};

class BitfieldConverter final : public ICustomConverter {
  toolbox::str<20> _key;
  Bitfield _fields;

public:
  BitfieldConverter() {}
  BitfieldConverter(const toolbox::strref& key, const Bitfield& fields) : _key(key), _fields(fields) {}

  void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const override {
    if (value) {
      output.openObject();
      for (uint8_t i = 0; i < MAX_BITFIELD_FIELDS; ++i) {
        auto fieldName = _fields.at(i);
        if (fieldName.length() > 0) {
          int32_t fieldMask = 1 << i;
          output.property(fieldName).boolean((value.get() & fieldMask) != 0);
        }
      }
      output.close();
    } else {
      output.null();
    }
  }
  toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const override {
    auto object = input.asObject();
    if (object.valid()) {
      uint16_t result = 0;
      for (auto& property : object) {
        size_t bit = _fields.find(property.name());
        auto boolean = property.asBoolean();
        if (bit < MAX_BITFIELD_FIELDS && boolean) {
          if (boolean.get()) {
            result |= (1u << bit);
          } else {
            result &= ~(1u << bit);
          }
        } else {
          return {};
        }
      }
      return result;
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return toolbox::format("%s bitfield", key());
  }
  const char* key() const override {
    return _key;
  }

  static const toolbox::strref TYPE;
  const toolbox::strref& type() const override {
    return TYPE;
  }
  void serialize(jsons::IWriter& output) const override {
    output.openObject();
    output.property("key").string(_key);
    output.property("fields");
    output.openList();
    for (size_t i = 0; i < MAX_BITFIELD_FIELDS; ++i) {
      output.string(_fields.at(i));
    }
    output.close();
    output.close();
  }
  bool deserialize(jsons::Value& input) override {
    auto object = input.asObject();
    if (object.valid()) {
      for (auto& property : object) {
        if (property.name() == "key" && property.type() == jsons::ValueType::String) {
          _key = property.asString();
        } else if (property.name() == "fields" && property.type() == jsons::ValueType::List) {
          size_t i = 0;
          for (auto& field : property.asList()) {
            auto fieldName = field.asString();
            if (fieldName) {
              _fields.set(i, fieldName.get());
            } else {
              return false;
            }
            ++i;
          }
        } else {
          return false;
        }
      }
      return true;
    } else {
      return false;
    }
  }
};
const toolbox::strref BitfieldConverter::TYPE {"bitfield"};

static const size_t MAX_ENUM_VALUES = 16u;
static const size_t MAX_ENUM_VALUE_LENGTH = 20u;

class EnumValue final {
  uint8_t _value = 0u;
  toolbox::str<MAX_ENUM_VALUE_LENGTH> _name = {};

public:
  EnumValue() {}
  EnumValue(uint8_t value, const toolbox::strref& name) : _value(value), _name(name) {}
  EnumValue(const EnumValue& other) : EnumValue(other._value, other._name) {}
  EnumValue& operator=(const EnumValue& other) {
    if (&other != this) {
      _value = other._value;
      _name = other._name; 
    }
    return *this;
  }

  uint8_t value() const { return _value; }
  toolbox::strref name() const { return _name; }
};

class Enum final {
  EnumValue _values[MAX_ENUM_VALUES] = {};
  size_t _length = 0u;

public:
  Enum() {}
  Enum(std::initializer_list<EnumValue> values) {
    _length = 0u;
    for (auto& value : values) {
      if (_length >= MAX_ENUM_VALUES) break;
      _values[_length] = value;
      ++_length;
    }
  }

  size_t size() const {
    return _length;
  }  

  void define(const EnumValue& value) {
    EnumValue* existing = byValue(value.value());
    if (!existing) {
      existing = byName(value.name());
    }

    if (existing) {
      *existing = value;
    } else if (_length < MAX_ENUM_VALUES) {
      _values[_length] = value;
      ++_length;
    }
  }

  const EnumValue* begin() const {
    return &_values[0];
  }

  const EnumValue* end() const {
    return &_values[_length];
  }

  const EnumValue* byValue(uint8_t value) const {
    for (size_t i = 0u; i < _length; ++i) {
      if (_values[i].value() == value) {
        return &_values[i];
      }
    }
    return nullptr;
  }

  EnumValue* byValue(uint8_t value) {
    for (size_t i = 0u; i < _length; ++i) {
      if (_values[i].value() == value) {
        return &_values[i];
      }
    }
    return nullptr;
  }

  const EnumValue* byName(const toolbox::strref& name) const {
    for (size_t i = 0u; i < _length; ++i) {
      if (name == _values[i].name()) {
        return &_values[i];
      }
    }
    return nullptr;
  }

  EnumValue* byName(const toolbox::strref& name) {
    for (size_t i = 0u; i < _length; ++i) {
      if (name == _values[i].name()) {
        return &_values[i];
      }
    }
    return nullptr;
  }
};

class EnumConverter final : public ICustomConverter {
  toolbox::str<20> _key;
  Enum _enum;

public:
  EnumConverter() {}
  EnumConverter(const toolbox::strref& key, const Enum& e) : _key(key), _enum(e) {}

  void toJson(const toolbox::Maybe<int32_t>& value, jsons::IWriter& output) const override {
    if (value && value.get() >= 0 && value.get() < 0xFF) {
      const EnumValue* v = _enum.byValue((uint8_t)value.get());
      if (v) {
        output.string(v->name());
      } else {
        output.null();
      }
    } else {
      output.null();
    }
  }
  toolbox::Maybe<int32_t> fromJson(jsons::Value& input) const override {
    auto string = input.asString();
    if (string) {
      const EnumValue* v = _enum.byName(string.get());
      if (v) {
        return v->value();
      } else {
        return {};
      }
    } else {
      return {};
    }
  }
  const char* describe() const override {
    return toolbox::format("%s enum", key());
  }
  const char* key() const override {
    return _key;
  }

  static const toolbox::strref TYPE;
  const toolbox::strref& type() const override {
    return TYPE;
  }
  void serialize(jsons::IWriter& output) const override {
    output.openObject();
    output.property("key").string(_key);
    output.property("enum");
    output.openObject();
    for (auto& value : _enum) {
      output.property(value.name()).number(value.value());
    }
    output.close();
    output.close();
  }
  bool deserialize(jsons::Value& input) override {
    auto object = input.asObject();
    if (object.valid()) {
      for (auto& property : object) {
        if (property.name() == "key" && property.type() == jsons::ValueType::String) {
          _key = property.asString();
        } else if (property.name() == "enum" && property.type() == jsons::ValueType::Object) {
          for (auto& property : property.asObject()) {
            auto integer = property.asInteger();
            if (integer) {
              _enum.define({integer.get(), property.name()});
            } else {
              return false;
            }
          }
        } else {
          return false;
        }
      }
      return true;
    } else {
      return false;
    }
  }
};
const toolbox::strref EnumConverter::TYPE {"enum"};

ICustomConverter* createConverter(const toolbox::strref& type) {
  if (type == EnumConverter::TYPE) {
    return new EnumConverter();
  } else if (type == BitfieldConverter::TYPE) {
    return new BitfieldConverter();
  } else {
    return nullptr;
  }
}

void serialize(jsons::IWriter& output, ICustomConverter* converter) {
  if (converter) {
    output.openObject();
    output.property(converter->type());
    converter->serialize(output);
    output.close();
  } else {
    output.null();    
  }
}

ICustomConverter* deserialize(jsons::Value& input, ICustomConverter* existing = nullptr) {
  auto object = input.asObject();
  if (object.valid()) {
    for (auto& property : object) {
      auto type = property.name();
      if (existing && existing->type() == type) {
        if (existing->deserialize(property)) {
          return existing;
        } else {
          return nullptr;
        }
      } else {
        ICustomConverter* newInstance = createConverter(type);
        if (newInstance && newInstance->deserialize(property)) {
          return newInstance;
        } else {
          delete newInstance;
          return nullptr;
        }
      }
    }
    return nullptr;
  } else {
    return nullptr;
  }
}

class Conversion final {
  const ICodec* _codec;
  const IConverter* _converter;

public:
  Conversion() {}
  Conversion(const ICodec* codec, const IConverter* converter) : _codec(codec), _converter(converter) {}

  bool isNull() const {
    return _codec == nullptr || _converter == nullptr;
  }

  const ICodec& codec() const {
    return _codec != nullptr ? *_codec : NoneCodec::INSTANCE;
  }

  const IConverter& converter() const {
    return _converter != nullptr ? *_converter : NoneConverter::INSTANCE;
  }

  void toJson(uint16_t value, jsons::IWriter& output) const {
    if (isNull()) {
      output.null();
    } else {
      _converter->toJson(_codec->decode(value), output);
    }
  }
  
  toolbox::Maybe<uint16_t> fromJson(jsons::Value& input) const {
    if (isNull()) {
      return {};
    } else {
      return _codec->encode(_converter->fromJson(input));
    }
  }
};

static const Conversion RAW_CONVERSION = { &Unsigned16BitCodec::INSTANCE, &HexStringConverter::INSTANCE };

using CodecId = uint8_t;
using ConverterId = uint8_t;

class IConversionRepository {
public:
  virtual toolbox::Iterable<const ICodec*> codecs() const = 0;
  virtual toolbox::Iterable<const IConverter*> builtinConverters() const = 0;
  virtual toolbox::Iterable<ICustomConverter* const> customConverters() const = 0;

  virtual const ICodec* getCodec(CodecId id) const = 0;
  virtual CodecId getCodecIdByKey(const toolbox::strref& key) const = 0;
  virtual const IConverter* getConverter(ConverterId id) const = 0;
  virtual ConverterId getConverterIdByKey(const toolbox::strref& key) const = 0;
};

class ICustomConverterRepository : public toolbox::IRepository {
public:
  virtual ICustomConverter* getCustomConverter(ConverterId id) const = 0;
  virtual bool store(ConverterId id, ICustomConverter* definition) = 0;
  virtual void remove(ConverterId id) = 0;
  virtual void removeAll() = 0;
};

static const CodecId NONE_CODEC_ID = 0u;
static const ConverterId NONE_CONVERTER_ID = 0u;

static const uint8_t CONVERTER_ID_INDEX_MASK = 0x7Fu;
static const uint8_t CONVERTER_ID_CUSTOM_FLAG = 0x80u;

constexpr bool isCustomConverterId(ConverterId id) {
  return (id & CONVERTER_ID_CUSTOM_FLAG) == CONVERTER_ID_CUSTOM_FLAG;
}

constexpr uint8_t converterIndex(ConverterId id) {
  return id & CONVERTER_ID_INDEX_MASK;
}

constexpr ConverterId customConverterId(uint8_t index) {
  return index | CONVERTER_ID_CUSTOM_FLAG;
}


static const Enum WeekdayEnum {
  { 0, "Monday" },
  { 1, "Tuesday" },
  { 2, "Wednesday" },
  { 3, "Thursday" },
  { 4, "Friday" },
  { 5, "Saturday" },
  { 6, "Sunday" }  
};

static const Enum ValvePositionEnum {
  { 0, "DHW" },
  { 2, "HC" }
};

static const Bitfield IVUOperatingStatus {
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

static const Enum IVUOperatingModeEnum {
  { 11, "AUTOMATIC" },
  { 1, "STANDBY" },
  { 3, "DAY MODE" },
  { 4, "SETBACK MODE" },
  { 5, "DHW" },
  { 14, "MANUAL MODE" },
  { 0, "EMERGENCY OPERATION" }
};

void defineExampleCustomConverters(ICustomConverterRepository& repository) {
  repository.store(customConverterId(0), new EnumConverter { F("Weekday"), WeekdayEnum });
  repository.store(customConverterId(1), new BitfieldConverter { F("OperatingStatus"), IVUOperatingStatus });
  repository.store(customConverterId(2), new EnumConverter { F("OperatingMode"), IVUOperatingModeEnum });
  repository.store(customConverterId(3), new EnumConverter { F("ValvePosition"), ValvePositionEnum });
}

static const ICodec* CODECS[5] {
  &NoneCodec::INSTANCE,
  &Unsigned8BitCodec::INSTANCE,
  &Signed8BitCodec::INSTANCE,
  &Unsigned16BitCodec::INSTANCE,
  &Signed16BitCodec::INSTANCE
};

static const IConverter* BUILT_IN_CONVERTERS[7] {
  &NoneConverter::INSTANCE,
  &HexStringConverter::INSTANCE,
  &BooleanConverter::INSTANCE,
  &NumericValueConverter<0>::INSTANCE,
  &NumericValueConverter<1>::INSTANCE,
  &NumericValueConverter<2>::INSTANCE,
  &NumericValueConverter<3>::INSTANCE
};

struct NullConversionRepository final : public IConversionRepository {
  toolbox::Iterable<const ICodec*> codecs() const override { return { nullptr, nullptr }; }
  toolbox::Iterable<const IConverter*> builtinConverters() const override { return { nullptr, nullptr }; }
  toolbox::Iterable<ICustomConverter* const> customConverters() const override { return { nullptr, nullptr }; }
  const ICodec* getCodec(CodecId id) const override { return nullptr; }
  CodecId getCodecIdByKey(const toolbox::strref& key) const override { return NONE_CODEC_ID; }
  const IConverter* getConverter(ConverterId id) const override { return nullptr; }
  ConverterId getConverterIdByKey(const toolbox::strref& key) const override { return NONE_CONVERTER_ID; }
};

class ConversionRepository final : public IConversionRepository, public ICustomConverterRepository, public iot_core::IApplicationComponent {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  ICustomConverter* _customConverters[8] {};
  size_t _converterCount = 0;
  bool _dirty = false;
  bool _defineExamplesIfEmpty = true;

  const IConverter* getBuiltInConverter(ConverterId id) {
    if (!isCustomConverterId(id)) {
      uint8_t i = converterIndex(id);
      if (i < std::size(BUILT_IN_CONVERTERS)) {
        return BUILT_IN_CONVERTERS[i];
      } else {
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  bool replaceCustomConverter(ConverterId id, ICustomConverter* converter) {
    if (!isCustomConverterId(id) || converterIndex(id) >= std::size(_customConverters)) {
      return false;
    }

    ICustomConverter* existing = _customConverters[converterIndex(id)];

    if (existing != converter) {
      if (existing) --_converterCount;

      delete existing;
      _customConverters[converterIndex(id)] = converter;

      if (converter) ++_converterCount;
    }

    return true;
  }

  void restoreConverters() {
    size_t stored = 0;
    size_t loaded = 0;
    for (size_t i = 0; i < std::size(_customConverters); ++i) {
      uint8_t converterId = customConverterId(i);
      auto file = LittleFS.open(toolbox::format(F("/cvt/custom/%u"), converterId), "r");
      if (file) {
        ++stored;
        toolbox::StreamInput input{file};
        auto reader = jsons::makeReader(input);
        auto json = reader.begin();
        replaceCustomConverter(converterId, deserialize(json, getCustomConverter(converterId)));
        reader.end();
        if (reader.failed()) {
          replaceCustomConverter(converterId, nullptr);
          _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to load converter %u: %s"), converterId, reader.diagnostics().errorMessage.toString().c_str()));
        } else {
          ++loaded;
          _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Loaded converter %u."), converterId));
        }
        file.close();
      } else {
        replaceCustomConverter(converterId, nullptr);
      }
    }
    _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Loaded converters (%u of %u)"), loaded, stored));
    _dirty = false;
  }

  void persistConverters() {
    size_t stored = 0;
    size_t loaded = 0;
    for (size_t i = 0; i < std::size(_customConverters); ++i) {
      uint8_t converterId = i | CONVERTER_ID_CUSTOM_FLAG;
      auto filename = toolbox::format(F("/cvt/custom/%u"), converterId);
      ICustomConverter* existing = getCustomConverter(converterId);
      if (existing) {
        ++loaded;
        auto file = LittleFS.open(filename, "w");
        if (file) {
          toolbox::PrintOutput output{file};
          auto writer = jsons::makeWriter(output);
          serialize(writer, existing);
          writer.end();
          file.close();
          if (writer.failed()) {
            _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to store converter %u."), converterId));
            LittleFS.remove(filename);
          } else {
            ++stored;
          }
        } else {
          _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to store converter %u."), converterId));
        }
      } else {
        LittleFS.remove(filename);
      }
    }
    _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Stored converters (%u of %u)"), loaded, stored));
    _dirty = loaded != stored;
  }

public:
  ConversionRepository(iot_core::ISystem& system) :
    _logger(system.logger("cvt")),
    _system(system)
  {}

  const char* name() const override {
    return "cvt";
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "examples") == 0) return setDefineExamples(iot_core::convert<bool>::fromString(value, false));
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("examples", iot_core::convert<bool>::toString(_defineExamplesIfEmpty));
  }

  bool setDefineExamples(bool enabled) {
    _defineExamplesIfEmpty = enabled;
    _logger.log(toolbox::format(F("Define example converters %s."), enabled ? "enabled" : "disabled"));
    if (_defineExamplesIfEmpty && _converterCount == 0) {
      defineExampleCustomConverters(*this);
    }
    return true;
  }

  void setup(bool /*connected*/) override {
    restoreConverters();
    if (_defineExamplesIfEmpty && _converterCount == 0) {
      defineExampleCustomConverters(*this);
    }
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector&) const override {
  }

  toolbox::Iterable<const ICodec*> codecs() const override {
    return { std::begin(CODECS), std::end(CODECS) };
  }

  toolbox::Iterable<const IConverter*> builtinConverters() const override {
    return { std::begin(BUILT_IN_CONVERTERS), std::end(BUILT_IN_CONVERTERS) };
  }

  toolbox::Iterable<ICustomConverter* const> customConverters() const override {
    return { std::begin(_customConverters), std::end(_customConverters) };
  }

  const ICodec* getCodec(CodecId id) const override {
    if (id < std::size(CODECS)) {
      return CODECS[id];
    } else {
      return &NoneCodec::INSTANCE;
    }
  }

  CodecId getCodecIdByKey(const toolbox::strref& key) const override {
    CodecId id = 0;
    for (auto& codec : CODECS) {
      if (key == codec->key()) {
        return id;
      }
      ++id;
    }
    return NONE_CODEC_ID;
  }

  const IConverter* getConverter(ConverterId id) const override {
    size_t index = converterIndex(id);
    if (isCustomConverterId(id)) {
      if (index < std::size(_customConverters) && _customConverters[index]) {
        return _customConverters[index];
      }
    } else {
      if (index < std::size(BUILT_IN_CONVERTERS)) {
        return BUILT_IN_CONVERTERS[index];
      }
    }
    return &NoneConverter::INSTANCE;
  }

  ConverterId getConverterIdByKey(const toolbox::strref& key) const override {
    ConverterId id = 0;
    for (auto& converter : BUILT_IN_CONVERTERS) {
      if (key == converter->key()) {
        return id;
      }
      ++id;
    }
    id = customConverterId(0);
    for (auto& converter : _customConverters) {
      if (converter && key == converter->key()) {
        return id;
      }
      ++id;
    }
    return NONE_CONVERTER_ID;
  }

  ICustomConverter* getCustomConverter(ConverterId id) const override {
    if (isCustomConverterId(id)) {
      uint8_t i = converterIndex(id);
      if (i < std::size(_customConverters)) {
        return _customConverters[i];
      } else {
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  bool store(ConverterId id, ICustomConverter* converter) override {
    _dirty = true;
    return replaceCustomConverter(id, converter);
  }
  
  void remove(ConverterId id) override {
    replaceCustomConverter(id, nullptr);
    _dirty = true;
  }
  
  void removeAll() override {
    for (uint8_t i = 0; i < std::size(_customConverters); ++i) {
      replaceCustomConverter(customConverterId(i), nullptr);
    }
    _dirty = true;
  }
  
  void commit() override {
    if (_dirty) {
      persistConverters();
    }
  }
  
  void rollback() override {
    if (_dirty) {
      restoreConverters();
    }
  }
};

#endif
