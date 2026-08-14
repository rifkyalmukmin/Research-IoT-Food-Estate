#include <Arduino.h>

#include "config.h"
#include "sensors.h"
#include "fuzzy.h"
#include "display.h"
#include "network.h"

// =====================================================
// POTANI — soil probe genggam berbasis sesi
// -----------------------------------------------------
// Alur: IDLE -> READING -> STABILIZING -> INFERENCE ->
//       DISPLAY -> PUBLISH -> IDLE
// Fuzzy dijalankan di perangkat, jadi hasil tetap tampil
// di OLED walau tanpa jaringan.
// =====================================================

enum State {
    S_IDLE,
    S_READING,
    S_STABILIZING,
    S_INFERENCE,
    S_DISPLAY,
    S_PUBLISH,
    S_ERROR
};

static State state = S_IDLE;

static SensorReading lastReading;
static FuzzyResult   lastResult;
static bool          lastStable = true;

// -------- buffer kestabilan pH (5 pembacaan terakhir) --------
static float phBuf[STABILITY_WINDOW];
static int   phBufIdx   = 0;
static int   phBufCount = 0;

static unsigned long readingStart = 0;   // awal sesi baca
static unsigned long stableSince  = 0;   // saat selisih pertama < ambang
static unsigned long lastSampleAt = 0;
static unsigned long displayStart = 0;

static void resetStability() {
    phBufIdx = phBufCount = 0;
    stableSince = 0;
}

static void pushPH(float v) {
    phBuf[phBufIdx] = v;
    phBufIdx = (phBufIdx + 1) % STABILITY_WINDOW;
    if (phBufCount < STABILITY_WINDOW) phBufCount++;
}

// selisih max-min dari buffer; besar bila belum penuh
static float phSpread() {
    if (phBufCount < STABILITY_WINDOW) return 999.0f;
    float mn = phBuf[0], mx = phBuf[0];
    for (int i = 1; i < phBufCount; i++) {
        if (phBuf[i] < mn) mn = phBuf[i];
        if (phBuf[i] > mx) mx = phBuf[i];
    }
    return mx - mn;
}

// =====================================================
// PERINTAH SERIAL (pemicu sementara pengganti tombol)
// -----------------------------------------------------
// "measure"  -> mulai sesi pengukuran
// "selftest" -> jalankan verifikasi fuzzy (CSV)
// TODO: ganti dengan tombol fisik GPIO<n>
// =====================================================
static String serialLine;

static String pollCommand() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            String cmd = serialLine;
            serialLine = "";
            cmd.trim();
            cmd.toLowerCase();
            if (cmd.length() > 0) return cmd;
        } else {
            serialLine += c;
        }
    }
    return "";
}

static bool triggerPressed(const String &cmd) {
    // tombol fisik belum ada -> pakai "measure" via serial
    return cmd == "measure";
}

static void printHelp() {
    Serial.println();
    Serial.println(F("=========== POTANI READY ==========="));
    Serial.println(F("Perintah serial:"));
    Serial.println(F("  measure   -> mulai pengukuran"));
    Serial.println(F("  selftest  -> verifikasi fuzzy (CSV)"));
    Serial.println(F("===================================="));
}

// =====================================================
// SETUP
// =====================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    sensorsInit();

    if (!displayInit()) {
        Serial.println(F("OLED gagal. Berhenti."));
        while (true) delay(100);
    }

    showSplash();
    delay(2000);

    // Jaringan dibangun di awal; kalau gagal, alat tetap jalan offline.
    networkInit();

    printHelp();
    state = S_IDLE;
}

// =====================================================
// LOOP — state machine
// =====================================================
void loop() {
    networkLoop();  // jaga WiFi/MQTT & kirim ulang antrean

    String cmd = pollCommand();

    // selftest bisa dipanggil kapan saja saat idle
    if (cmd == "selftest") {
        runFuzzySelfTest();
    }

    unsigned long now = millis();

    switch (state) {

        // -------------------------------------------------
        case S_IDLE:
            showIdle(isOnline());
            if (triggerPressed(cmd)) {
                Serial.println(F("Mulai pengukuran..."));
                resetStability();
                readingStart = now;
                lastSampleAt = 0;
                state = S_READING;
            }
            break;

        // -------------------------------------------------
        case S_READING: {
            // Pembacaan awal lengkap + deteksi sensor.
            lastReading = readAllSensors();
            if (!lastReading.tempOk) {
                state = S_ERROR;
                break;
            }
            resetStability();
            pushPH(lastReading.ph);
            readingStart = now;
            lastSampleAt = now;
            showReading(20, isOnline());
            state = S_STABILIZING;
            break;
        }

        // -------------------------------------------------
        case S_STABILIZING: {
            if (now - lastSampleAt >= READING_SAMPLE_MS) {
                lastSampleAt = now;
                pushPH(readPHValue());
            }

            float spread  = phSpread();
            unsigned long elapsed = now - readingStart;

            // Deteksi kestabilan: selisih 5 pembacaan < 0.1 pH
            // selama minimal 3 detik.
            bool withinBand = (spread < STABILITY_THRESHOLD);
            if (withinBand) {
                if (stableSince == 0) stableSince = now;
            } else {
                stableSince = 0;
            }

            int progress;
            if (stableSince > 0) {
                progress = (int)((now - stableSince) * 100UL / STABILITY_MIN_MS);
            } else {
                progress = (int)(elapsed * 100UL / STABILITY_TIMEOUT_MS);
            }
            if (progress > 100) progress = 100;
            if (progress < 0)   progress = 0;
            showStabilizing(progress, (spread > 90 ? 0.0f : spread), isOnline());

            bool stableReached =
                (stableSince > 0) && (now - stableSince >= STABILITY_MIN_MS);
            bool timedOut = (elapsed >= STABILITY_TIMEOUT_MS);

            if (stableReached || timedOut) {
                lastStable = stableReached;  // false bila mentok timeout
                // Pembacaan akhir lengkap agar ketiga nilai sinkron.
                lastReading = readAllSensors();
                if (!lastReading.tempOk) {
                    state = S_ERROR;
                    break;
                }
                state = S_INFERENCE;
            }
            break;
        }

        // -------------------------------------------------
        case S_INFERENCE: {
            lastResult = fuzzyInfer(lastReading.moisture,
                                    lastReading.ph,
                                    lastReading.temperature);

            Serial.println(F("---- HASIL FUZZY ----"));
            Serial.print(F("Kelembapan : ")); Serial.print(lastReading.moisture, 1); Serial.println(F(" %"));
            Serial.print(F("pH         : ")); Serial.print(lastReading.ph, 2);
            Serial.println(lastReading.phValid ? F("") : F(" (di luar rentang, nilai lama)"));
            Serial.print(F("Suhu       : ")); Serial.print(lastReading.temperature, 2); Serial.println(F(" C"));
            Serial.print(F("Aturan aktif: ")); Serial.println(lastResult.activeRules);
            Serial.print(F("Skor       : ")); Serial.println(lastResult.score, 2);
            Serial.print(F("Status     : ")); Serial.println(statusLabel(lastResult.status));
            Serial.print(F("Stabil     : ")); Serial.println(lastStable ? F("ya") : F("tidak"));

            displayStart = now;
            state = S_DISPLAY;
            break;
        }

        // -------------------------------------------------
        case S_DISPLAY:
            showResult(lastReading, lastResult, lastStable, isOnline());
            // tahan 15 detik atau sampai tombol/measure ditekan
            if (now - displayStart >= DISPLAY_HOLD_MS || triggerPressed(cmd)) {
                state = S_PUBLISH;
            }
            break;

        // -------------------------------------------------
        case S_PUBLISH:
            publishReading(lastReading, lastResult, lastStable);
            Serial.println(F("Kembali ke IDLE."));
            state = S_IDLE;
            break;

        // -------------------------------------------------
        case S_ERROR:
            showError("Sensor tidak\nterdeteksi.\nCek DS18B20.", isOnline());
            // tekan measure untuk coba lagi
            if (triggerPressed(cmd)) {
                state = S_IDLE;
            }
            break;
    }

    delay(30);  // ~30 FPS untuk animasi wajah
}
