#include "measurements.h"

#include <Wire.h>
#include <Adafruit_AHTX0.h>

#include "config.h"

namespace measure {
namespace {

Data g;
Adafruit_AHTX0 aht;
uint8_t ahtFail = 0;
uint32_t lastAhtMs = 0;
uint32_t lastVbatMs = 0;

struct BatteryReading {
  uint32_t rawAverage;
  uint32_t rawMin;
  uint32_t rawMax;
  float adcVoltage;
  float batteryVoltage;
  bool valid;
};

uint8_t vbatToPercent(float voltage) {
  if (!isfinite(voltage)) return 0;
  if (voltage <= 3.30f) return 0;
  if (voltage >= 4.20f) return 100;
  float percent = (voltage - 3.30f) / (4.20f - 3.30f) * 100.0f;
  return (uint8_t)(percent + 0.5f);
}

uint8_t vbatToLevel0to4(float voltage) {
  if (!isfinite(voltage)) return 0;
  if (voltage >= 4.10f) return 4;
  if (voltage >= 3.90f) return 3;
  if (voltage >= 3.75f) return 2;
  if (voltage >= 3.60f) return 1;
  return 0;
}

BatteryReading readBatteryVoltage() {
  digitalWrite(cfg::Pins::MEAS_EN, HIGH);
  delay(cfg::Timing::VBAT_SETTLE_MS);

  uint32_t sumRaw = 0;
  uint32_t sumMilliVolts = 0;
  uint32_t rawMin = UINT32_MAX;
  uint32_t rawMax = 0;
  for (uint8_t i = 0; i < cfg::Timing::VBAT_SAMPLE_COUNT; ++i) {
    uint32_t raw = analogRead(cfg::Pins::VBAT_ADC);
    if (raw < rawMin) rawMin = raw;
    if (raw > rawMax) rawMax = raw;
    sumRaw += raw;
    sumMilliVolts += analogReadMilliVolts(cfg::Pins::VBAT_ADC);
  }

  digitalWrite(cfg::Pins::MEAS_EN, LOW);
  BatteryReading reading;
  reading.rawAverage = sumRaw / cfg::Timing::VBAT_SAMPLE_COUNT;
  reading.rawMin = rawMin;
  reading.rawMax = rawMax;
  reading.adcVoltage = sumMilliVolts / (1000.0f * cfg::Timing::VBAT_SAMPLE_COUNT);
  reading.batteryVoltage = reading.adcVoltage * cfg::VBAT_SCALE;
  reading.valid = isfinite(reading.batteryVoltage)
    && reading.batteryVoltage >= 2.5f
    && reading.batteryVoltage <= 4.5f;
  return reading;
}

uint8_t pollBattery() {
  uint32_t now = millis();
  if (now - lastVbatMs < cfg::Timing::VBAT_POLL_MS) return CHANGED_NONE;
  lastVbatMs = now;

  BatteryReading reading = readBatteryVoltage();
  float newVbat = reading.valid ? reading.batteryVoltage : NAN;
  uint8_t newLevel = vbatToLevel0to4(newVbat);
  uint8_t newPercent = vbatToPercent(newVbat);
  bool changed = isfinite(newVbat) != isfinite(g.vbat)
    || newPercent != g.vbatPercent
    || newLevel != g.vbatLevel;
#if VBAT_SCREEN_DEBUG
  changed = changed || g.rawAverage != reading.rawAverage;
#endif
  g.vbat = newVbat;
  g.vbatLevel = newLevel;
  g.vbatPercent = newPercent;
  g.rawAverage = reading.rawAverage;
  g.adcVoltage = reading.adcVoltage;

#if DBG
  Serial.printf(
    "[VBAT] pin=GPIO%d raw(avg/min/max)=%lu/%lu/%lu adc=%.3fV scale=%.4f "
    "battery=%.3fV status=%s",
    cfg::Pins::VBAT_ADC,
    (unsigned long)reading.rawAverage,
    (unsigned long)reading.rawMin,
    (unsigned long)reading.rawMax,
    reading.adcVoltage,
    cfg::VBAT_SCALE,
    reading.batteryVoltage,
    reading.valid ? "OK" : "OUT_OF_RANGE");
  if (reading.valid) {
    Serial.printf(" level=%u percent=%u%%\n", g.vbatLevel, g.vbatPercent);
  } else {
    Serial.println();
  }
#endif
  return changed ? (CHANGED_BAR | CHANGED_BATTERY) : CHANGED_NONE;
}

uint8_t reinitializeAht() {
#if DBG
  Serial.println("[AHT] reinitializing");
#endif
  Wire.end();
  delay(5);
  Wire.begin(cfg::Pins::I2C_SDA, cfg::Pins::I2C_SCL);
  Wire.setClock(100000);
  delay(10);

  g.ahtOk = aht.begin(&Wire);
  ahtFail = 0;
#if DBG
  Serial.printf("[AHT] begin=%s\n", g.ahtOk ? "OK" : "FAIL");
#endif
  if (!g.ahtOk) {
    g.temperature = NAN;
    g.humidity = NAN;
    return CHANGED_BAR;
  }
  return CHANGED_NONE;
}

uint8_t pollAht() {
  uint32_t now = millis();
  if (now - lastAhtMs < cfg::Timing::AHT_POLL_MS) return CHANGED_NONE;
  lastAhtMs = now;

  if (!g.ahtOk) return reinitializeAht();

  sensors_event_t humidity, temperature;
  bool valid = aht.getEvent(&humidity, &temperature)
    && isfinite(humidity.relative_humidity)
    && humidity.relative_humidity >= 0.0f
    && humidity.relative_humidity <= 100.0f
    && isfinite(temperature.temperature)
    && temperature.temperature >= -40.0f
    && temperature.temperature <= 85.0f;

  if (valid) {
    float newTemp = temperature.temperature;
    float newHum = humidity.relative_humidity;
    bool changed = !isfinite(g.temperature) || !isfinite(g.humidity)
      || lroundf(newTemp * 10) != lroundf(g.temperature * 10)
      || lroundf(newHum) != lroundf(g.humidity);
    g.temperature = newTemp;
    g.humidity = newHum;
    ahtFail = 0;
    return changed ? CHANGED_BAR : CHANGED_NONE;
  }

  // Einzelne Fehlmessungen behalten den letzten Wert, damit nichts flackert.
  uint8_t result = CHANGED_NONE;
  ++ahtFail;
#if DBG
  Serial.println("[AHT] invalid reading");
#endif
  if (ahtFail >= cfg::Timing::AHT_FAIL_CLEAR_AFTER) {
    g.temperature = NAN;
    g.humidity = NAN;
    result = CHANGED_BAR;
  }
  if (ahtFail >= cfg::Timing::AHT_FAIL_REINIT_AFTER) result |= reinitializeAht();
  return result;
}

}  // namespace

void begin() {
  pinMode(cfg::Pins::MEAS_EN, OUTPUT);
  digitalWrite(cfg::Pins::MEAS_EN, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(cfg::Pins::VBAT_ADC, ADC_11db);

  Wire.begin(cfg::Pins::I2C_SDA, cfg::Pins::I2C_SCL);
  Wire.setClock(100000);
  delay(50);
  g.ahtOk = aht.begin(&Wire);

  // Beide Messungen sofort beim ersten update() ausfuehren.
  lastVbatMs = millis() - cfg::Timing::VBAT_POLL_MS;
  lastAhtMs = millis() - cfg::Timing::AHT_POLL_MS;
}

uint8_t update() {
  return pollAht() | pollBattery();
}

const Data& data() {
  return g;
}

}  // namespace measure
