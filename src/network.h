#ifndef NETWORK_H
#define NETWORK_H

// =====================================================
// MODUL JARINGAN — WiFi, MQTT (HiveMQ TLS), NTP, antrean
// =====================================================

#include <Arduino.h>
#include "sensors.h"
#include "fuzzy.h"

void  networkInit();
void  networkLoop();          // panggil rutin: jaga koneksi & flush antrean

bool  wifiConnected();
bool  mqttConnected();
bool  isOnline();             // WiFi + MQTT siap

void  syncTimeNTP();
bool  getTimestamp(char *buf, size_t len);  // false jika NTP belum sinkron

// Bangun payload JSON (dipakai publish & antrean). Mengembalikan panjang.
void  buildPayload(String &out, const SensorReading &s, const FuzzyResult &f, bool stable);

// Publish hasil. Jika gagal / offline -> masuk antrean melingkar.
// Mengembalikan true bila terkirim langsung.
bool  publishReading(const SensorReading &s, const FuzzyResult &f, bool stable);

int   queuedCount();

#endif // NETWORK_H
