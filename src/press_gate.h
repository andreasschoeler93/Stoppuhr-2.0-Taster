#pragma once

#include <stdint.h>

class PressGate {
 public:
  constexpr bool accept(int64_t pressUs, uint32_t intervalMs) {
    if (hasPress_ && (pressUs < lastPressUs_ ||
        pressUs - lastPressUs_ < static_cast<int64_t>(intervalMs) * 1000)) return false;
    hasPress_ = true;
    lastPressUs_ = pressUs;
    return true;
  }

 private:
  int64_t lastPressUs_ = 0;
  bool hasPress_ = false;
};
