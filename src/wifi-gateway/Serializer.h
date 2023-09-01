#ifndef SERIALIZER_H_
#define SERIALIZER_H_

#include "src/iot-core/Utils.h"
#include "src/iot-core/JsonWriter.h"
#include "DataAccess.h"

namespace serializer {

template<typename JsonWriter>
void serialize(JsonWriter& writer, const DataEntry& entry, bool compact = false, bool numbersAsDecimals = false) {
  writer.openObject();
  
  writer.propertyPlain(F("id"), iot_core::convert<ValueId>::toString(entry.id, 10));
  if (entry.hasDefinition()) {
    if (!compact) {
      writer.propertyString(F("name"), entry.definition->name);
      writer.propertyString(F("accessMode"), valueAccessModeToString(entry.definition->accessMode));
    }
    if (entry.definition->unit != Unit::Unknown) {
      writer.propertyString(F("unit"), unitSymbol(entry.definition->unit));
    }
  }
  if (entry.lastUpdate.isSet()) {
    if (numbersAsDecimals) {
      writer.propertyString(F("rawValue"), getRawValueAsDecString(entry.rawValue));
    } else {
      writer.propertyString(F("rawValue"), getRawValueAsHexString(entry.rawValue));
    }
    if (entry.hasDefinition()) {
      writer.propertyPlain(F("value"), entry.definition->fromRaw(entry.rawValue));
    }
  }
  writer.propertyString(F("lastUpdate"), entry.lastUpdate.toString());
  writer.propertyString(F("source"), entry.source.toString());
  if (!compact) {
    writer.propertyPlain(F("subscribed"), iot_core::convert<bool>::toString(entry.subscribed));
    writer.propertyPlain(F("writable"), iot_core::convert<bool>::toString(entry.writable));
  }
  writer.close();
}

template<typename JsonWriter>
void serialize(JsonWriter& writer, const ValueDefinition& definition) {
  writer.openObject();

  writer.propertyPlain(F("id"), iot_core::convert<ValueId>::toString(definition.id, 10));
  writer.propertyString(F("name"), definition.name);
  writer.propertyString(F("unit"), unitSymbol(definition.unit));
  writer.propertyString(F("accessMode"), valueAccessModeToString(definition.accessMode));
  writer.propertyString(F("source"), definition.source.toString());
  
  writer.close();
}

};

#endif
