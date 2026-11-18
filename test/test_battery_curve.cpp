#include "../src/battery_curve.h"
#include "../src/battery_median.h"

constexpr bool curveBehavior() {
  uint8_t previous = 0;
  for (int millivolts = 2500; millivolts <= 4500; ++millivolts) {
    const float voltage = millivolts / 1000.0f;
    const uint8_t displayed = battery::displayPercent(voltage);
    if (displayed < previous || (millivolts < 4000 && displayed == 100) ||
        (millivolts >= 4000 && displayed != 100) ||
        (millivolts <= 3950 && displayed != battery::protectionPercent(voltage))) return false;
    previous = displayed;
  }
  return true;
}

constexpr bool smoothingBehavior() {
  BatteryMedian median;
  for (int i = 0; i < 5; ++i) median.add(battery::displayPercent(3.95f));
  if (median.add(battery::displayPercent(4.00f)) != 70) return false;
  if (median.add(battery::displayPercent(4.00f)) != 70) return false;
  return median.add(battery::displayPercent(4.00f)) == 100;
}

constexpr bool configurableCurveBehavior(uint16_t zero, uint16_t full) {
  uint8_t previous = 0;
  for (int mv = 2500; mv <= 4500; ++mv) {
    const uint8_t value = battery::displayPercent(mv / 1000.0f, zero, full);
    if (value < previous || (mv <= zero && value != 0) ||
        (mv < full && value == 100) || (mv >= full && value != 100)) return false;
    previous = value;
  }
  return true;
}

constexpr bool originalCurveBehavior() {
  for (int mv = 3300; mv <= 4200; ++mv) {
    uint8_t original = battery::protectionPercent(mv / 1000.0f);
    if (mv < 4200 && original == 100) original = 99;
    if (battery::displayPercent(mv / 1000.0f, 3300, 4200) != original) return false;
  }
  return true;
}

static_assert(curveBehavior(), "Kennlinie ist monoton und erreicht 100 Prozent erst bei 4 V");
static_assert(originalCurveBehavior(), "Urspruengliche Kurve bei 3,30 bis 4,20 V");
static_assert(configurableCurveBehavior(2500, 4500), "Breiter Bereich");
static_assert(configurableCurveBehavior(3800, 3900), "Bereich unter 3,95 V");
static_assert(configurableCurveBehavior(4000, 4100), "Bereich ueber 3,95 V");
static_assert(configurableCurveBehavior(3500, 4200), "Abweichende Untergrenze");
static_assert(smoothingBehavior(), "Glattung bleibt bei neuer Kennlinie erhalten");
static_assert(battery::displayPercent(3.30f) == 0, "Untergrenze");
static_assert(battery::displayPercent(3.69f) == 10, "Warnbereich unveraendert");
static_assert(battery::displayPercent(3.95f) == 70, "Uebergang");
static_assert(battery::displayPercent(3.975f) == 85, "Interpolation");
static_assert(battery::displayPercent(3.9999f) == 99, "Unter 4 V nicht voll");
static_assert(battery::displayPercent(4.00f) == 100, "4 V sind 100 Prozent");
static_assert(battery::displayPercent(4.20f) == 100, "Obergrenze");
static_assert(battery::protectionPercent(3.98f) == 75, "Schutzkurve unveraendert");
static_assert(battery::protectionPercent(4.00f) == 78, "Schutzkurve bleibt unabhaengig");
static_assert(battery::protectionPercent(4.20f) == 100, "Schutzkurve Obergrenze");
static_assert(battery::displayPercent(4.11f, 3300, 4200) == 90, "Urspruengliche Kurve bei 4,11 V");
static_assert(battery::displayPercent(4.01f, 3300, 4200) == 79, "Urspruengliche Kurve bei 4,01 V");
static_assert(battery::displayPercent(3.50f, 3500, 4200) == 0, "Einstellbare Untergrenze");
static_assert(battery::displayPercent(3.95f, 3500, 4200) == 70, "Stuetzpunkt bleibt erhalten");
static_assert(battery::displayPercent(4.20f, 3500, 4200) == 100, "Einstellbare Obergrenze");
static_assert(battery::displayPercent(4.30f, 3500, 4200) == 100, "Oberhalb der Obergrenze");
static_assert(battery::displayPercent(3.95f, 3000, 4300) == 70, "Kennlinie bei anderen Grenzen");
static_assert(battery::displayPercent(4.05f, 4000, 4100) == 70, "Stuetzpunkt im engen Bereich");
