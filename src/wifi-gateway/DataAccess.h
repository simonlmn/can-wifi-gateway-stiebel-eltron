#ifndef DATAACCESS_H_
#define DATAACCESS_H_

#include <iot_core/Interfaces.h>
#include <iot_core/DateTime.h>
#include <iot_core/Utils.h>
#include "DateTimeSource.h"
#include "OperationResult.h"
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
  uint8_t writeRetries;
  bool subscribed;
  bool writable; // NOTE: this only means that this entry has been marked for writing via the API

  bool isConfigured() const { return subscribed || writable; }

  DataEntry() : id(0), source(), rawValue(0), toWrite(0), lastUpdate(), lastUpdateMs(0), lastRequestMs(0), lastWriteMs(0), writeRetries(0), subscribed(false), writable(false) {}
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
    case DataCaptureMode::None: return "None";
    case DataCaptureMode::Configured: return "Configured";
    case DataCaptureMode::Defined: return "Defined";
    case DataCaptureMode::Any: return "Any";
    default: return "?";
  }
}

DataCaptureMode dataCaptureModeFromString(const toolbox::strref& mode) {
  if (mode == F("None")) return DataCaptureMode::None;
  if (mode == F("Configured")) return DataCaptureMode::Configured;
  if (mode == F("Defined")) return DataCaptureMode::Defined;
  if (mode == F("Any")) return DataCaptureMode::Any;
  return DataCaptureMode::None;
}

enum struct WriteResult : uint8_t {
  Accepted = 0,
  ReadOnly = 1,
  NotFound = 2,
  NotWritable = 3,
  ConfirmationRequired = 4,
};

const char* writeResultToString(WriteResult result) {
  switch (result) {
    case WriteResult::Accepted: return "Accepted";
    case WriteResult::ReadOnly: return "Write access is disabled (read-only mode)";
    case WriteResult::NotFound: return "Data entry not found or not configured";
    case WriteResult::NotWritable: return "Value is not defined/configured as writable";
    case WriteResult::ConfirmationRequired: return "Write confirmation required for protected value";
    default: return "Unknown error";
  }
}

class DataAccess final : public iot_core::IApplicationComponent, public IStiebelEltronDevice {
public:
  using DataKey = std::pair<DeviceId, ValueId>;
  using DataMap = std::map<DataKey, DataEntry>;

private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  StiebelEltronProtocol& _protocol;
  IDefinitionRepository& _definitions;
  gpiobj::DigitalInput& _writeEnablePin;
  DeviceId _deviceId;
  DataCaptureMode _mode;
  bool _readOnly;
  bool _ignoreDateTime;
  DataMap _data;
  DataMap::iterator _dataIterator;

  std::function<void(DataEntry const& entry)> _updateHandler;

public:
  DataAccess(iot_core::ISystem& system, StiebelEltronProtocol& protocol, IDefinitionRepository& definitions, gpiobj::DigitalInput& writeEnablePin)
    : _logger(system.logger("dta")),
    _system(system),
    _protocol(protocol),
    _definitions(definitions),
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
      _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Cannot set device ID with wildcards '%s'."), deviceId.toString()));
      return false;
    }
    _deviceId = deviceId;
    _logger.log(toolbox::format(F("Set device ID '%s'."), _deviceId.toString()));
    return true;
  }

  bool setMode(DataCaptureMode mode) {
    _mode = mode;
    _logger.log(toolbox::format(F("Set mode '%s'."), dataCaptureModeToString(_mode)));
    return true;
  }

  bool setReadOnly(bool readOnly) {
    _readOnly = readOnly;
    _logger.log(toolbox::format(F("%s write access (%seffective)."), _readOnly ? F("Disabled") : F("Enabled"), effectiveReadOnly() == _readOnly ? "" : "NOT "));
    return true;
  }

  bool effectiveReadOnly() const {
    return !_writeEnablePin ? true : _readOnly;
  }

  bool setIgnoreDateTime(bool ignoreDateTime) {
    _ignoreDateTime = ignoreDateTime;
    _logger.log(toolbox::format(F("%s date/time availability."), _ignoreDateTime ? F("Ignoring") : F("Waiting for")));
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

  WriteResult write(DataKey const& key, uint16_t rawValue, bool confirmWrite = false) {
    if (effectiveReadOnly()) {
      return WriteResult::ReadOnly;
    }

    DataEntry* entry = getEntryInternal(key);
    if (entry == nullptr || !entry->isConfigured()) {
      return WriteResult::NotFound;
    }

    const ValueDefinition& definition = getDefinition(entry->id);
    if (!entry->writable || definition.accessMode < ValueAccessMode::Writable) {
      return WriteResult::NotWritable;
    }

    // Require confirmation for protected values
    if ((definition.accessMode == ValueAccessMode::WritableProtected || 
         definition.accessMode == ValueAccessMode::WritableExtraProtected) && !confirmWrite) {
      return WriteResult::ConfirmationRequired;
    }

    entry->toWrite = rawValue;
    entry->lastWriteMs = millis() - WRITE_INTERVAL_MS; // Schedule immediate write on next maintenance cycle
    entry->writeRetries = 0;
    _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Write scheduled for %u: %u"), entry->id, rawValue));
    return WriteResult::Accepted;
  }

private:
  const ValueDefinition& getDefinition(ValueId id) const {
    return _definitions.get(id);
  }

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

    if (getDefinition(key.second).accessMode == ValueAccessMode::None) {
      // Don't allow subscribing to inaccessible values
      return false;
    }

    auto& entry = _data[key];
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

    if (getDefinition(key.second).accessMode < ValueAccessMode::Writable) {
      // Don't allow adding not writable values
      return false;
    }

    auto& entry = _data[key];
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

  // Note: CAN bus protection is handled by SerialCan with token bucket rate limiting
  // These constants control retry behavior and timing, not rate limiting
  static constexpr uint32_t MIN_UPDATE_INTERVAL_MS = 30000; // Min 30s between subscription requests
  static constexpr unsigned long WRITE_INTERVAL_MS = 3000; // 3s between write retries
  static constexpr unsigned long WRITE_VERIFY_DELAY_MS = 1000; // Wait 1s after write before requesting verification
  static constexpr uint8_t MAX_WRITE_RETRIES = 5; // Limit write attempts to avoid infinite retries
  static constexpr unsigned long MAINTENANCE_INTERVAL_MS = 100; // Check every 100ms

  iot_core::IntervalTimer _maintenanceInterval {MAINTENANCE_INTERVAL_MS};

  void maintainData() {
    if (_maintenanceInterval.elapsed()) {
      unsigned long currentMs = millis();
      doDataMaintenance(currentMs);
      _maintenanceInterval.restart();
    }
  }

  void doDataMaintenance(unsigned long currentMs) {
    size_t remainingDataEntries = _data.size();
    while (remainingDataEntries > 0) {
      if (_dataIterator == _data.end()) {
        _dataIterator = _data.begin();
      }
      
      DataEntry& entry = _dataIterator->second;

      OperationResult sendResult = OperationResult::Accepted;
      if (entry.writable) {
        if (entry.lastUpdateMs != 0) { // we already have received a value from the source
          if (entry.lastWriteMs != 0 && (currentMs > entry.lastWriteMs + WRITE_INTERVAL_MS)) {
            if (entry.writeRetries >= MAX_WRITE_RETRIES) {
              _logger.log(iot_core::LogLevel::Warning, toolbox::format(F("Write failed after %u retries for %u (wanted: %u, current: %u)"), 
                entry.writeRetries, entry.id, entry.toWrite, entry.rawValue));
              entry.lastWriteMs = 0; // Give up on this write
              entry.writeRetries = 0;
            } else {
              _logger.log(iot_core::LogLevel::Debug, toolbox::format(F("Attempting write for %u: %u"), entry.id, entry.toWrite));
              sendResult = _protocol.write({ _deviceId, entry.source, entry.id, entry.toWrite });
              if (sendResult == OperationResult::Accepted) {
                entry.lastWriteMs = currentMs;
                entry.writeRetries++;
                // Wait a bit before requesting verification to give the target time to process
                entry.lastRequestMs = currentMs + WRITE_VERIFY_DELAY_MS - MIN_UPDATE_INTERVAL_MS;
                entry.lastUpdateMs = 0; // triggers request for new value from source after delay
                _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Write attempt %u for %u: %u"), entry.writeRetries, entry.id, entry.toWrite));
              }
            }
          }
        } else { // request the value from the source first before allowing to write it
          if (currentMs > entry.lastRequestMs + MIN_UPDATE_INTERVAL_MS) {
            _logger.log(iot_core::LogLevel::Debug, toolbox::format(F("Requesting initial value for writable %u"), entry.id));
            sendResult = _protocol.request({ _deviceId, entry.source, entry.id });
            if (sendResult == OperationResult::Accepted) {
              entry.lastRequestMs = currentMs;
            }
          }
        }
      } else if (entry.subscribed) {
        if (currentMs > entry.lastUpdateMs + std::max(MIN_UPDATE_INTERVAL_MS, getDefinition(entry.id).updateIntervalMs)
          && currentMs > entry.lastRequestMs + MIN_UPDATE_INTERVAL_MS) {
          _logger.log(iot_core::LogLevel::Debug, toolbox::format(F("Requesting update for subscribed %u"), entry.id));
          sendResult = _protocol.request({ _deviceId, entry.source, entry.id });
          if (sendResult == OperationResult::Accepted) {
            entry.lastRequestMs = currentMs;
          }
        }
      }

      switch (sendResult)
      {
        case OperationResult::Accepted:
          // All good, proceed with next entry
          break;
        case OperationResult::Invalid:
          // Should normally not happen, but if it does, skip this entry this time and proceed with next entry
          _logger.log(iot_core::LogLevel::Error, toolbox::format(F("Failed to send request/write for %u: Invalid parameters."), entry.id));
          break;
        case OperationResult::NotReady:
        case OperationResult::RateLimited:
        case OperationResult::QueueFull:
          // stop processing for now, try again later
          _logger.log(iot_core::LogLevel::Debug, toolbox::format(F("Deferring further data maintenance due to %s."), operationResultToString(sendResult)));
          return;
      }

      ++_dataIterator;
      --remainingDataEntries;

      _system.lyield();
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
      switch (_mode) {
        case DataCaptureMode::Configured:
          entry = getEntryInternal(key);
          break;
        case DataCaptureMode::Defined:
          entry = getDefinition(key.second).isUndefined() ? nullptr : &_data[key];
          break;
        case DataCaptureMode::Any:
          entry = &_data[key];
          break;
        default:
          entry = nullptr;
          break;
      }

      if (entry == nullptr) {
        return;
      }

      entry->source = key.first;
      entry->id = key.second;
      entry->rawValue = value;
      entry->lastUpdate = now;
      entry->lastUpdateMs = millis();

      if (entry->lastWriteMs > 0 && entry->toWrite == entry->rawValue) {
        // As there is currently a write in progress and we just received
        // the new value which is now the same, we consider it done.
        _logger.log(iot_core::LogLevel::Info, toolbox::format(F("Write confirmed for %u: %u (after %u attempts)"), entry->id, entry->rawValue, entry->writeRetries));
        entry->lastWriteMs = 0;
        entry->writeRetries = 0;
      }

      if (_updateHandler) _updateHandler(*entry);
    }
  }
};

#endif
