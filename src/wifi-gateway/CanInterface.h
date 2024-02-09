#ifndef CANINTERFACE_H_
#define CANINTERFACE_H_

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <toolbox/String.h>

struct CanMessage {
  uint32_t id;
  bool ext;
  bool rtr;
  uint8_t len;
  uint8_t data[8];

  CanMessage()
  : id(0), ext(false), rtr(false), len(0) {
    memset(this->data, 0, 8);
  }

  CanMessage(uint32_t id, bool ext, bool rtr, uint8_t len, const uint8_t (&data)[8])
  : id(id), ext(ext), rtr(rtr), len(len) {
    memcpy(this->data, data, std::size(data));
  }
};

enum struct CanMode : uint8_t {
  Normal = 0,
  ListenOnly = 1,
  LoopBack = 2,
};

toolbox::strref canModeToString(CanMode mode) {
  switch (mode) {
    case CanMode::Normal: return "Normal";
    case CanMode::ListenOnly: return "ListenOnly";
    case CanMode::LoopBack: return "LoopBack";
    default: return "?";
  }
}

CanMode canModeFromString(toolbox::strref mode) {
  if (mode == "Normal") return CanMode::Normal;
  if (mode == "ListenOnly") return CanMode::ListenOnly;
  if (mode == "LoopBack") return CanMode::LoopBack;
  return CanMode::ListenOnly;
}

struct CanCounters {
  uint32_t rx = 0;
  uint32_t tx = 0;
  uint32_t err = 0;
};

class ICanInterface {
public:
  virtual bool setMode(CanMode mode) = 0;
  virtual void reset() = 0;
  virtual bool ready() const = 0;
  virtual void onReady(std::function<void()> readyHandler) = 0;
  virtual void onMessage(std::function<void(const CanMessage& message)> messageHandler) = 0;
  virtual void sendCanMessage(const CanMessage& message) = 0;
  virtual CanCounters const& counters() const = 0;
};

#endif
