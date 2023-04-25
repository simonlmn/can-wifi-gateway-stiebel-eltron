#pragma once

#ifdef TEST_ENV
#include <iterator>
#endif

#include "StiebelEltronTypes.h"
#include "ValueConversion.h"

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

const char* getValueAccessModeString(ValueAccessMode accessMode) {
  switch (accessMode) {
    case ValueAccessMode::None: return "None";
    case ValueAccessMode::Readable: return "Readable";
    case ValueAccessMode::Writable: return "Writable";
    case ValueAccessMode::WritableProtected: return "WritableProtected";
    case ValueAccessMode::WritableExtraProtected: return "WritableExtraProtected";
    default: return "";
  }
}

ValueAccessMode getValueAccessModeFromString(const char* accessMode) {
  if (strcmp(accessMode, "None") == 0) return ValueAccessMode::None;
  if (strcmp(accessMode, "Readable") == 0) return ValueAccessMode::Readable;
  if (strcmp(accessMode, "Writable") == 0) return ValueAccessMode::Writable;
  if (strcmp(accessMode, "WritableProtected") == 0) return ValueAccessMode::WritableProtected;
  if (strcmp(accessMode, "WritableExtraProtected") == 0) return ValueAccessMode::WritableExtraProtected;
  
  return ValueAccessMode::None;
}

struct ValueDefinition {
  typedef const char* (*FromRawFunc)(uint16_t);
  typedef uint32_t (*ToRawFunc)(const char*, const char*);
  typedef const char* (*DescriptionFunc)();
  
  ValueId id;
  Unit unit;
  const char* name;
  DeviceId source;
  ValueAccessMode accessMode;
  unsigned long updateIntervalMs;
  const char* (*fromRaw)(uint16_t);
  uint32_t (*toRaw)(const char*, const char*);
  const char* (*description)();

  bool isUnknown() const {
    return id == UNKNOWN_VALUE_ID;
  }

  ValueDefinition(ValueId id, Unit unit, const char* name, DeviceId source, ValueAccessMode accessMode, unsigned long updateIntervalMs, FromRawFunc fromRaw, ToRawFunc toRaw, DescriptionFunc description)
    : id(id), unit(unit), name(name), source(source), accessMode(accessMode), updateIntervalMs(updateIntervalMs), fromRaw(fromRaw), toRaw(toRaw), description(description) {}

  ValueDefinition()
    : id(UNKNOWN_VALUE_ID), unit(Unit::Unknown), name(""), source(), accessMode(ValueAccessMode::None), updateIntervalMs(0), fromRaw(ValueConversion<Unsigned16BitCodec, ValueConverter>::fromRaw), toRaw(ValueConversion<Unsigned16BitCodec, ValueConverter>::toRaw), description(ValueConverter::description) {}
};

template<typename Codec, typename Converter>
ValueDefinition define(ValueId id, Unit unit, const char* name, DeviceId source, ValueAccessMode accessMode = ValueAccessMode::Readable, unsigned long updateIntervalMs = 0) {
  return ValueDefinition(id, unit, name, source, accessMode, updateIntervalMs, ValueConversion<Codec, Converter>::fromRaw, ValueConversion<Codec, Converter>::toRaw, Converter::description);
}

const ValueDefinition UNKNOWN_VALUE_DEFINITION;

const ValueId DATETIME_DAY_ID = 0x0122;
const ValueId DATETIME_MONTH_ID = 0x0123;
const ValueId DATETIME_YEAR_ID = 0x0124;
const ValueId DATETIME_HOUR_ID = 0x0125;
const ValueId DATETIME_MINUTE_ID = 0x0126;

const ValueDefinition DEFINITIONS[] = {
/*define< codec              , value converter                                >( value ID           , unit                    , name                                    , source ID                                              , access mode                             , update interval ), */  
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0003             , Unit::Celsius           , "DHW set temperature"                   , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0004             , Unit::Celsius           , "HC set temperature"                    , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Readable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0005             , Unit::Celsius           , "set room temperature day"              , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
  define< Signed16BitCodec   , NumericValueConverter<-1>                      >( 0x0008             , Unit::Celsius           , "set room temperature night"            , DeviceId{DeviceType::HeatingCircuit, DEVICE_ADDR_ANY } , ValueAccessMode::Writable               ),
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
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( DATETIME_DAY_ID    , Unit::Day               , "current day"                           , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( DATETIME_MONTH_ID  , Unit::Month             , "current month"                         , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( DATETIME_YEAR_ID   , Unit::Year              , "current year"                          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( DATETIME_HOUR_ID   , Unit::Hour              , "current hour"                          , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
  define< Unsigned8BitCodec  , NumericValueConverter<0>                       >( DATETIME_MINUTE_ID , Unit::Minute            , "current minute"                        , DeviceId{DeviceType::System        , 0u              } , ValueAccessMode::Readable               ),
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
