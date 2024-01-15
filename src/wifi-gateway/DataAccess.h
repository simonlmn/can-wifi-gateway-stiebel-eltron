#ifndef DATAACCESS_H_
#define DATAACCESS_H_

#include <iot_core/Interfaces.h>
#include <iot_core/DateTime.h>
#include <iot_core/Utils.h>
#include "DateTimeSource.h"
#include "StiebelEltronProtocol.h"
#include "ValueDefinitions.h"
#include <utility>
#include <map>
#include <algorithm>
#include <LittleFS.h>
#include <gpiobj/DigitalInput.h>

struct DataEntry {
  ValueId id;
  DeviceId source;
  uint16_t rawValue;
  uint16_t toWrite;
  iot_core::DateTime lastUpdate;
  unsigned long lastUpdateMs;
  unsigned long lastRequestMs;
  unsigned long lastWriteMs;
  bool subscribed;
  bool writable; // NOTE: this only means that this entry has been marked for writing via the API
  const ValueDefinition* definition;

  bool hasDefinition() const { return definition != NULL && !definition->isUnknown(); }
  bool isConfigured() const { return subscribed || writable; }

  DataEntry() : id(0), source(), rawValue(0), toWrite(0), lastUpdate(), lastUpdateMs(0), lastRequestMs(0), lastWriteMs(0), subscribed(false), writable(false), definition(NULL) {}
};

static const char SUBSCRIPTIONS_FILE_HEADER[] = "~S1.0";
static const char WRITABLES_FILE_HEADER[] = "~W1.0";

enum struct DataCaptureMode : uint8_t {
  None = 0, // Do not store any data at all
  Configured = 1, // Only store data for previously configured keys (subscriptions or writables).
  Defined = 2, // Store any data with a known ValueDefinition.
  Any = 3, // Store any data, even if no ValueDefinition exists.
};

const char* dataCaptureModeToString(DataCaptureMode mode) {
  switch (mode) {
    case DataCaptureMode::None:
      return "None";
    case DataCaptureMode::Configured:
      return "Configured";
    case DataCaptureMode::Defined:
      return "Defined";
    case DataCaptureMode::Any:
      return "Any";
    default:
      return "?";
  }
}

DataCaptureMode dataCaptureModeFromString(const char* mode, size_t length = SIZE_MAX) {
  if (strncmp(mode, "None", length) == 0) return DataCaptureMode::None;
  if (strncmp(mode, "Configured", length) == 0) return DataCaptureMode::Configured;
  if (strncmp(mode, "Defined", length) == 0) return DataCaptureMode::Defined;
  if (strncmp(mode, "Any", length) == 0) return DataCaptureMode::Any;
  return DataCaptureMode::None;
}

class DataAccess final : public iot_core::IApplicationComponent, public IStiebelEltronDevice {
public:
  using DataKey = std::pair<DeviceId, ValueId>;
  using DataMap = std::map<DataKey, DataEntry>;

private:
  iot_core::Logger& _logger;
  iot_core::ISystem& _system;
  StiebelEltronProtocol& _protocol;
  gpiobj::DigitalInput& _writeEnablePin;
  DeviceId _deviceId;
  DataCaptureMode _mode;
  bool _readOnly;
  bool _ignoreDateTime;
  DataMap _data;
  DataMap::iterator _dataIterator;

  std::function<void(DataEntry const& entry)> _updateHandler;

public:
  DataAccess(iot_core::ISystem& system, StiebelEltronProtocol& protocol, gpiobj::DigitalInput& writeEnablePin)
    : _logger(system.logger()),
    _system(system),
    _protocol(protocol),
    _writeEnablePin(writeEnablePin),
    _deviceId(),
    _mode(DataCaptureMode::Configured),
    _readOnly(true),
    _ignoreDateTime(false),
    _data(),
    _dataIterator(_data.begin()),
    _updateHandler()
  { }

  const char* name() const override {
    return "dta";
  }

  const char* description() const override {
    return "Data Access";
  }

  bool configure(const char* name, const char* value) override {
    if (strcmp(name, "deviceId") == 0) return DeviceId::fromString(value).then([this] (DeviceId id) { return setDeviceId(id); }).otherwise([] () { return false; });
    if (strcmp(name, "mode") == 0) return setMode(dataCaptureModeFromString(value));
    if (strcmp(name, "readOnly") == 0) return setReadOnly(iot_core::convert<bool>::fromString(value, true));
    if (strcmp(name, "ignoreDateTime") == 0) return setIgnoreDateTime(iot_core::convert<bool>::fromString(value, false));
    return false;
  }

  void getConfig(std::function<void(const char*, const char*)> writer) const override {
    writer("deviceId", _deviceId.toString());
    writer("mode", dataCaptureModeToString(_mode));
    writer("readOnly", iot_core::convert<bool>::toString(_readOnly));
    writer("ignoreDateTime", iot_core::convert<bool>::toString(_ignoreDateTime));
  }

  bool setDeviceId(DeviceId deviceId) {
    if (!deviceId.isExact()) {
      _logger.log(iot_core::LogLevel::Warning, name(), iot_core::format(F("Cannot set device ID with wildcards '%s'."), deviceId.toString()));
      return false;
    }
    _deviceId = deviceId;
    _logger.log(name(), iot_core::format(F("Set device ID '%s'."), _deviceId.toString()));
    return true;
  }

  bool setMode(DataCaptureMode mode) {
    _mode = mode;
    _logger.log(name(), iot_core::format(F("Set mode '%s'."), dataCaptureModeToString(_mode)));
    return true;
  }

  bool setReadOnly(bool readOnly) {
    _readOnly = readOnly;
    _logger.log(name(), iot_core::format(F("%s write access (%seffective)."), _readOnly ? "Disabled" : "Enabled", effectiveReadOnly() == _readOnly ? "" : "NOT "));
    return true;
  }

  bool effectiveReadOnly() const {
    return !_writeEnablePin ? true : _readOnly;
  }

  bool setIgnoreDateTime(bool ignoreDateTime) {
    _ignoreDateTime = ignoreDateTime;
    _logger.log(name(), iot_core::format(F("%s date/time availability."), _ignoreDateTime ? "Ignoring" : "Waiting for"));
    return true;
  }

  void setup(bool /*connected*/) override {
    restoreSubscriptions();
    restoreWritables();

    _protocol.addDevice(this);
    _protocol.onResponse([this] (ResponseData const& data) { processData({data.sourceId, data.valueId}, data.value); });
    _protocol.onWrite([this] (WriteData const& data) { processData({data.targetId.isExact() ? data.targetId : data.sourceId, data.valueId}, data.value); });
  }

  void loop(iot_core::ConnectionStatus /*status*/) override {
    if (!_deviceId.isExact() || !_protocol.ready() || (!_ignoreDateTime && !currentDateTime().isSet())) {
      return;
    }
    
    maintainData();
  }

  void getDiagnostics(iot_core::IDiagnosticsCollector& /*collector*/) const override {
  }

  const DeviceId& deviceId() const override {
    return _deviceId;
  }

  void request(const RequestData& data) override {
    // no data to be requested from here
  }

  void write(const WriteData& data) override {
    // we capture any writes with a global listener already
  }

  void receive(const ResponseData& data) override {
    // we capture any responses with a global listener already
  }

  void onUpdate(std::function<void(DataEntry const& entry)> updateHandler) {
    if (_updateHandler) {
      auto previousHandler = _updateHandler;
      _updateHandler = [=](DataEntry const& entry) { previousHandler(entry); updateHandler(entry); };
    } else {
      _updateHandler = updateHandler;
    }
  }

  const DataMap& getData() const {
    return _data;
  }

  const DataEntry* getEntry(DataKey const& key) const {
    auto result = _data.find(key);
    if (result == _data.end()) {
      return nullptr;
    } else {
      return &result->second;
    }
  }

  bool addSubscription(DataKey const& key) {
    bool added = addSubscriptionInternal(key);
    if (added) {
      persistSubscriptions();
    }
    return added;
  }

  void removeSubscription(DataKey const& key) {
    removeSubscriptionInternal(key);
    persistSubscriptions();
  }

  bool addWritable(DataKey const& key) {
    bool added = addWritableInternal(key);
    if (added) {
      persistWritables();
    }
    return added;
  }

  void removeWritable(DataKey const& key) {
    removeWritableInternal(key);
    persistWritables();
  }

  const iot_core::DateTime& currentDateTime() const {
    return _system.currentDateTime();
  }

  bool write(DataKey const& key, uint16_t rawValue, ValueAccessMode accessMode) {
    if (effectiveReadOnly()) {
      return false;
    }

    DataEntry* entry = getEntryInternal(key);
    if (entry != nullptr && entry->writable && entry->definition->accessMode == accessMode) {
      entry->toWrite = rawValue;
      entry->lastWriteMs = 1;
      return true;
    }

    return false;
  }

private:
  DataEntry* getEntryInternal(DataKey const& key) {
    auto result = _data.find(key);
    if (result == _data.end()) {
      return nullptr;
    } else {
      return &result->second;
    }
  }
  
  bool addSubscriptionInternal(DataKey const& key) {
    if (!key.first.isExact()) {
      // Subscription has to be to a specific device ID
      return false;
    }

    const auto& definition = getDefinition(key.second);
    if (definition.accessMode == ValueAccessMode::None) {
      // Don't allow subscribing to inaccessible values
      return false;
    }

    auto& entry = _data[key];
    entry.definition = &definition;
    entry.source = key.first;
    entry.id = key.second;
    entry.subscribed = true;

    return true;
  }

  void removeSubscriptionInternal(DataKey const& key) {
    auto& entry = _data[key];
    entry.subscribed = false;
  }

  void restoreSubscriptions() {
    auto subscriptionsFile = LittleFS.open("/subscriptions", "r");
    if (subscriptionsFile) {
      if (subscriptionsFile.available() > 5) {
        char header[6] = {0};
        subscriptionsFile.readBytes(header, 5);
        if (strcmp(header, SUBSCRIPTIONS_FILE_HEADER) == 0) {
          uint8_t entry[4] = {0};
          while (subscriptionsFile.available()) {
            if (subscriptionsFile.read(entry, 4) == 4) {
              ValueId valueId {(entry[0] << 8) | entry[1]};
              DeviceId deviceId {DeviceType(entry[2]), entry[3]};
              addSubscriptionInternal({deviceId, valueId});
            }
            _system.lyield();
          }
        } else {
          // Different file format or version, ignore for now.
          // The user has to set up the subscriptions again.
        }
      }
      subscriptionsFile.close();
    }
  }

  void persistSubscriptions() {
    auto subscriptionsFile = LittleFS.open("/subscriptions", "w");
    if (subscriptionsFile) {
      subscriptionsFile.write(SUBSCRIPTIONS_FILE_HEADER);
      for (auto& data : _data) {
        if (data.second.subscribed) {
          uint8_t entry[4] = {
            (data.second.id >> 8) & 0xFFu,
            data.second.id & 0xFFu,
            static_cast<uint8_t>(data.second.source.type),
            data.second.source.address
          };
          subscriptionsFile.write(entry, 4);
        }
        _system.lyield();
      }
      subscriptionsFile.close();
    }
  }
  
  bool addWritableInternal(DataKey const& key) {
    if (!key.first.isExact()) {
      // Writable has to be to a specific device ID
      return false;
    }

    const auto& definition = getDefinition(key.second);
    if (definition.accessMode < ValueAccessMode::Writable) {
      // Don't allow adding not writable values
      return false;
    }

    auto& entry = _data[key];
    entry.definition = &definition;
    entry.source = key.first;
    entry.id = key.second;
    entry.writable = true;

    return true;
  }

  void removeWritableInternal(DataKey const& key) {
    auto& entry = _data[key];
    entry.writable = false;
  }
  
  void restoreWritables() {
    auto writablesFile = LittleFS.open("/writables", "r");
    if (writablesFile) {
      if (writablesFile.available() > 5) {
        char header[6] = {0};
        writablesFile.readBytes(header, 5);
        if (strcmp(header, WRITABLES_FILE_HEADER) == 0) {
          uint8_t entry[4] = {0};
          while (writablesFile.available()) {
            if (writablesFile.read(entry, 4) == 4) {
              ValueId valueId {(entry[0] << 8) | entry[1]};
              DeviceId deviceId {DeviceType(entry[2]), entry[3]};
              addWritableInternal({deviceId, valueId});
            }
            _system.lyield();
          }
        } else {
          // Different file format or version, ignore for now.
          // The user has to set up the writables again.
        }
      }
      writablesFile.close();
    }
  }

  void persistWritables() {
    auto writablesFile = LittleFS.open("/writables", "w");
    if (writablesFile) {
      writablesFile.write(WRITABLES_FILE_HEADER);
      for (auto& data : _data) {
        if (data.second.writable) {
          uint8_t entry[4] = {
            (data.second.id >> 8) & 0xFFu,
            data.second.id & 0xFFu,
            static_cast<uint8_t>(data.second.source.type),
            data.second.source.address
          };
          writablesFile.write(entry, 4);
        }
        _system.lyield();
      }
      writablesFile.close();
    }
  }

  static constexpr unsigned long MIN_UPDATE_INTERVAL_MS = 30000;
  static constexpr unsigned long WRITE_INTERVAL_MS = 30000;
  static constexpr unsigned long MAINTENANCE_INTERVAL_MS = 375;
  static constexpr size_t MAX_CONCURRENT_OPERATIONS = 2;

  iot_core::IntervalTimer _maintenanceInterval {MAINTENANCE_INTERVAL_MS};

  void maintainData() {
    if (_maintenanceInterval.elapsed()) {
      unsigned long currentMs = millis();
      size_t remainingDataEntries = _data.size();
      size_t remainingRequests = MAX_CONCURRENT_OPERATIONS;
      while (remainingRequests > 0 && remainingDataEntries > 0) {
        if (_dataIterator == _data.end()) {
          _dataIterator = _data.begin();
        }
        
        DataEntry& entry = _dataIterator->second;

        if (entry.writable) {
          if (entry.lastUpdateMs != 0) { // we already have received a value from the source
            if (entry.lastWriteMs != 0 && (currentMs > entry.lastWriteMs + WRITE_INTERVAL_MS)) {
              _protocol.write({ _deviceId, entry.source, entry.id, entry.toWrite }); --remainingRequests;
              entry.lastWriteMs = currentMs;
              entry.lastUpdateMs = 0; // triggers request for new value from source
            }
          } else { // request the value from the source first before allowing to write it
            if (currentMs > entry.lastRequestMs + MIN_UPDATE_INTERVAL_MS) {
              _protocol.request({ _deviceId, entry.source, entry.id }); --remainingRequests;
              entry.lastRequestMs = currentMs;
            }
          }
        } else if (entry.subscribed) {
          if (currentMs > entry.lastUpdateMs + std::max(MIN_UPDATE_INTERVAL_MS, entry.definition->updateIntervalMs)
           && currentMs > entry.lastRequestMs + MIN_UPDATE_INTERVAL_MS) {
            _protocol.request({ _deviceId, entry.source, entry.id }); --remainingRequests;
            entry.lastRequestMs = currentMs;
          }
        }

        ++_dataIterator;
        --remainingDataEntries;

        _system.lyield();
      }

      _maintenanceInterval.restart();
    }
  }

  void processData(DataKey const& key, uint16_t value) {
    if (_mode == DataCaptureMode::None) {
      return;
    }

    _system.lyield();

    auto const& now = currentDateTime();
    if (_ignoreDateTime || now.isSet()) { // Only process data if we have established the current date and time
      DataEntry* entry = nullptr;
      const ValueDefinition* definition = nullptr;
      switch (_mode) {
        case DataCaptureMode::Configured:
          // we don't need the definition as it already is in the entry when configured.
          entry = getEntryInternal(key);
          break;
        case DataCaptureMode::Defined:
          definition = &getDefinition(key.second);
          entry = definition->isUnknown() ? nullptr : &_data[key];
          break;
        case DataCaptureMode::Any:
          definition = &getDefinition(key.second);
          entry = &_data[key];
          break;
        default:
          definition = nullptr;
          entry = nullptr;
          break;
      }

      if (entry == nullptr) {
        return;
      }

      if (entry->definition == nullptr) {
        entry->definition = definition;
        entry->source = key.first;
        entry->id = key.second;
      }
      entry->rawValue = value;
      entry->lastUpdate = now;
      entry->lastUpdateMs = millis();

      if (entry->lastWriteMs > 0 && entry->toWrite == entry->rawValue) {
        // As there is currently a write in progress and we just received
        // the new value which is now the same, we consider it done.
        entry->lastWriteMs = 0;
      }

      if (_updateHandler) _updateHandler(*entry);
    }
  }
};

#endif
