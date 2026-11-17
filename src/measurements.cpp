#include "measurements.h"

#include <Wire.h>
#include <Adafruit_AHTX0.h>

#include "config.h"
#include "battery_curve.h"
#include "battery_median.h"
#include "device_log.h"
#include "netlink.h"

namespace measure {
namespace {

Data g;
BatteryMedian batteryMedian;
Adafruit_AHTX0 aht;
uint8_t ahtFail = 0;
uint32_t lastAhtMs = 0;
uint32_t lastVbatMs = 0;
uint8_t lowBatterySamples = 0;
uint8_t batteryRecoverySamples = 0;
uint8_t batteryWarningPercent = 0;
bool batteryWarningConfigured = false;
bool batteryWarningOn = false;
bool batteryAtOrBelowThreshold = false;
bool batterySampleProcessed = false;
bool batteryValidityKnown = false;
bool batteryWasValid = true;
uint32_t lastBatteryWarningSampleId = 0;
uint32_t lowBatteryCandidateSinceMs = 0;
uint32_t lowBatteryDurationMs = 0;
uint32_t lastLowBatterySampleMs = 0;

struct BatteryReading {
  uint32_t rawAverage;
  uint32_t rawMin;
  uint32_t rawMax;
  float adcVoltage;
  float batteryVoltage;
  bool valid;
};

uint8_t vbatToLevel0to4(float voltage, uint16_t zeroMv, uint16_t fullMv) {
  if (!isfinite(voltage)) return 0;
  if (voltage >= fullMv / 1000.0f) return 4;
  voltage = battery::displayCurveVoltage(voltage, zeroMv, fullMv);
  if (voltage >= 4.00f) return 4;
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
  if ((!reading.valid && (!batteryValidityKnown || batteryWasValid)) ||
      (reading.valid && batteryValidityKnown && !batteryWasValid)) {
    device_log::addf("battery voltage %s", reading.valid ? "recovered" : "out of range");
  }
  batteryValidityKnown = true;
  batteryWasValid = reading.valid;
  return reading;
}

uint8_t pollBattery() {
  uint32_t now = millis();
  if (now - lastVbatMs < cfg::Timing::VBAT_POLL_MS) return CHANGED_NONE;
  lastVbatMs = now;

  BatteryReading reading = readBatteryVoltage();
  float newVbat = reading.valid ? reading.batteryVoltage : NAN;
  uint16_t zeroMv, fullMv;
  netlink::batteryDisplayLimits(zeroMv, fullMv);
  static uint16_t lastZeroMv = 3300, lastFullMv = 4000;
  if (zeroMv != lastZeroMv || fullMv != lastFullMv) {
    batteryMedian.clear();
    lastZeroMv = zeroMv;
    lastFullMv = fullMv;
  }
  uint8_t newLevel = vbatToLevel0to4(newVbat, zeroMv, fullMv);
  uint8_t rawPercent = reading.valid ? battery::protectionPercent(newVbat) : 0;
  uint8_t displayPercent = reading.valid ? battery::displayPercent(newVbat, zeroMv, fullMv) : 0;
  if (!reading.valid) batteryMedian.clear();
  uint8_t newPercent = reading.valid ? batteryMedian.add(displayPercent) : displayPercent;
  bool changed = isfinite(newVbat) != isfinite(g.vbat)
    || newPercent != g.vbatPercent
    || newLevel != g.vbatLevel;
#if VBAT_SCREEN_DEBUG
  changed = changed || g.rawAverage != reading.rawAverage;
#endif
  g.vbat = newVbat;
  g.vbatLevel = newLevel;
  g.vbatPercent = newPercent;
  g.vbatPercentRaw = rawPercent;
  g.vbatCalc = reading.batteryVoltage;
  g.rawAverage = reading.rawAverage;
  g.adcVoltage = reading.adcVoltage;
  ++g.vbatSampleId;

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
    Serial.printf(" level=%u percent=%u%% raw_percent=%u%%\n",
                  g.vbatLevel, g.vbatPercent, g.vbatPercentRaw);
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
  device_log::addf("AHT20 reinitialized: %s", g.ahtOk ? "ok" : "failed");
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
  if (ahtFail == 1) device_log::addf("AHT20 reading invalid");
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

uint8_t updateBatteryWarning(uint8_t warningPercent) {
  const uint32_t now = millis();
  if (!batteryWarningConfigured || warningPercent != batteryWarningPercent) {
    const bool wasActive = batteryWarningOn;
    batteryWarningConfigured = true;
    batteryWarningPercent = warningPercent;
    batteryWarningOn = false;
    batteryAtOrBelowThreshold = false;
    lowBatterySamples = 0;
    batteryRecoverySamples = 0;
    lowBatteryDurationMs = 0;
    batterySampleProcessed = true;
    lastBatteryWarningSampleId = g.vbatSampleId;
    return wasActive ? CHANGED_BATTERY : CHANGED_NONE;
  }

  if (batterySampleProcessed && g.vbatSampleId == lastBatteryWarningSampleId) return CHANGED_NONE;
  batterySampleProcessed = true;
  lastBatteryWarningSampleId = g.vbatSampleId;

  if (warningPercent == 0) return CHANGED_NONE;
  if (!isfinite(g.vbat)) {
    lowBatterySamples = 0;
    batteryRecoverySamples = 0;
    batteryAtOrBelowThreshold = false;
    lowBatteryDurationMs = 0;
    lastLowBatterySampleMs = now;
    return CHANGED_NONE;
  }

  const bool wasAtOrBelowThreshold = batteryAtOrBelowThreshold;
  batteryAtOrBelowThreshold = g.vbatPercentRaw <= warningPercent;

  if (!batteryWarningOn) {
    if (!batteryAtOrBelowThreshold) {
      lowBatterySamples = 0;
      return CHANGED_NONE;
    }

    if (lowBatterySamples == 0) lowBatteryCandidateSinceMs = now;
    if (++lowBatterySamples < cfg::Timing::LOW_BATTERY_CONFIRM_SAMPLES) return CHANGED_NONE;

    batteryWarningOn = true;
    batteryRecoverySamples = 0;
    lowBatteryDurationMs = now - lowBatteryCandidateSinceMs;
    lastLowBatterySampleMs = now;
    return CHANGED_BATTERY;
  }

  if (batteryAtOrBelowThreshold) {
    batteryRecoverySamples = 0;
    if (wasAtOrBelowThreshold) {
      lowBatteryDurationMs += now - lastLowBatterySampleMs;
    } else {
      lowBatteryDurationMs = 0;
    }
    lastLowBatterySampleMs = now;
    return CHANGED_NONE;
  }

  lowBatteryDurationMs = 0;
  lastLowBatterySampleMs = now;
  const uint16_t recoveryPercent = (uint16_t)warningPercent
    + cfg::Timing::LOW_BATTERY_RECOVERY_MARGIN_PERCENT;
  if (g.vbatPercentRaw >= recoveryPercent) {
    if (++batteryRecoverySamples >= cfg::Timing::LOW_BATTERY_RECOVERY_SAMPLES) {
      batteryWarningOn = false;
      batteryRecoverySamples = 0;
      lowBatterySamples = 0;
      return CHANGED_BATTERY;
    }
  } else {
    batteryRecoverySamples = 0;
  }
  return CHANGED_NONE;
}

bool batteryWarningActive() {
  return batteryWarningOn;
}

bool batteryShutdownDue(uint32_t shutdownAfterMs) {
  if (!batteryWarningOn || !batteryAtOrBelowThreshold || shutdownAfterMs == 0) return false;
  return lowBatteryDurationMs + millis() - lastLowBatterySampleMs >= shutdownAfterMs;
}

const Data& data() {
  return g;
}

}  // namespace measure
