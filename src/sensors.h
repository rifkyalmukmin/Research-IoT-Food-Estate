#ifndef SENSORS_H
#define SENSORS_H

// =====================================================
// MODUL SENSOR — pembacaan & konversi tiga sensor
//   - Kelembapan (soil moisture, ADC)
//   - pH (ADC -> tegangan -> pH)
//   - Suhu (DS18B20)
// =====================================================

#include <Arduino.h>

struct SensorReading {
    int   soilADC;
    float moisture;      // persen 0..100
    int   phADC;
    float phVoltage;     // Volt (analogReadMilliVolts terkalibrasi)
    float ph;            // 0..14 (nilai valid terakhir bila di luar rentang)
    bool  phValid;       // false bila tegangan/pH di luar rentang wajar
    float temperature;   // derajat C
    bool  tempOk;        // false jika DS18B20 tidak terdeteksi
};

void  sensorsInit();

int   readSoilRaw();               // rata-rata SOIL_SAMPLES pembacaan
int   readPHRaw();                 // rata-rata PH_SAMPLES pembacaan (ADC mentah)
float readPHVoltage();             // rata-rata PH_SAMPLES analogReadMilliVolts -> Volt

float convertMoisture(int adc);          // ADC -> persen
float phFromVoltage(float voltage, bool &valid);  // Volt -> pH (+ flag valid)
float readPHValue();               // baca cepat + konversi (untuk uji kestabilan)

SensorReading readAllSensors();

#endif // SENSORS_H
