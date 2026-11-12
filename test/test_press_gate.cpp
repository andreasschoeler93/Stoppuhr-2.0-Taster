#include "../src/press_gate.h"

constexpr bool normalInterval() {
  PressGate gate;
  return gate.accept(1000000, 1000) &&
         !gate.accept(1999999, 1000) &&
         gate.accept(2000000, 1000) &&
         !gate.accept(1999999, 1000) &&
         gate.accept(3000000, 1000);
}

constexpr bool finishInterval() {
  PressGate gate;
  return gate.accept(1000000, 300) &&
         !gate.accept(1299999, 300) &&
         gate.accept(1300000, 300) &&
         !gate.accept(1599999, 300) &&
         gate.accept(1600000, 300);
}

static_assert(normalInterval(), "normal press interval");
static_assert(finishInterval(), "finish press interval");
