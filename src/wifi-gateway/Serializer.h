#ifndef SERIALIZER_H_
#define SERIALIZER_H_

#include "src/iot-core/Utils.h"
#include "src/iot-core/JsonWriter.h"
#include "DataAccess.h"

namespace serializer {

template<typename JsonWriter>
void serialize(JsonWriter& writer, const DataEntry& entry, bool compact = false, bool numbersAsDecimals = false) {
  writer.objectOpen();
  writer.propertyRaw(F("id"), iot_core::convert<ValueId>::toString(entry.id, 10));
  writer.separator();
  if (entry.hasDefinition()) {
    if (!compact) {
      writer.propertyString(F("name"), entry.definition->name);
      writer.separator();
      writer.propertyString(F("accessMode"), valueAccessModeToString(entry.definition->accessMode));
      writer.separator();
    }
    if (entry.definition->unit != Unit::Unknown) {
      writer.propertyString(F("unit"), unitSymbol(entry.definition->unit));
      writer.separator();
    }
  }
  if (entry.lastUpdate.isSet()) {
    if (numbersAsDecimals) {
      writer.propertyString(F("rawValue"), getRawValueAsDecString(entry.rawValue));
    } else {
      writer.propertyString(F("rawValue"), getRawValueAsHexString(entry.rawValue));
    }
    writer.separator();
    if (entry.hasDefinition()) {
      writer.propertyRaw(F("value"), entry.definition->fromRaw(entry.rawValue));
      writer.separator();
    }
  }
  writer.propertyString(F("lastUpdate"), entry.lastUpdate.toString());
  writer.separator();
  writer.propertyString(F("source"), entry.source.toString());
  if (!compact) {
    writer.separator();
    writer.propertyRaw(F("subscribed"), iot_core::convert<bool>::toString(entry.subscribed));
    writer.separator();
    writer.propertyRaw(F("writable"), iot_core::convert<bool>::toString(entry.writable));
  }
  writer.objectClose();
}

template<typename JsonWriter>
void serialize(JsonWriter& writer, const ValueDefinition& definition) {
  writer.objectOpen();

  writer.propertyRaw(F("id"), iot_core::convert<ValueId>::toString(definition.id, 10));
  writer.separator();
  writer.propertyString(F("name"), definition.name);
  writer.separator();
  writer.propertyString(F("unit"), unitSymbol(definition.unit));
  writer.separator();
  writer.propertyString(F("accessMode"), valueAccessModeToString(definition.accessMode));
  writer.separator();
  writer.propertyString(F("source"), definition.source.toString());
  
  writer.objectClose();
}

};

#endif
