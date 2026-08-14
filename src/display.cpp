#include "display.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================================================
// INISIALISASI
// =====================================================
bool displayInit() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("OLED tidak ditemukan!"));
        return false;
    }
    return true;
}

// =====================================================
// INDIKATOR KONEKSI (pojok kanan atas)
// -----------------------------------------------------
// Saat demonstrasi terlihat jelas alat tetap bekerja
// walau WiFi mati: "NET" = online, "OFF" = offline.
// =====================================================
static void drawConnIndicator(bool online) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(SCREEN_WIDTH - 18, 0);
    display.print(online ? F("NET") : F("OFF"));
}

// =====================================================
// PRIMITIF WAJAH (dari firmware sebelumnya)
// =====================================================
static void drawSmile(int cx, int baseY, int w, int depth) {
    for (int dx = -w; dx <= w; dx++) {
        int y = baseY + (depth * (w * w - dx * dx)) / (w * w);
        display.drawPixel(cx + dx, y, SSD1306_WHITE);
        display.drawPixel(cx + dx, y + 1, SSD1306_WHITE);
    }
}

static void drawFrown(int cx, int baseY, int w, int depth) {
    for (int dx = -w; dx <= w; dx++) {
        int y = baseY + (depth * dx * dx) / (w * w);
        display.drawPixel(cx + dx, y, SSD1306_WHITE);
        display.drawPixel(cx + dx, y + 1, SSD1306_WHITE);
    }
}

static void drawSparkle(int x, int y, int s) {
    display.drawFastVLine(x, y - s, 2 * s + 1, SSD1306_WHITE);
    display.drawFastHLine(x - s, y, 2 * s + 1, SSD1306_WHITE);
    display.drawPixel(x - s + 1, y - s + 1, SSD1306_WHITE);
    display.drawPixel(x + s - 1, y - s + 1, SSD1306_WHITE);
    display.drawPixel(x - s + 1, y + s - 1, SSD1306_WHITE);
    display.drawPixel(x + s - 1, y + s - 1, SSD1306_WHITE);
}

// state: 0 = sedih (TINDAKAN), 1 = datar (PERHATIAN), 2 = senyum (SEHAT)
static void drawFace(int state) {
    unsigned long t = millis();
    int cx = 64, cy = 42, r = 18;
    int ox = 0, oy = 0;

    if (state == 2)      oy = (int)round(3.0 * sin(t / 140.0));
    else if (state == 1) ox = (int)round(2.0 * sin(t / 400.0));

    int fx = cx + ox;
    int fy = cy + oy;

    display.drawCircle(fx, fy, r, SSD1306_WHITE);

    int eyeDX = 8;
    int eyeY  = fy - 6;
    int lx    = fx - eyeDX;
    int rx    = fx + eyeDX;
    bool blink = (t % 2500) < 150;

    if (state == 2) {
        // SEHAT: mata ceria + senyum + sparkle
        bool happyEye = ((t / 1500) % 2) == 0;
        if (happyEye) {
            for (int dx = -3; dx <= 3; dx++) {
                int ey = eyeY + (dx * dx) / 4;
                display.drawPixel(lx + dx, ey, SSD1306_WHITE);
                display.drawPixel(rx + dx, ey, SSD1306_WHITE);
            }
        } else {
            display.fillCircle(lx, eyeY, 3, SSD1306_WHITE);
            display.fillCircle(rx, eyeY, 3, SSD1306_WHITE);
        }
        drawSmile(fx, fy + 3, 11, 7);
        display.drawCircle(fx - 14, fy + 4, 2, SSD1306_WHITE);
        display.drawCircle(fx + 14, fy + 4, 2, SSD1306_WHITE);
        if ((t / 350) % 2 == 0) drawSparkle(cx - 30, cy - 14, 3);
        if ((t / 350) % 3 == 0) drawSparkle(cx + 32, cy - 8, 2);
        if ((t / 350) % 2 == 1) drawSparkle(cx + 28, cy + 16, 3);
    } else if (state == 1) {
        // PERHATIAN: wajah datar
        if (blink) {
            display.drawFastHLine(lx - 3, eyeY, 6, SSD1306_WHITE);
            display.drawFastHLine(rx - 3, eyeY, 6, SSD1306_WHITE);
        } else {
            display.fillCircle(lx, eyeY, 3, SSD1306_WHITE);
            display.fillCircle(rx, eyeY, 3, SSD1306_WHITE);
        }
        // mulut datar
        display.drawFastHLine(fx - 8, fy + 8, 16, SSD1306_WHITE);
    } else {
        // TINDAKAN: wajah sedih + air mata
        display.drawLine(lx - 4, eyeY - 7, lx + 4, eyeY - 4, SSD1306_WHITE);
        display.drawLine(rx + 4, eyeY - 7, rx - 4, eyeY - 4, SSD1306_WHITE);
        display.fillCircle(lx, eyeY, 3, SSD1306_WHITE);
        display.fillCircle(rx, eyeY, 3, SSD1306_WHITE);
        drawFrown(fx, fy + 6, 9, 5);
        float p = (t % 1400) / 1400.0;
        int ty = eyeY + 4 + (int)(p * 20);
        display.fillCircle(lx, ty, 2, SSD1306_WHITE);
        if (p > 0.85) {
            display.drawPixel(lx - 3, ty, SSD1306_WHITE);
            display.drawPixel(lx + 3, ty, SSD1306_WHITE);
        }
    }
}

// Baris bawah: pH, kelembapan, suhu
static void drawBottomValues(const SensorReading &s) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.drawFastHLine(0, 53, SCREEN_WIDTH, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print(F("L:"));
    display.print((int)round(s.moisture));
    display.print(F("%"));
    display.setCursor(45, 56);
    display.print(F("pH:"));
    display.print(s.ph, 1);
    display.setCursor(96, 56);
    display.print(s.temperature, 1);
    display.print(F("C"));
}

// =====================================================
// SPLASH
// =====================================================
void showSplash() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(20, 8);
    display.println(F("POTANI"));
    display.setTextSize(1);
    display.setCursor(8, 34);
    display.println(F("Soil Probe Fuzzy"));
    display.setCursor(18, 48);
    display.println(F("Kesehatan Tanah"));
    display.display();
}

// =====================================================
// IDLE — ajakan menancapkan probe
// =====================================================
void showIdle(bool online) {
    display.clearDisplay();
    drawConnIndicator(online);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("POTANI"));

    display.setTextSize(1);
    display.setCursor(10, 22);
    display.println(F("Tancapkan probe"));
    display.setCursor(10, 34);
    display.println(F("ke media tanam."));

    // Kedip ajakan tekan tombol
    if ((millis() / 600) % 2 == 0) {
        display.setCursor(6, 50);
        display.println(F("> Tekan untuk ukur"));
    }
    display.display();
}

// =====================================================
// READING — indikator proses baca sensor
// =====================================================
void showReading(int progressPct, bool online) {
    display.clearDisplay();
    drawConnIndicator(online);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("MEMBACA SENSOR"));

    display.setCursor(20, 26);
    display.setTextSize(1);
    display.println(F("Menahan probe..."));

    // bar progres
    display.drawRect(14, 42, 100, 10, SSD1306_WHITE);
    int w = (progressPct * 96) / 100;
    if (w > 0) display.fillRect(16, 44, w, 6, SSD1306_WHITE);
    display.display();
}

// =====================================================
// STABILIZING — menunggu pembacaan stabil
// =====================================================
void showStabilizing(int progressPct, float phSpread, bool online) {
    display.clearDisplay();
    drawConnIndicator(online);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("STABILISASI"));

    display.setCursor(0, 22);
    display.print(F("Selisih pH: "));
    display.print(phSpread, 3);

    display.drawRect(14, 42, 100, 10, SSD1306_WHITE);
    int w = (progressPct * 96) / 100;
    if (w > 0) display.fillRect(16, 44, w, 6, SSD1306_WHITE);
    display.display();
}

// =====================================================
// HASIL — status, skor, wajah, nilai sensor
// =====================================================
void showResult(const SensorReading &s, const FuzzyResult &f, bool stable, bool online) {
    display.clearDisplay();
    drawConnIndicator(online);

    // Baris atas: label status + skor
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(statusLabel(f.status));

    display.setCursor(0, 10);
    display.print(F("Skor: "));
    display.print((int)round(f.score));
    if (!stable) display.print(F(" (?)"));  // penanda belum stabil
    display.drawFastHLine(0, 20, SCREEN_WIDTH, SSD1306_WHITE);

    // Area tengah: wajah sesuai status
    drawFace(f.faceState);

    // Baris bawah: pH, kelembapan, suhu
    drawBottomValues(s);

    display.display();
}

// =====================================================
// ERROR — sensor tidak terdeteksi
// =====================================================
void showError(const char *msg, bool online) {
    display.clearDisplay();
    drawConnIndicator(online);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 6);
    display.println(F("ERROR"));
    display.setTextSize(1);
    display.setCursor(0, 34);
    display.println(msg);
    display.display();
}
