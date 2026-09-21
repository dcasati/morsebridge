#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

enum rmt_ch_dir_t { RMT_TX_MODE };
enum rmt_reserve_memsize_t { RMT_MEM_NUM_BLOCKS_1 };
struct rmt_data_t {
  uint32_t duration0 = 0;
  uint32_t level0 = 0;
  uint32_t duration1 = 0;
  uint32_t level1 = 0;
};

namespace FakeRmt {
inline int pin = -1;
inline bool initSucceeds = true;
inline bool writeSucceeds = true;
inline bool completed = true;
inline const rmt_data_t *inFlight = nullptr;
inline std::vector<std::vector<rmt_data_t>> frames;
}

inline bool rmtInit(int pin, rmt_ch_dir_t, rmt_reserve_memsize_t, uint32_t frequency) {
  assert(frequency == 10000000);
  FakeRmt::pin = pin;
  return FakeRmt::initSucceeds;
}
inline bool rmtTransmitCompleted(int pin) {
  assert(pin == FakeRmt::pin);
  return FakeRmt::completed;
}
inline bool rmtWriteAsync(int pin, rmt_data_t *data, size_t count) {
  assert(pin == 21 && count == 25);
  if (!FakeRmt::writeSucceeds) return false;
  FakeRmt::inFlight = data;
  FakeRmt::frames.emplace_back(data, data + count);
  return true;
}
