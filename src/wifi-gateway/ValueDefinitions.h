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
    case Unit::Unknown: return "Unknown";
    case Unit::None: return "None";
    case Unit::Percent: return "Percent";
    case Unit::RelativeHumidity: return "RelativeHumidity";
    case Unit::Hertz: return "Hertz";
    case Unit::Celsius: return "Celsius";
    case Unit::Kelvin: return "Kelvin";
    case Unit::Bar: return "Bar";
    case Unit::KelvinPerMinute: return "KelvinPerMinute";
    case Unit::LiterPerMinute: return "LiterPerMinute";
    case Unit::CubicmeterPerHour: return "CubicmeterPerHour";
    case Unit::Years: return "Years";
    case Unit::Year: return "Year";
    case Unit::Months: return "Months";
    case Unit::Month: return "Month";
    case Unit::Days: return "Days";
    case Unit::Day: return "Day";
    case Unit::Weekday: return "Weekday";
    case Unit::Hours: return "Hours";
    case Unit::ThousandHours: return "ThousandHours";
    case Unit::Hour: return "Hour";
    case Unit::Minutes: return "Minutes";
    case Unit::Minute: return "Minute";
    case Unit::Seconds: return "Seconds";
    case Unit::Second: return "Second";
    case Unit::Watt: return "Watt";
    case Unit::KiloWatt: return "KiloWatt";
    case Unit::WattHour: return "WattHour";
    case Unit::KiloWattHour: return "KiloWattHour";
    case Unit::MegaWattHour: return "MegaWattHour";
    case Unit::Ampere: return "Ampere";
    case Unit::Volt: return "Volt";
    default: return "?";
  }
}

Unit unitFromString(const toolbox::strref& unit) {
  if (unit == "Unknown") return Unit::Unknown;
  if (unit == "None") return Unit::None;
  if (unit == "Percent") return Unit::Percent;
  if (unit == "RelativeHumidity") return Unit::RelativeHumidity;
  if (unit == "Hertz") return Unit::Hertz;
  if (unit == "Celsius") return Unit::Celsius;
  if (unit == "Kelvin") return Unit::Kelvin;
  if (unit == "Bar") return Unit::Bar;
  if (unit == "KelvinPerMinute") return Unit::KelvinPerMinute;
  if (unit == "LiterPerMinute") return Unit::LiterPerMinute;
  if (unit == "CubicmeterPerHour") return Unit::CubicmeterPerHour;
  if (unit == "Years") return Unit::Years;
  if (unit == "Year") return Unit::Year;
  if (unit == "Months") return Unit::Months;
  if (unit == "Month") return Unit::Month;
  if (unit == "Days") return Unit::Days;
  if (unit == "Day") return Unit::Day;
  if (unit == "Weekday") return Unit::Weekday;
  if (unit == "Hours") return Unit::Hours;
  if (unit == "ThousandHours") return Unit::ThousandHours;
  if (unit == "Hour") return Unit::Hour;
  if (unit == "Minutes") return Unit::Minutes;
  if (unit == "Minute") return Unit::Minute;
  if (unit == "Seconds") return Unit::Seconds;
  if (unit == "Second") return Unit::Second;
  if (unit == "Watt") return Unit::Watt;
  if (unit == "KiloWatt") return Unit::KiloWatt;
  if (unit == "WattHour") return Unit::WattHour;
  if (unit == "KiloWattHour") return Unit::KiloWattHour;
  if (unit == "MegaWattHour") return Unit::MegaWattHour;
  if (unit == "Ampere") return Unit::Ampere;
  if (unit == "Volt") return Unit::Volt;
  return Unit::Unknown;
}

toolbox::strref unitSymbol(Unit unit) {
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
    case ValueAccessMode::None: return "None";
    case ValueAccessMode::Readable: return "Readable";
    case ValueAccessMode::Writable: return "Writable";
    case ValueAccessMode::WritableProtected: return "WritableProtected";
    case ValueAccessMode::WritableExtraProtected: return "WritableExtraProtected";
    default: return "";
  }
}

ValueAccessMode valueAccessModeFromString(const toolbox::strref& accessMode) {
  if (accessMode == "None") return ValueAccessMode::None;
  if (accessMode == "Readable") return ValueAccessMode::Readable;
  if (accessMode == "Writable") return ValueAccessMode::Writable;
  if (accessMode == "WritableProtected") return ValueAccessMode::WritableProtected;
  if (accessMode == "WritableExtraProtected") return ValueAccessMode::WritableExtraProtected;
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
        if (property.name() == "name" && property.type() == jsons::ValueType::String) {
          property.asString().get().copy(name, MAX_DEFINITION_NAME_LENGTH, true);
        } else if (property.name() == "unit" && property.type() == jsons::ValueType::String) {
          unit = unitFromString(property.asString().get());
        } else if (property.name() == "access" && property.type() == jsons::ValueType::String) {
          accessMode = valueAccessModeFromString(property.asString().get());
        } else if (property.name() == "interval" && property.type() == jsons::ValueType::Integer) {
          updateIntervalMs = property.asInteger().get();
        } else if (property.name() == "codec") {
          if (property.type() == jsons::ValueType::Integer) {
            codec = property.asInteger().get();
          } else if (property.type() == jsons::ValueType::String) {
            codec = repository.getCodecIdByKey(property.asString().get());
          } else {
            return false;
          }
        } else if (property.name() == "converter") {
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
    auto dir = LittleFS.openDir("/def/");
    while (dir.next()) {
      if (dir.isFile()) {
        ++stored;
        ValueId id = dir.fileName().toInt();
        auto file = dir.openFile("r");
        if (file) {
          toolbox::StreamInput input{file};
          auto reader = jsons::makeReader(input);
          auto json = reader.begin();
          ValueDefinition definition {};
          bool ok = definition.deserialize(json, _conversionRepo);
          reader.end();
          if (ok && !reader.failed()) {
            _definitions.insert(id, definition);
          } else {
            _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to load definition %u: %s"), id, reader.diagnostics().errorMessage.toString().c_str()));
          }
        }
      }
    }
    _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Restored definitions (%u of %u)"), stored, _definitions.size()));
    _dirty = false;
  }

  void persistDefinitions() {
    size_t stored = 0;
    LittleFS.rmdir("/def/");
    for (auto& entry : _definitions) {
      auto filename = toolbox::format("/def/%u", entry.key);
      auto file = LittleFS.open(filename, "w");
      if (file) {
        toolbox::PrintOutput output{file};
        auto writer = jsons::makeWriter(output);
        entry.value.serialize(writer, NullConversionRepository{});
        writer.end();
        file.close();
        if (writer.failed()) {
          _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to store definition %u."), entry.key));
          LittleFS.remove(filename);
        } else {
          ++stored;
        }
      } else {
        _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Failed to store definition %u."), entry.key));
      }
    }
    _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Stored definitions (%u of %u)"), stored, _definitions.size()));
    _dirty = stored != _definitions.size();
  }

public:
  DefinitionRepository(iot_core::ISystem& system, IConversionRepository& conversionRepo) :
    _logger(system.logger("def")),
    _system(system),
    _conversionRepo(conversionRepo)
  {}

  const char* name() const override {
    return "def";
  }

  bool configure(const char* /*name*/, const char* /*value*/) override {
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> /*writer*/) const override {
  }

  void setup(bool /*connected*/) override {
    restoreDefinitions();
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
  }
  
  void getDiagnostics(iot_core::IDiagnosticsCollector& collector) const override {
    collector.addValue("size", iot_core::convert<size_t>::toString(_definitions.size(), 10));
    collector.addValue("capacity", iot_core::convert<size_t>::toString(_definitions.capacity(), 10));
  }

  bool store(ValueId id, const ValueDefinition& definition) override {
    bool changed = _definitions.insert(id, definition);
    _dirty = _dirty || changed;
    return changed;
  }

  void remove(ValueId id) override {
    bool changed = _definitions.remove(id);
    _dirty = _dirty || changed;
  }

  void removeAll() override {
    _definitions.clear();
    _dirty = true;
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


#if false
const ValueDefinition DEFINITIONS[] = {
/*define< codec              , value converter                                >( value ID           , unit                    , name                                    , source ID                                              , access mode                             , update interval ), */  
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0003             , Unit::Celsius           , "DHW set temperature"                   , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0004             , Unit::Celsius           , "HC set temperature"                    , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0005             , Unit::Celsius           , "set room temperature day"              , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0008             , Unit::Celsius           , "set room temperature night"            , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Signed16BitCodec   , NumericValueConverter<0>                       >( 0x000B             , Unit::None              , "device ID"                             , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x000C             , Unit::Celsius           , "outside temperature"                   , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x000D             , Unit::Celsius           , "flow temperature"                      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x000E             , Unit::Celsius           , "DHW temperature"                       , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x000F             , Unit::Celsius           , "HC temperature"                        , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0011             , Unit::Celsius           , "room temperature"                      , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0012             , Unit::Celsius           , "set room temperature"                  , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0013             , Unit::Celsius           , "DHW set temperature day"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0014             , Unit::Celsius           , "evaporator temperature"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0016             , Unit::Celsius           , "return temperature"                    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x001A             , Unit::Celsius           , "collector temperature"                 , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0027             , Unit::Celsius           , "? HC max flow temperature"             , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0027             , Unit::Celsius           , "HC max curve set temperature"          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0028             , Unit::Celsius           , "? HC max flow temperature"             , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , BooleanConverter                               >( 0x0060             , Unit::None              , "defrost evaporator"                    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<-1>                      >( 0x0075             , Unit::RelativeHumidity  , "room humidity"                         , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x008E             , Unit::Percent           , "humidity hysteresis"                   , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x010C             , Unit::Hours             , "outside temperature damping"           , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x010E             , Unit::Percent           , "HC curve gradient slope"               , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x010F             , Unit::Percent           , "HC room influence"                     , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Unsigned8BitCodec  , IVUOperatingModeEnumConverter                  >( 0x0112             , Unit::None              , "operating mode"                        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned8BitCodec  , WeekdayEnumConverter                           >( 0x0121             , Unit::Weekday           , "current weekday"                       , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x0122             , Unit::Day               , "current day"                           , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x0123             , Unit::Month             , "current month"                         , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x0124             , Unit::Year              , "current year"                          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x0125             , Unit::Hour              , "current hour"                          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( 0x0126             , Unit::Minute            , "current minute"                        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , BitfieldConverter<IVUOperatingStatusBitfield>  >( 0x0176             , Unit::None              , "operating status"                      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x01DA             , Unit::LiterPerMinute    , "flow rate"                             , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0264             , Unit::Celsius           , "dew-point temperature"                 , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0265             , Unit::Celsius           , "hot gas temperature"                   , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0341             , Unit::Days              , "ventilation filter runtime"            , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x03AE             , Unit::WattHour          , "HRV heat quant. per day (% 1000)"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x03AF             , Unit::KiloWattHour      , "HRV heat quant. per day (/ 1000)"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x03B0             , Unit::KiloWattHour      , "HRV heat quant. total (% 1000)"        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x03B1             , Unit::MegaWattHour      , "HRV heat quant. total (/ 1000)"        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x056C             , Unit::None              , "ventilation level day"                 , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x056D             , Unit::None              , "ventilation level night"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x056F             , Unit::None              , "ventilation level standby"             , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0570             , Unit::None              , "ventilation level party"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0571             , Unit::Minutes           , "unscheduled vent. level 0 duration"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0572             , Unit::Minutes           , "unscheduled vent. level 1 duration"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0573             , Unit::Minutes           , "unscheduled vent. level 2 duration"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0574             , Unit::Minutes           , "unscheduled vent. level 3 duration"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0576             , Unit::CubicmeterPerHour , "air ventilation level 1"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0577             , Unit::CubicmeterPerHour , "air ventilation level 2"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0578             , Unit::CubicmeterPerHour , "air ventilation level 3"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0579             , Unit::CubicmeterPerHour , "air extraction level 1"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x057A             , Unit::CubicmeterPerHour , "air extraction level 2"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x057B             , Unit::CubicmeterPerHour , "air extraction level 3"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0596             , Unit::CubicmeterPerHour , "air ventilation current setpoint"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0597             , Unit::Hertz             , "air ventilation actual"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0598             , Unit::CubicmeterPerHour , "air extraction current setpoint"       , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0599             , Unit::Hertz             , "air extraction actual"                 , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x059A             , Unit::Percent           , "exhaust air current setpoint"          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x059B             , Unit::Hertz             , "exhaust air actual"                    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x059C             , Unit::Celsius           , "condenser temperature"                 , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x059D             , Unit::Celsius           , "HC flow temp. influence"               , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x059E             , Unit::Celsius           , "HC curve gradient base"                , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x05BB             , Unit::None              , "heating level"                         , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x05DD             , Unit::None              , "unscheduled ventilation level"         , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Writable               , 60000 ),
  define< Unsigned16BitCodec , NumericValueConverter<-1>                      >( 0x064A             , Unit::Bar               , "HC pressure"                           , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , ValvePositionEnumConverter                     >( 0x064B             , Unit::None              , "HC/DHW valve position"                 , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , BooleanConverter                               >( 0x0652             , Unit::None              , "HC/DHW valve: force to HC"             , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::WritableExtraProtected ),
  define< Unsigned16BitCodec , BooleanConverter                               >( 0x0653             , Unit::None              , "HC/DHW valve: force to DHW"            , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::WritableExtraProtected ),
  define< Unsigned16BitCodec , BooleanConverter                               >( 0x0654             , Unit::None              , "mixing circuit pump: force enable"     , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::WritableExtraProtected ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0694             , Unit::Celsius           , "air extraction temperature"            , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0698             , Unit::Percent           , "PWM solar pump"                        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0699             , Unit::Percent           , "PWM mixing circuit pump"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<0>                       >( 0x069A             , Unit::Percent           , "relative heating power"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x069B             , Unit::Percent           , "compressor current setpoint"           , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x069C             , Unit::Hertz             , "compressor freq. setpoint (unlimited)" , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x069D             , Unit::Hertz             , "compressor freq. setpoint (limited)"   , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x069E             , Unit::Hertz             , "compressor freq."                      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x069F             , Unit::Ampere            , "compressor current"                    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<1>                       >( 0x06A0             , Unit::Watt              , "compressor power"                      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x06A1             , Unit::Volt              , "compressor voltage"                    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<-1>                      >( 0x06A2             , Unit::Celsius           , "compressor temperature"                , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x06A3             , Unit::None              , "compressor fault"                      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x07A9             , Unit::Celsius           , "evaporator outlet temperature"         , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<-2>                      >( 0x080D             , Unit::Bar               , "low pressure (filtered)"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x091A             , Unit::WattHour          , "DHW el. cons. per day (% 1000)"        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x091B             , Unit::KiloWattHour      , "DHW el. cons. per day (/ 1000)"        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x091C             , Unit::KiloWattHour      , "DHW el. cons. total (% 1000)"          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x091D             , Unit::MegaWattHour      , "DHW el. cons. total (/ 1000)"          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x091E             , Unit::WattHour          , "heating el. cons. per day (% 1000)"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x091F             , Unit::KiloWattHour      , "heating el. cons. per day (/ 1000)"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0920             , Unit::KiloWattHour      , "heating el. cons. total (% 1000)"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0921             , Unit::MegaWattHour      , "heating el. cons. total (/ 1000)"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0924             , Unit::KiloWattHour      , "DHW EH heat quant. total (% 1000)"     , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0925             , Unit::MegaWattHour      , "DHW EH heat quant. total (/ 1000)"     , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0928             , Unit::KiloWattHour      , "heating EH heat quant. total (% 1000)" , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0929             , Unit::MegaWattHour      , "heating EH heat quant. total (/ 1000)" , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x092A             , Unit::WattHour          , "DHW heat quant. per day (% 1000)"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x092B             , Unit::KiloWattHour      , "DHW heat quant. per day (/ 1000)"      , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x092C             , Unit::KiloWattHour      , "DHW heat quant. total (% 1000)"        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x092D             , Unit::MegaWattHour      , "DHW heat quant. total (/ 1000)"        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x092E             , Unit::WattHour          , "heating heat quant. per day (% 1000)"  , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x092F             , Unit::KiloWattHour      , "heating heat quant. per day (/ 1000)"  , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0930             , Unit::KiloWattHour      , "heating heat quant. total (% 1000)"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0x0931             , Unit::MegaWattHour      , "heating heat quant. total (/ 1000)"    , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0xC0EF             , Unit::RelativeHumidity  , "air extraction humidity"               , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0xC0F4             , Unit::Hours             , "compressor starts (% 1000)"            , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned16BitCodec , NumericValueConverter<0>                       >( 0xC0F5             , Unit::ThousandHours     , "compressor starts (/ 1000)"            , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0xC0F6             , Unit::Celsius           , "air extraction dew point"              , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),

/*
  { 0x0116, ValueFormat::TenthDegree, "Sommerbetrieb_Heizgrundeinstellung" },
  { 0x011B, ValueFormat::DayByte, "Ferienbeginn_Tag" },
  { 0x011C, ValueFormat::MonthByte, "Ferienbeginn_Monat" },
  { 0x011D, ValueFormat::YearByte, "Ferienbeginn_Jahr" },
  { 0x011E, ValueFormat::DayByte, "Ferienende_Tag" },
  { 0x011F, ValueFormat::MonthByte, "Ferienende_Monat" },
  { 0x0120, ValueFormat::YearByte, "Ferienende_Jahr" },

  { 0x0129, ValueFormat::TenthDegree, "Heizen_Soll_Handbetrieb" },
  { 0x012B, ValueFormat::TenthDegree, "Heizkurfe_Sollwert_Min" },
  { 0x013D, ValueFormat::TenthDegree, "Heizen_RaumtemperaturBereitschaft" },
  { 0x0140, ValueFormat::TenthKelvin, "Warmwasser_Hysterese" },
  { 0x0162, ValueFormat::KelvinPerMinute, "Integralanteil_Heizgrundeinstellung" },
  { 0x0180, ValueFormat::Hour, "Maximaldauer_Warmwasser_Erzeugung" },
  { 0x02F5, ValueFormat::Boolean, "Solar_Kollektorschutz" },
  { 0x02F6, ValueFormat::TenthDegree, "Solar_Kollektorgrenztemperatur" },
  { 0x03AE, ValueFormat::WattHour, "Wärmemenge_Wärmerückgewinnung_Tag_Wh" },
  { 0x03AF, ValueFormat::KiloWattHour, "Wärmemenge_Wärmerückgewinnung_Tag_KWh" },
  { 0x03B0, ValueFormat::KiloWattHour, "Wärmemenge_Wärmerückgewinnung_Summe_KWh" },
  { 0x03B1, ValueFormat::MegaWattHour, "Wärmemenge_Wärmerückgewinnung_Summe_MWh" },
  { 0x033B, ValueFormat::Boolean, "Lüftung_Filter_Reset" },
  { 0x02B8, ValueFormat::TenthDegree, "Solar_Kollektorschutztemperatur" },
  { 0x02BB, ValueFormat::TenthDegree, "Solar_Kollektorsperrtemperatur" },
  { 0x0569, ValueFormat::TenthDegree, "Kühlen_Raumtemperatur_Tag" },
  { 0x056A, ValueFormat::TenthDegree, "Kühlen_RaumtemperaturBereitschaft" },
  { 0x056B, ValueFormat::TenthDegree, "Kühlen_Raumtemperatur_Nacht" },
  { 0x056C, ValueFormat::None, "Lüftung_Stufe_Tag" },
  { 0x056D, ValueFormat::None, "Lüftung_Stufe_Nacht" },
  { 0x056F, ValueFormat::None, "Lüftung_StufeBereitschaft" },
  { 0x0570, ValueFormat::None, "Lüftung_Stufe_Party" },
  { 0x0571, ValueFormat::Minute, "Lüftung_Außerordentlich_Stufe_0" },
  { 0x0572, ValueFormat::Minute, "Lüftung_Außerordentlich_Stufe_1" },
  { 0x0573, ValueFormat::Minute, "Lüftung_Außerordentlich_Stufe_2" },
  { 0x0574, ValueFormat::Minute, "Lüftung_Außerordentlich_Stufe_3" },
  { 0x0575, ValueFormat::None, "Passivkühlung" },
  { 0x0576, ValueFormat::CubicmeterPerHour, "Lüfterstufe_Zuluft_1" },
  { 0x0577, ValueFormat::CubicmeterPerHour, "Lüfterstufe_Zuluft_2" },
  { 0x0578, ValueFormat::CubicmeterPerHour, "Lüfterstufe_Zuluft_3" },
  { 0x0579, ValueFormat::CubicmeterPerHour, "Lüfterstufe_Abluft_1" },
  { 0x057A, ValueFormat::CubicmeterPerHour, "Lüfterstufe_Abluft_2" },
  { 0x057B, ValueFormat::CubicmeterPerHour, "Lüfterstufe_Abluft_3" },
  { 0x057C, ValueFormat::Boolean, "Ofen_Kamin" },
  { 0x057D, ValueFormat::Minute, "LL_Wärmetauscher_Max_Abtaudauer" },
  { 0x057E, ValueFormat::Percent, "LL_Wärmetauscher_Abtaubeginnschwelle" },
  { 0x057F, ValueFormat::Percent, "LL_Wärmetauscher_Drehzahl_Filter" },
  { 0x0580, ValueFormat::TenthDegree, "Warmwasser_Solltemperatur_Handbetrieb" },
  { 0x0581, ValueFormat::TenthDegree, "Warmwasser_SolltemperaturBereitschaft" },
  { 0x0582, ValueFormat::TenthDegree, "Kühlen_Temperatur_Heizkreis" },
  { 0x0583, ValueFormat::TenthKelvin, "Kühlen_Vorlauftemperatur_Hysterese_Unbekannt" },
  { 0x0584, ValueFormat::TenthKelvin, "Kühlen_Raumtemperatur_Hysterese" },
  { 0x0585, ValueFormat::Day, "Antilegionellen" },
  { 0x0586, ValueFormat::TenthDegree, "Warmwasser_Temperatur_Legionellen" },
  { 0x0588, ValueFormat::Minute, "Zeitsperre_Nacherwärmung" },
  { 0x0589, ValueFormat::TenthDegree, "Temperaturfreigabe_Nacherwärmung" },
  { 0x058A, ValueFormat::None, "Nacherwärmung_Stufe_Warmwasser" },
  { 0x058B, ValueFormat::Boolean, "Warmwasser_Pufferbetrieb" },
  { 0x058C, ValueFormat::TenthDegree, "Warmwasser_Max_Vorlauftemperatur" },
  { 0x058D, ValueFormat::Boolean, "Warmwasser_ECO_Modus" },
  { 0x058F, ValueFormat::TenthKelvin, "Solar_Hysterese" },

  { 0x059D, ValueFormat::TenthDegree, "Heizkurve_Anteil_Vorlauf" },
  { 0x059E, ValueFormat::TenthDegree, "Heizkurve_Fußpunkt" },
  { 0x059F, ValueFormat::None, "Heizgrundeinstellung_Nacherwärmung_Maximale_Stufe" },
  { 0x05A0, ValueFormat::Minute, "Heizgrundeinstellung_Zeitsperre_Nacherwärmung" },
  { 0x05A1, ValueFormat::HundrethKiloWatt, "Heizgrundeinstellung_Heizleistung_Nacherwärmung_1" },
  { 0x05A2, ValueFormat::TenthKelvin, "Heizgrundeinstellung_Hysterese_Sommerbetrieb" },
  { 0x05A3, ValueFormat::TenthDegree, "Heizgrundeinstellung_Korrektur_Außentemperatur" },
  
  { 0x05BC, ValueFormat::Boolean, "LL_Wärmetauscher_Abtauen" },
  { 0x05BF, ValueFormat::TenthDegree, "Warmwasser_Solltemperatur_Nacht" },
  { 0x05C0, ValueFormat::TenthKelvin, "Hysterese_1" },
  { 0x05C1, ValueFormat::TenthKelvin, "Hysterese_2" },
  { 0x05C2, ValueFormat::TenthKelvin, "Hysterese_3" },
  { 0x05C3, ValueFormat::TenthKelvin, "Hysterese_4" },
  { 0x05C4, ValueFormat::TenthKelvin, "Hysterese_5" },
  { 0x05C5, ValueFormat::None, "Hysterese_Asymmetrie" },
  { 0x05D3, ValueFormat::None, "Ferienbeginn_Uhrzeit_Unbekannt" },
  { 0x05D4, ValueFormat::None, "Ferienende_Uhrzeit_Unbekannt" },
  { 0x05DB, ValueFormat::Boolean, "Passivkühlung_Fortluft" },
  { 0x0611, ValueFormat::None, "Heizgrundeinstellung_Unterdrücke_Temperaturmessung" },
  { 0x0612, ValueFormat::None, "Lüftung_Stufe_Handbetrieb" },
  { 0x0640, ValueFormat::WattHour, "Wärmemenge_Solar_Heizung_Tag_Wh" },
  { 0x0641, ValueFormat::KiloWattHour, "Wärmemenge_Solar_Heizung_Tag_KWh" },
  { 0x0642, ValueFormat::KiloWattHour, "Wärmemenge_Solar_Heizung_Summe_KWh" },
  { 0x0643, ValueFormat::MegaWattHour, "Wärmemenge_Solar_Heizung_Summe_MWh" },
  { 0x0644, ValueFormat::WattHour, "Wärmemenge_Solar_Warmwasser_Tag_Wh" },
  { 0x0645, ValueFormat::KiloWattHour, "Wärmemenge_Solar_Warmwasser_Tag_KWh" },
  { 0x0646, ValueFormat::KiloWattHour, "Wärmemenge_Solar_Warmwasser_Summe_KWh" },
  { 0x0647, ValueFormat::MegaWattHour, "Wärmemenge_Solar_Warmwasser_Summe_MWh" },
  { 0x0648, ValueFormat::KiloWattHour, "Wärmemenge_Kühlen_Summe_KWh" },
  { 0x0649, ValueFormat::MegaWattHour, "Wärmemenge_Kühlen_Summe_MWh" },
  { 0x064C, ValueFormat::None, "Solar_Kühlzeit_Von_Unbekannt" },
  { 0x064D, ValueFormat::None, "Solar_KühlzeitBis_Unbekannt" },
  { 0x064F, ValueFormat::Minute, "Feuchte_Maskierzeit" },
  { 0x0650, ValueFormat::Percent, "Feuchte_Schwellwert" },
  { 0x06A4, ValueFormat::Percent, "Lüftung_Leistungsreduktion" },
  { 0x06A5, ValueFormat::Percent, "Lüftung_Leistungserhöhung" },
  
  { 0x09D2, ValueFormat::Percent, "Feuchte_Soll_Minimum" },
  { 0x09D3, ValueFormat::Percent, "Feuchte_Soll_Maximum" },
  { 0x11AC, ValueFormat::TenthDegree, "HeizgrundeinstellungBivalenzpunkt" }*/
};

const ValueDefinition& getDefinition(ValueId id) {
  size_t lower = 0u;
  size_t upper = std::size(DEFINITIONS) - 1u;
  
  while (lower != upper) {
    size_t i = (lower + upper) / 2u;
    if (id > DEFINITIONS[i].id) {
      lower = i + 1u;
    } else {
      upper = i;
    }
  }

  if (id == DEFINITIONS[lower].id) {
    return DEFINITIONS[lower];
  } else {
    return UNKNOWN_VALUE_DEFINITION;
  }
}

bool hasDefinition(ValueId id) {
  return getDefinition(id).isUnknown();
}

#endif

#endif
