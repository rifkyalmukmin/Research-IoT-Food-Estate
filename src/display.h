#ifndef DISPLAY_H
#define DISPLAY_H

// =====================================================
// MODUL OLED — penggambaran layar per state
// =====================================================

#include <Arduino.h>
#include "sensors.h"
#include "fuzzy.h"

bool displayInit();

void showSplash();
void showIdle(bool online);
void showReading(int progressPct, bool online);
void showStabilizing(int progressPct, float phSpread, bool online);
void showResult(const SensorReading &s, const FuzzyResult &f, bool stable, bool online);
void showError(const char *msg, bool online);

#endif // DISPLAY_H
