#pragma once

#include <stdint.h>

class BatteryMedian {
 public:
  constexpr void clear() {
    count_ = 0;
    next_ = 0;
  }

  constexpr uint8_t add(uint8_t value) {
    values_[next_] = value;
    next_ = (next_ + 1) % 5;
    if (count_ < 5) ++count_;
    uint8_t sorted[5] = {};
    for (uint8_t i = 0; i < count_; ++i) {
      sorted[i] = values_[i];
      for (uint8_t j = i; j > 0 && sorted[j] < sorted[j - 1]; --j) {
        uint8_t tmp = sorted[j];
        sorted[j] = sorted[j - 1];
        sorted[j - 1] = tmp;
      }
    }
    if (count_ % 2) return sorted[count_ / 2];
    return (static_cast<uint16_t>(sorted[count_ / 2 - 1]) + sorted[count_ / 2] + 1) / 2;
  }

 private:
  uint8_t values_[5] = {};
  uint8_t count_ = 0;
  uint8_t next_ = 0;
};
