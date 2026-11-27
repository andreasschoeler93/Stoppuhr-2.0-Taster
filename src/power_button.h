#pragma once

#include <stdint.h>

namespace power_button {

constexpr uint32_t WARNING_MS = 5000;
constexpr uint32_t SHUTDOWN_MS = 10000;

constexpr bool warningDue(uint32_t heldMs) {
  return heldMs >= WARNING_MS;
}

constexpr bool shutdownDue(uint32_t heldMs, bool blocked) {
  return heldMs >= SHUTDOWN_MS && !blocked;
}

constexpr uint8_t secondsRemaining(uint32_t heldMs) {
  return heldMs >= SHUTDOWN_MS
             ? 0
             : static_cast<uint8_t>((SHUTDOWN_MS - heldMs + 999) / 1000);
}

}  // namespace power_button
