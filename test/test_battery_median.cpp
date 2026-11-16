#include "../src/battery_median.h"

constexpr bool medianBehavior() {
  BatteryMedian median;
  if (median.add(50) != 50 || median.add(60) != 55 || median.add(50) != 50 ||
      median.add(50) != 50 || median.add(50) != 50 || median.add(5) != 50 ||
      median.add(5) != 50 || median.add(5) != 5 || median.add(80) != 5) return false;
  median.clear();
  return median.add(80) == 80 && median.add(100) == 90;
}

static_assert(medianBehavior(), "Median: Anlauf, Ausreisser, Ringpuffer und Reset");
