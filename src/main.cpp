#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// ==========================
// KONFIGURASI OLED
// ==========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);

// ==========================
// KONFIGURASI SENSOR KELEMBAPAN TANAH
// ==========================
const int soilPin = 34;

// Kalibrasi sensor kapasitif
// Kering -> ADC besar
// Basah  -> ADC kecil
const int dryValue = 4095;
const int wetValue = 1500;

// ==========================
// KONFIGURASI DS18B20
// ==========================
#define DS18B20_PIN 5

OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);

// ==========================
// VARIABEL SOIL MOISTURE
// ==========================
int sensorValue = 0;
int moisturePercent = 0;

// 0 = kering
// 1 = lembab
// 2 = basah
int faceState = 1;

String status = "";

// ==========================
// VARIABEL TEMPERATUR
// ==========================
float temperatureC = 0.0;

unsigned long lastSensorRead = 0;

// ==========================
// BACA SOIL MOISTURE
// ==========================
int readSoilRaw()
{
    long sum = 0;

    for (int i = 0; i < 10; i++)
    {
        sum += analogRead(soilPin);
        delay(3);
    }

    return sum / 10;
}

// ==========================
// GAMBAR SENYUM
// ==========================
void drawSmile(int cx, int baseY, int w, int depth)
{
    for (int dx = -w; dx <= w; dx++)
    {
        int y = baseY +
                (depth * (w * w - dx * dx)) /
                (w * w);

        display.drawPixel(
            cx + dx,
            y,
            SSD1306_WHITE
        );

        display.drawPixel(
            cx + dx,
            y + 1,
            SSD1306_WHITE
        );
    }
}

// ==========================
// GAMBAR WAJAH SEDIH
// ==========================
void drawFrown(int cx, int baseY, int w, int depth)
{
    for (int dx = -w; dx <= w; dx++)
    {
        int y = baseY +
                (depth * dx * dx) /
                (w * w);

        display.drawPixel(
            cx + dx,
            y,
            SSD1306_WHITE
        );

        display.drawPixel(
            cx + dx,
            y + 1,
            SSD1306_WHITE
        );
    }
}

// ==========================
// SPARKLE
// ==========================
void drawSparkle(int x, int y, int s)
{
    display.drawFastVLine(
        x,
        y - s,
        2 * s + 1,
        SSD1306_WHITE
    );

    display.drawFastHLine(
        x - s,
        y,
        2 * s + 1,
        SSD1306_WHITE
    );

    display.drawPixel(
        x - s + 1,
        y - s + 1,
        SSD1306_WHITE
    );

    display.drawPixel(
        x + s - 1,
        y - s + 1,
        SSD1306_WHITE
    );

    display.drawPixel(
        x - s + 1,
        y + s - 1,
        SSD1306_WHITE
    );

    display.drawPixel(
        x + s - 1,
        y + s - 1,
        SSD1306_WHITE
    );
}

// ==========================
// GAMBAR WAJAH ANIMASI
// ==========================
void drawFace(int state)
{
    unsigned long t = millis();

    int cx = 64;
    int cy = 42;
    int r = 20;

    // ==========================
    // GERAKAN WAJAH
    // ==========================
    int ox = 0;
    int oy = 0;

    if (state == 2)
    {
        // Basah -> memantul
        oy = (int)round(
            3.0 * sin(t / 140.0)
        );
    }
    else if (state == 1)
    {
        // Lembab -> bergoyang
        ox = (int)round(
            2.0 * sin(t / 400.0)
        );
    }

    int fx = cx + ox;
    int fy = cy + oy;

    // ==========================
    // WAJAH
    // ==========================
    display.drawCircle(
        fx,
        fy,
        r,
        SSD1306_WHITE
    );

    int eyeDX = 8;

    int eyeY = fy - 6;

    int lx = fx - eyeDX;
    int rx = fx + eyeDX;

    // Kedip
    bool blink = (t % 2500) < 150;

    // ==========================
    // BASAH
    // ==========================
    if (state == 2)
    {
        bool happyEye =
            ((t / 1500) % 2) == 0;

        if (happyEye)
        {
            for (int dx = -3; dx <= 3; dx++)
            {
                int ey =
                    eyeY + (dx * dx) / 4;

                display.drawPixel(
                    lx + dx,
                    ey,
                    SSD1306_WHITE
                );

                display.drawPixel(
                    rx + dx,
                    ey,
                    SSD1306_WHITE
                );
            }
        }
        else
        {
            display.fillCircle(
                lx,
                eyeY,
                3,
                SSD1306_WHITE
            );

            display.fillCircle(
                rx,
                eyeY,
                3,
                SSD1306_WHITE
            );
        }

        // Senyum
        drawSmile(
            fx,
            fy + 3,
            11,
            7
        );

        // Pipi
        display.drawCircle(
            fx - 14,
            fy + 4,
            2,
            SSD1306_WHITE
        );

        display.drawCircle(
            fx + 14,
            fy + 4,
            2,
            SSD1306_WHITE
        );

        // Sparkle
        if ((t / 350) % 2 == 0)
        {
            drawSparkle(
                cx - 30,
                cy - 14,
                3
            );
        }

        if ((t / 350) % 3 == 0)
        {
            drawSparkle(
                cx + 32,
                cy - 8,
                2
            );
        }

        if ((t / 350) % 2 == 1)
        {
            drawSparkle(
                cx + 28,
                cy + 16,
                3
            );
        }
    }

    // ==========================
    // LEMBAB
    // ==========================
    else if (state == 1)
    {
        if (blink)
        {
            display.drawFastHLine(
                lx - 3,
                eyeY,
                6,
                SSD1306_WHITE
            );

            display.drawFastHLine(
                rx - 3,
                eyeY,
                6,
                SSD1306_WHITE
            );
        }
        else
        {
            display.fillCircle(
                lx,
                eyeY,
                3,
                SSD1306_WHITE
            );

            display.fillCircle(
                rx,
                eyeY,
                3,
                SSD1306_WHITE
            );
        }

        // Senyum tipis
        drawSmile(
            fx,
            fy + 6,
            8,
            3
        );
    }

    // ==========================
    // KERING
    // ==========================
    else
    {
        // Alis khawatir
        display.drawLine(
            lx - 4,
            eyeY - 7,
            lx + 4,
            eyeY - 4,
            SSD1306_WHITE
        );

        display.drawLine(
            rx + 4,
            eyeY - 7,
            rx - 4,
            eyeY - 4,
            SSD1306_WHITE
        );

        // Mata
        display.fillCircle(
            lx,
            eyeY,
            3,
            SSD1306_WHITE
        );

        display.fillCircle(
            rx,
            eyeY,
            3,
            SSD1306_WHITE
        );

        // Mulut sedih
        drawFrown(
            fx,
            fy + 6,
            9,
            5
        );

        // Air mata
        float p =
            (t % 1400) / 1400.0;

        int ty =
            eyeY +
            4 +
            (int)(p * 20);

        display.fillCircle(
            lx,
            ty,
            2,
            SSD1306_WHITE
        );

        if (p > 0.85)
        {
            display.drawPixel(
                lx - 3,
                ty,
                SSD1306_WHITE
            );

            display.drawPixel(
                lx + 3,
                ty,
                SSD1306_WHITE
            );
        }
    }
}

// ==========================
// HEADER OLED
// ==========================
void drawHeader()
{
    display.setTextColor(
        SSD1306_WHITE
    );

    display.setTextSize(1);

    // Contoh:
    // KERING 20%  28.5C
    String line =
        status +
        " " +
        String(moisturePercent) +
        "% " +
        String(temperatureC, 1) +
        "C";

    int w = line.length() * 6;

    int x =
        (SCREEN_WIDTH - w) / 2;

    if (x < 0)
        x = 0;

    display.setCursor(x, 0);

    display.print(line);

    display.drawFastHLine(
        0,
        10,
        SCREEN_WIDTH,
        SSD1306_WHITE
    );
}

// ==========================
// SETUP
// ==========================
void setup()
{
    Serial.begin(115200);

    // ==========================
    // ADC ESP32
    // ==========================
    analogReadResolution(12);

    analogSetPinAttenuation(
        soilPin,
        ADC_11db
    );

    // ==========================
    // OLED
    // ==========================
    Wire.begin(
        OLED_SDA,
        OLED_SCL
    );

    if (!display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDR))
    {
        Serial.println(
            "OLED tidak ditemukan!"
        );

        while (true)
        {
            delay(100);
        }
    }

    // ==========================
    // DS18B20
    // ==========================
    temperatureSensor.begin();

    // ==========================
    // SPLASH SCREEN
    // ==========================
    display.clearDisplay();

    display.setTextColor(
        SSD1306_WHITE
    );

    display.setTextSize(2);

    display.setCursor(4, 8);
    display.println("ANIMASI");

    display.setCursor(40, 30);
    display.println("v3");

    display.setTextSize(1);

    display.setCursor(24, 52);
    display.println("Soil + Temp");

    display.display();

    delay(2500);

    Serial.println(
        "================================="
    );

    Serial.println(
        "Monitoring Tanah + Temperatur"
    );

    Serial.println(
        "ESP32 + OLED + Soil Moisture"
    );

    Serial.println(
        "+ DS18B20 + Wajah Animasi"
    );

    Serial.println(
        "================================="
    );
}

// ==========================
// LOOP
// ==========================
void loop()
{
    unsigned long now = millis();

    // ==========================
    // BACA SENSOR SETIAP 2 DETIK
    // ==========================
    if (now - lastSensorRead >= 2000)
    {
        lastSensorRead = now;

        // ==========================
        // SOIL MOISTURE
        // ==========================
        sensorValue = readSoilRaw();

        moisturePercent =
            map(
                sensorValue,
                dryValue,
                wetValue,
                0,
                100
            );

        moisturePercent =
            constrain(
                moisturePercent,
                0,
                100
            );

        // ==========================
        // STATUS TANAH
        // ==========================
        if (moisturePercent < 30)
        {
            status = "KERING";
            faceState = 0;
        }
        else if (moisturePercent < 70)
        {
            status = "LEMBAB";
            faceState = 1;
        }
        else
        {
            status = "BASAH";
            faceState = 2;
        }

        // ==========================
        // BACA DS18B20
        // ==========================
        temperatureSensor.requestTemperatures();

        float temp =
            temperatureSensor.getTempCByIndex(0);

        // Cek apakah sensor valid
        if (temp != DEVICE_DISCONNECTED_C)
        {
            temperatureC = temp;
        }
        else
        {
            Serial.println(
                "ERROR: DS18B20 tidak terdeteksi!"
            );
        }

        // ==========================
        // SERIAL MONITOR
        // ==========================
        Serial.println(
            "----------------------------"
        );

        Serial.print("ADC     : ");
        Serial.println(sensorValue);

        Serial.print("Moisture: ");
        Serial.print(moisturePercent);
        Serial.println("%");

        Serial.print("Status  : ");
        Serial.println(status);

        Serial.print("Suhu    : ");

        if (temp != DEVICE_DISCONNECTED_C)
        {
            Serial.print(
                temperatureC,
                2
            );

            Serial.println(" °C");
        }
        else
        {
            Serial.println(
                "Sensor error"
            );
        }
    }

    // ==========================
    // GAMBAR OLED
    // ==========================
    display.clearDisplay();

    drawHeader();

    drawFace(faceState);

    display.display();

    // ~25 FPS
    delay(40);
}