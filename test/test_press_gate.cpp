#include "../src/press_gate.h"

constexpr bool pressInterval() {
  PressGate gate;
  return gate.accept(1000000, 300) &&
         !gate.accept(1299999, 300) &&
         gate.accept(1300000, 300) &&
         !gate.accept(1299999, 300) &&
         !gate.accept(1599999, 300) &&
         gate.accept(1600000, 300);
}

static_assert(pressInterval(), "300 ms press interval");
static_assert(!showStarterHoldWarning(999999, 1000), "no warning before one second");
static_assert(!showStarterHoldWarning(1000000, 1000), "no warning at one second");
static_assert(showStarterHoldWarning(1000001, 1000), "warning after one second");
