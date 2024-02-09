#ifndef SERIALIZER_H_
#define SERIALIZER_H_

#include <iot_core/Utils.h>
#include <jsons/Writer.h>
#include "DataAccess.h"

namespace serializer {

void serialize(jsons::IWriter& writer, const IConversionService& conversion, const IDefinitionRepository& definitions, const DataEntry& entry, bool compact = false, bool numbersAsDecimals = false) {
  auto& definition = definitions.get(entry.id);

  writer.openObject();
  
  writer.property(F("id")).number(entry.id);
  if (!definition.isUndefined()) {
    if (!compact) {
      writer.property(F("name")).string(definition.name);
      writer.property(F("accessMode")).string(valueAccessModeToString(definition.accessMode));
    }
    if (definition.unit != Unit::Unknown) {
      writer.property(F("unit")).string(unitSymbol(definition.unit));
    }
  }
  if (entry.lastUpdate.isSet()) {
    if (numbersAsDecimals) {
      writer.property(F("rawValue")).string(getRawValueAsDecString(entry.rawValue));
    } else {
      writer.property(F("rawValue")).string(getRawValueAsHexString(entry.rawValue));
    }
    if (!definition.isUndefined()) {
      writer.property(F("value"));
      conversion.toJson(writer, entry.id, entry.rawValue);
    }
  }
  writer.property(F("lastUpdate")).string(entry.lastUpdate.toString());
  writer.property(F("source")).string(entry.source.toString());
  if (!compact) {
    writer.property(F("subscribed")).boolean(entry.subscribed);
    writer.property(F("writable")).boolean(entry.writable);
  }
  
  writer.close();
}

};

#endif
