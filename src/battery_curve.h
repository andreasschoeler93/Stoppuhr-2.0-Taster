#pragma once

#include <stdint.h>

namespace battery {

struct CurvePoint { float volt; float percent; };
constexpr CurvePoint CURVE[] = {
  {3.30f, 0}, {3.61f, 5}, {3.69f, 10}, {3.71f, 15}, {3.73f, 20}, {3.75f, 25},
  {3.77f, 30}, {3.79f, 35}, {3.80f, 40}, {3.82f, 45}, {3.84f, 50}, {3.85f, 55},
  {3.87f, 60}, {3.91f, 65}, {3.95f, 70}, {3.98f, 75}, {4.02f, 80}, {4.08f, 85},
  {4.11f, 90}, {4.15f, 95}, {4.20f, 100},
};
constexpr int CURVE_N = sizeof(CURVE) / sizeof(CURVE[0]);

constexpr uint8_t protectionPercent(float voltage) {
  if (!(voltage > CURVE[0].volt)) return 0;
  if (voltage >= CURVE[CURVE_N - 1].volt) return 100;
  for (int i = 1; i < CURVE_N; ++i) {
    if (voltage <= CURVE[i].volt) {
      float span = CURVE[i].volt - CURVE[i - 1].volt;
      float f = (voltage - CURVE[i - 1].volt) / span;
      return static_cast<uint8_t>(CURVE[i - 1].percent
                                  + f * (CURVE[i].percent - CURVE[i - 1].percent) + 0.5f);
    }
  }
  return 0;
}

constexpr float displayCurveVoltage(float voltage, uint16_t zeroMv, uint16_t fullMv) {
  return 3.30f + (voltage - zeroMv / 1000.0f) * 0.70f / ((fullMv - zeroMv) / 1000.0f);
}

constexpr uint8_t displayPercent(float voltage, uint16_t zeroMv = 3300, uint16_t fullMv = 4000) {
  const float zero = zeroMv / 1000.0f;
  const float full = fullMv / 1000.0f;
  if (!(voltage > zero)) return 0;
  if (voltage >= full) return 100;
  const float pivot = zero < 3.95f && full > 3.95f ? 3.95f : (zero + full) / 2.0f;
  const float curveVoltage = voltage <= pivot
    ? (zeroMv == 3300 && pivot == 3.95f
       ? voltage : 3.30f + (voltage - zero) * 0.65f / (pivot - zero))
    : (fullMv == 4200 && pivot == 3.95f
       ? voltage : 3.95f + (voltage - pivot) * 0.25f / (full - pivot));
  const uint8_t percent = protectionPercent(curveVoltage);
  return percent < 100 ? percent : 99;
}

}  // namespace battery
