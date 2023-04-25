#pragma once

#ifdef TEST_ENV
#include <cstdint>
#include <cstring>
#endif

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
    memcpy(this->data, data, 8);
  }
};

enum struct CanLogLevel : uint8_t {
  None = 0,
  Error = 1,
  Status = 2,
  Rx = 3,
  Tx = 4
};

struct CanCounters {
  uint32_t rx = 0;
  uint32_t tx = 0;
  uint32_t err = 0;
};
