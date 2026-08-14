#include "sensors.h"
#include "config.h"

#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire oneWire(DS18B20_PIN);
static DallasTemperature temperatureSensor(&oneWire);

// =====================================================
// INISIALISASI ADC + DS18B20
// =====================================================
void sensorsInit() {
    analogReadResolution(12);
    analogSetPinAttenuation(SOIL_PIN, ADC_11db);
    analogSetPinAttenuation(PH_PIN, ADC_11db);
    temperatureSensor.begin();
}

// =====================================================
// BACA SOIL MOISTURE (10x, jeda 3 ms) — JANGAN DIUBAH
// =====================================================
int readSoilRaw() {
    long sum = 0;
    for (int i = 0; i < SOIL_SAMPLES; i++) {
        sum += analogRead(SOIL_PIN);
        delay(SOIL_SAMPLE_MS);
    }
    return sum / SOIL_SAMPLES;
}

// =====================================================
// BACA pH (20x, jeda 2 ms) — JANGAN DIUBAH
// =====================================================
int readPHRaw() {
    long sum = 0;
    for (int i = 0; i < PH_SAMPLES; i++) {
        sum += analogRead(PH_PIN);
        delay(PH_SAMPLE_MS);
    }
    return (int)(sum / PH_SAMPLES);
}

// Baca tegangan pH memakai pembacaan millivolt terkalibrasi ESP32.
// Lebih aman daripada mengasumsikan ADC selalu linear 0-3.3 V.
float readPHVoltage() {
    long sum = 0;
    for (int i = 0; i < PH_SAMPLES; i++) {
        sum += analogReadMilliVolts(PH_PIN);
        delay(PH_SAMPLE_MS);
    }
    return (sum / (float)PH_SAMPLES) / 1000.0f;  // mV -> V
}

// =====================================================
// KONVERSI
// =====================================================
float convertMoisture(int adc) {
    // KERING (SOIL_DRY) -> 0%, BASAH (SOIL_WET) -> 100%
    float pct = (float)(SOIL_DRY - adc) * 100.0f / (float)(SOIL_DRY - SOIL_WET);
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

// Nilai pH valid terakhir; dipertahankan saat pembacaan di luar rentang.
static float lastValidPH = 7.0f;

// Konversi tegangan -> pH. Bila tegangan atau pH di luar rentang wajar,
// TIDAK dipaksa ke 0 melainkan nilai valid terakhir dipertahankan.
float phFromVoltage(float voltage, bool &valid) {
    float calculated = 7.0f + (V_NETRAL - voltage) / SLOPE;

    if (voltage < PH_MIN_VALID_VOLTAGE ||
        voltage > PH_MAX_VALID_VOLTAGE ||
        calculated < 0.0f ||
        calculated > 14.0f) {
        valid = false;
        return lastValidPH;
    }

    valid = true;
    lastValidPH = calculated;
    return calculated;
}

float readPHValue() {
    bool valid;
    return phFromVoltage(readPHVoltage(), valid);
}

// =====================================================
// BACA SEMUA SENSOR
// =====================================================
SensorReading readAllSensors() {
    SensorReading r;

    r.soilADC  = readSoilRaw();
    r.moisture = convertMoisture(r.soilADC);

    r.phADC     = readPHRaw();            // ADC mentah (untuk field ph_adc)
    r.phVoltage = readPHVoltage();        // Volt terkalibrasi
    r.ph        = phFromVoltage(r.phVoltage, r.phValid);

    temperatureSensor.requestTemperatures();
    float temp = temperatureSensor.getTempCByIndex(0);
    if (temp != DEVICE_DISCONNECTED_C) {
        r.temperature = temp;
        r.tempOk      = true;
    } else {
        r.temperature = 0.0f;
        r.tempOk      = false;
        Serial.println(F("WARNING: DS18B20 tidak terdeteksi!"));
    }

    return r;
}
