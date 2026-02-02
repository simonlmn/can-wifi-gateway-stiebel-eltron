#ifndef VALUEDEFINITIONS_H_
#define VALUEDEFINITIONS_H_

#include "StiebelEltronTypes.h"
#include "ValueConversion.h"
#include <toolbox/FixedCapacityMap.h>
#include <toolbox/Repository.h>
#include <iot_core/Interfaces.h>
#include <LittleFS.h>

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

toolbox::strref unitToString(Unit unit) {
  switch (unit) {
    case Unit::Unknown: return F("Unknown");
    case Unit::None: return F("None");
    case Unit::Percent: return F("Percent");
    case Unit::RelativeHumidity: return F("RelativeHumidity");
    case Unit::Hertz: return F("Hertz");
    case Unit::Celsius: return F("Celsius");
    case Unit::Kelvin: return F("Kelvin");
    case Unit::Bar: return F("Bar");
    case Unit::KelvinPerMinute: return F("KelvinPerMinute");
    case Unit::LiterPerMinute: return F("LiterPerMinute");
    case Unit::CubicmeterPerHour: return F("CubicmeterPerHour");
    case Unit::Years: return F("Years");
    case Unit::Year: return F("Year");
    case Unit::Months: return F("Months");
    case Unit::Month: return F("Month");
    case Unit::Days: return F("Days");
    case Unit::Day: return F("Day");
    case Unit::Weekday: return F("Weekday");
    case Unit::Hours: return F("Hours");
    case Unit::ThousandHours: return F("ThousandHours");
    case Unit::Hour: return F("Hour");
    case Unit::Minutes: return F("Minutes");
    case Unit::Minute: return F("Minute");
    case Unit::Seconds: return F("Seconds");
    case Unit::Second: return F("Second");
    case Unit::Watt: return F("Watt");
    case Unit::KiloWatt: return F("KiloWatt");
    case Unit::WattHour: return F("WattHour");
    case Unit::KiloWattHour: return F("KiloWattHour");
    case Unit::MegaWattHour: return F("MegaWattHour");
    case Unit::Ampere: return F("Ampere");
    case Unit::Volt: return F("Volt");
    default: return F("?");
  }
}

Unit unitFromString(const toolbox::strref& unit) {
  if (unit == F("Unknown")) return Unit::Unknown;
  if (unit == F("None")) return Unit::None;
  if (unit == F("Percent")) return Unit::Percent;
  if (unit == F("RelativeHumidity")) return Unit::RelativeHumidity;
  if (unit == F("Hertz")) return Unit::Hertz;
  if (unit == F("Celsius")) return Unit::Celsius;
  if (unit == F("Kelvin")) return Unit::Kelvin;
  if (unit == F("Bar")) return Unit::Bar;
  if (unit == F("KelvinPerMinute")) return Unit::KelvinPerMinute;
  if (unit == F("LiterPerMinute")) return Unit::LiterPerMinute;
  if (unit == F("CubicmeterPerHour")) return Unit::CubicmeterPerHour;
  if (unit == F("Years")) return Unit::Years;
  if (unit == F("Year")) return Unit::Year;
  if (unit == F("Months")) return Unit::Months;
  if (unit == F("Month")) return Unit::Month;
  if (unit == F("Days")) return Unit::Days;
  if (unit == F("Day")) return Unit::Day;
  if (unit == F("Weekday")) return Unit::Weekday;
  if (unit == F("Hours")) return Unit::Hours;
  if (unit == F("ThousandHours")) return Unit::ThousandHours;
  if (unit == F("Hour")) return Unit::Hour;
  if (unit == F("Minutes")) return Unit::Minutes;
  if (unit == F("Minute")) return Unit::Minute;
  if (unit == F("Seconds")) return Unit::Seconds;
  if (unit == F("Second")) return Unit::Second;
  if (unit == F("Watt")) return Unit::Watt;
  if (unit == F("KiloWatt")) return Unit::KiloWatt;
  if (unit == F("WattHour")) return Unit::WattHour;
  if (unit == F("KiloWattHour")) return Unit::KiloWattHour;
  if (unit == F("MegaWattHour")) return Unit::MegaWattHour;
  if (unit == F("Ampere")) return Unit::Ampere;
  if (unit == F("Volt")) return Unit::Volt;
  return Unit::Unknown;
}

toolbox::strref unitSymbol(Unit unit) {
  switch (unit) {
    case Unit::Unknown: return F("?"); 
    case Unit::None: return F("");
    case Unit::Percent: return F("%");
    case Unit::RelativeHumidity: return F("% RH");
    case Unit::Hertz: return F("Hz");
    case Unit::Celsius: return F("°C");
    case Unit::Kelvin: return F("K");
    case Unit::Bar: return F("bar");
    case Unit::KelvinPerMinute: return F("K/min");
    case Unit::LiterPerMinute: return F("l/min");
    case Unit::CubicmeterPerHour: return F("m^3/h");
    case Unit::Years: return F("years");
    case Unit::Year: return F("");
    case Unit::Months: return F("months");
    case Unit::Month: return F("");
    case Unit::Days: return F("days");
    case Unit::Day: return F("");
    case Unit::Weekday: return F("");
    case Unit::Hours: return F("h");
    case Unit::ThousandHours: return F("1000h");
    case Unit::Hour: return F("");
    case Unit::Minutes: return F("min");
    case Unit::Minute: return F("");
    case Unit::Seconds: return F("s");
    case Unit::Second: return F("");
    case Unit::Watt: return F("W");
    case Unit::KiloWatt: return F("kW");
    case Unit::WattHour: return F("Wh");
    case Unit::KiloWattHour: return F("kWh");
    case Unit::MegaWattHour: return F("MWh");
    case Unit::Ampere: return F("A");
    case Unit::Volt: return F("V");
    default: return F("?");
  }
}

enum struct ValueAccessMode : uint8_t {
  /*
   * Value is not accessible (i.e. is not available at all).
   */
  None = 0,
  /*
   * Value is read-only (i.e. is a sensor reading or other computed/derived value).
   */
  Readable = 1,
  /*
   * Value can be read and written (i.e. is a parameter).
   */
  Writable = 2,
  /*
   * Value can be read and written, but should be changed more
   * carefully (it requires the code 1000 on the built-in display).
   */
  WritableProtected = 3,
  /*
   * Value can be read and written, but extra care must be taken
   * as the device can be damaged (it requires the "secret" sevice
   * code 7777 on the built-in display).
   */
  WritableExtraProtected = 4, 
};

toolbox::strref valueAccessModeToString(ValueAccessMode accessMode) {
  switch (accessMode) {
    case ValueAccessMode::None: return F("None");
    case ValueAccessMode::Readable: return F("Readable");
    case ValueAccessMode::Writable: return F("Writable");
    case ValueAccessMode::WritableProtected: return F("WritableProtected");
    case ValueAccessMode::WritableExtraProtected: return F("WritableExtraProtected");
    default: return F("");
  }
}

ValueAccessMode valueAccessModeFromString(const toolbox::strref& accessMode) {
  if (accessMode == F("None")) return ValueAccessMode::None;
  if (accessMode == F("Readable")) return ValueAccessMode::Readable;
  if (accessMode == F("Writable")) return ValueAccessMode::Writable;
  if (accessMode == F("WritableProtected")) return ValueAccessMode::WritableProtected;
  if (accessMode == F("WritableExtraProtected")) return ValueAccessMode::WritableExtraProtected;
  return ValueAccessMode::None;
}

static const size_t MAX_DEFINITION_NAME_LENGTH = 32u;

struct __attribute__((__packed__)) ValueDefinition final {
  static const ValueDefinition UNDEFINED;

  Unit unit = Unit::None;
  ValueAccessMode accessMode = ValueAccessMode::None;
  uint8_t codec = NONE_CODEC_ID;
  uint8_t converter = NONE_CONVERTER_ID;
  uint32_t updateIntervalMs = 30000u;
  char name[MAX_DEFINITION_NAME_LENGTH] = {'\0'};

  ValueDefinition() {};
  ValueDefinition(const toolbox::strref& name, Unit unit, uint8_t codec, uint8_t converter, ValueAccessMode mode, uint16_t updateIntervalMs) :
    unit(unit),
    accessMode(mode),
    codec(codec),
    converter(converter),
    updateIntervalMs(updateIntervalMs)
  {
    name.copy(this->name, MAX_DEFINITION_NAME_LENGTH, true);
  }

  bool operator==(const ValueDefinition& other) const {
    return unit == other.unit
        && accessMode == other.accessMode
        && codec == other.codec
        && converter == other.converter
        && updateIntervalMs == other.updateIntervalMs
        && strncmp(name, other.name, MAX_DEFINITION_NAME_LENGTH) == 0;
  }

  bool isUndefined() const {
    return *this == UNDEFINED;
  }

  void serialize(jsons::IWriter& output, const IConversionRepository& repository) const {
    output.openObject();
    output.property(F("name")).string(name);
    output.property(F("unit")).string(unitToString(unit));
    output.property(F("access")).string(valueAccessModeToString(accessMode));
    output.property(F("interval")).number(updateIntervalMs);
    output.property(F("codec"));
    auto codecObject = repository.getCodec(codec);
    if (codecObject) {
      output.string(codecObject->key());
    } else {
      output.number(codec);
    }
    output.property(F("converter"));
    auto converterObject = repository.getConverter(converter);
    if (converterObject) {
      output.string(converterObject->key());
    } else {
      output.number(converter);
    }
    output.close();
  }

  bool deserialize(jsons::Value& input, const IConversionRepository& repository) {
    auto object = input.asObject();
    if (object.valid()) {
      for (auto& property : object) {
        if (property.name() == F("name") && property.type() == jsons::ValueType::String) {
          property.asString().get().copy(name, MAX_DEFINITION_NAME_LENGTH, true);
        } else if (property.name() == F("unit") && property.type() == jsons::ValueType::String) {
          unit = unitFromString(property.asString().get());
        } else if (property.name() == F("access") && property.type() == jsons::ValueType::String) {
          accessMode = valueAccessModeFromString(property.asString().get());
        } else if (property.name() == F("interval") && property.type() == jsons::ValueType::Integer) {
          updateIntervalMs = property.asInteger().get();
        } else if (property.name() == F("codec")) {
          if (property.type() == jsons::ValueType::Integer) {
            codec = property.asInteger().get();
          } else if (property.type() == jsons::ValueType::String) {
            codec = repository.getCodecIdByKey(property.asString().get());
          } else {
            return false;
          }
        } else if (property.name() == F("converter")) {
          if (property.type() == jsons::ValueType::Integer) {
            converter = property.asInteger().get();
          } else if (property.type() == jsons::ValueType::String) {
            converter = repository.getConverterIdByKey(property.asString().get());
          } else {
            return false;
          }
        } else {
          return false;
        }
      }
    }
    return true;
  }
};
const ValueDefinition ValueDefinition::UNDEFINED {};

class IDefinitionRepository : public toolbox::IRepository {
public:
  virtual bool store(ValueId id, const ValueDefinition& definition) = 0;
  virtual void remove(ValueId id) = 0;
  virtual void removeAll() = 0;
  virtual const ValueDefinition& get(ValueId id) const = 0;
  virtual toolbox::Iterable<const toolbox::Mapping<ValueId, ValueDefinition>> all() const = 0;
};

class DefinitionRepository final : public IDefinitionRepository, public iot_core::IApplicationComponent {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  IConversionRepository& _conversionRepo;
  toolbox::FixedCapacityMap<ValueId, ValueDefinition, 200u> _definitions {};
  bool _dirty = false;

  void restoreDefinitions() {
    size_t stored = 0;
    _definitions.clear();
    auto file = LittleFS.open("/def/definitions.json", "r");
    if (file) {
      toolbox::StreamInput input{file};
      auto reader = jsons::makeReader(input);
      auto json = reader.begin();
      for (auto& property : json.asObject()) {
        ++stored;
        toolbox::Maybe<ValueId> valueId = toolbox::convert<ValueId>::fromString(property.name(), nullptr, 10);
        ValueDefinition definition {};
        if (valueId.available() && definition.deserialize(property, _conversionRepo) && !reader.failed()) {
          _definitions.insert(valueId.get(), definition);
        } else {
          _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to load definition %u: %s"), valueId, reader.diagnostics().errorMessage.cstr()));
        }
      }
    }
    _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Restored definitions (%u of %u)"), _definitions.size(), stored));
    _dirty = false;
  }

  void persistDefinitions() {
    size_t stored = 0;
    auto file = LittleFS.open("/def/definitions.json", "w");
    if (file) {
      toolbox::PrintOutput output{file};
      auto writer = jsons::makeWriter(output);
      writer.openObject();
      for (auto& entry : _definitions) {
        writer.property(toolbox::convert<long>::toString(entry.key(), 10));
        entry.value().serialize(writer, NullConversionRepository{});
      }
      writer.end();
      file.close();
      if (writer.failed()) {
        _logger.log(iot_core::LogLevel::Warning, F("Failed to store definitions."));
        LittleFS.remove("/def/definitions.json");
      } else {
        _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Stored %u definitions."), _definitions.size()));
        _dirty = false;
      }
    } else {
      _logger.log(iot_core::LogLevel::Error, F("Failed to open file to store definitions."));
    }
  }

public:
  DefinitionRepository(iot_core::ISystem& system, IConversionRepository& conversionRepo) :
    _logger(system.logger(F("def"))),
    _system(system),
    _conversionRepo(conversionRepo)
  {}

  toolbox::strref name() const override {
    return F("def");
  }

  bool configure(const toolbox::strref& /*name*/, const toolbox::strref& /*value*/) override {
    return false;
  }

  void getConfig(iot_core::ConfigWriter writer) const override {
  }

  void setup(bool /*connected*/) override {
    restoreDefinitions();
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& collector) const override {
    collector.addValue(F("size"), toolbox::convert<size_t>::toString(_definitions.size(), 10));
    collector.addValue(F("capacity"), toolbox::convert<size_t>::toString(_definitions.capacity(), 10));
  }

  bool store(ValueId id, const ValueDefinition& definition) override {
    bool changed = _definitions.insert(id, definition);
    _dirty = _dirty || changed;
    return changed;
  }

  void remove(ValueId id) override {
    auto definition = _definitions.find(id);
    if (definition) {
      *definition = ValueDefinition::UNDEFINED;
      _dirty = true;
    }
  }

  void removeAll() override {
    for (auto& entry : _definitions) {
      if (!entry.value().isUndefined()) {
        entry.value() = ValueDefinition::UNDEFINED;
        _dirty = true;
      }
    }
  }

  void commit() override {
    if (_dirty) {
      persistDefinitions();
    }
  }

  void rollback() override {
    if (_dirty) {
      restoreDefinitions();
    }
  }

  const ValueDefinition& get(ValueId id) const override {
    auto definition = _definitions.find(id);
    if (definition) {
      return *definition;
    } else {
      return ValueDefinition::UNDEFINED;
    }
  }

  toolbox::Iterable<const toolbox::Mapping<ValueId, ValueDefinition>> all() const override {
    return {_definitions.begin(), _definitions.end()};
  }
};

class IConversionService {
public:
  virtual void toJson(jsons::IWriter& output, ValueId id, uint16_t rawValue) const = 0;
  virtual toolbox::Maybe<uint16_t> fromJson(jsons::Value& input, ValueId id) const = 0;
  virtual Conversion getConversion(ValueId id) const = 0;
};

class ConversionService final : public IConversionService {
  IConversionRepository& _conversions;
  IDefinitionRepository& _definitions;

public:
  ConversionService(IConversionRepository& conversions, IDefinitionRepository& definitions) :
    _conversions(conversions),
    _definitions(definitions)
  {}

  void toJson(jsons::IWriter& output, ValueId id, uint16_t rawValue) const override {
    getConversion(id).toJson(rawValue, output);
  }
  
  toolbox::Maybe<uint16_t> fromJson(jsons::Value& input, ValueId id) const override {
    return getConversion(id).fromJson(input);
  }

  Conversion getConversion(ValueId id) const override {
    auto& definition = _definitions.get(id);
    return {
      _conversions.getCodec(definition.codec),
      _conversions.getConverter(definition.converter)
    };
  }
};

#endif
