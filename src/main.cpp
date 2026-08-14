#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <math.h>

// =====================================================
// OLED
// =====================================================

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

// =====================================================
// SOIL MOISTURE
// =====================================================

const int SOIL_PIN = 35;

// Kalibrasi
// Kering -> ADC besar
// Basah  -> ADC kecil

const int dryValue = 4095;
const int wetValue = 1500;

// =====================================================
// SENSOR pH
// =====================================================

const int PH_PIN = 27;

// =====================================================
// KALIBRASI pH SEMENTARA
// =====================================================

// Berdasarkan pembacaan sensor kamu:
// sekitar 0.400 V
//
// Untuk sementara kita anggap:
// 0.400 V = pH 7

const float PH_NEUTRAL_VOLTAGE = 0.400;

// Sensitivitas sementara.
// Nanti bisa dikalibrasi menggunakan buffer pH.
const float PH_SLOPE = 0.180;

// =====================================================
// DS18B20
// =====================================================

#define DS18B20_PIN 5

OneWire oneWire(
    DS18B20_PIN
);

DallasTemperature temperatureSensor(
    &oneWire
);

// =====================================================
// VARIABEL SENSOR
// =====================================================

// Soil
int sensorValue = 0;
int moisturePercent = 0;

// pH
int phADC = 0;
float phVoltage = 0.0;
float pH = 7.0;

// Temperature
float temperatureC = 0.0;

// Status tanah
String status = "";

// 0 = Kering
// 1 = Lembab
// 2 = Basah
int faceState = 1;

// =====================================================
// TIMER
// =====================================================

unsigned long lastSensorRead = 0;

const unsigned long SENSOR_INTERVAL = 2000;

// =====================================================
// BACA SOIL MOISTURE
// =====================================================

int readSoilRaw()
{
    long sum = 0;

    for (int i = 0; i < 10; i++)
    {
        sum += analogRead(
            SOIL_PIN
        );

        delay(3);
    }

    return sum / 10;
}

// =====================================================
// BACA pH
// =====================================================

int readPHRaw()
{
    long sum = 0;

    for (int i = 0; i < 20; i++)
    {
        sum += analogRead(
            PH_PIN
        );

        delay(2);
    }

    return sum / 20;
}

// =====================================================
// BACA SEMUA SENSOR
// =====================================================

void readSensors()
{
    // =================================================
    // SOIL MOISTURE
    // =================================================

    sensorValue =
        readSoilRaw();

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

    // =================================================
    // STATUS TANAH
    // =================================================

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

    // =================================================
    // pH
    // =================================================

    phADC =
        readPHRaw();

    // Konversi ADC ke voltage
    phVoltage =
        phADC *
        (3.3 / 4095.0);

    // =================================================
    // HITUNG pH
    // =================================================

    pH =
        7.0 +
        (
            (
                PH_NEUTRAL_VOLTAGE -
                phVoltage
            )
            /
            PH_SLOPE
        );

    // Batasi pH antara 0 - 14
    pH =
        constrain(
            pH,
            0.0,
            14.0
        );

    // =================================================
    // DS18B20
    // =================================================

    temperatureSensor
        .requestTemperatures();

    float temp =
        temperatureSensor
            .getTempCByIndex(0);

    if (
        temp !=
        DEVICE_DISCONNECTED_C
    )
    {
        temperatureC = temp;
    }
    else
    {
        Serial.println(
            "WARNING: DS18B20 tidak terdeteksi!"
        );
    }
}

// =====================================================
// GAMBAR SENYUM
// =====================================================

void drawSmile(
    int cx,
    int baseY,
    int w,
    int depth
)
{
    for (
        int dx = -w;
        dx <= w;
        dx++
    )
    {
        int y =
            baseY +
            (
                depth *
                (
                    w * w -
                    dx * dx
                )
            )
            /
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

// =====================================================
// GAMBAR SEDIH
// =====================================================

void drawFrown(
    int cx,
    int baseY,
    int w,
    int depth
)
{
    for (
        int dx = -w;
        dx <= w;
        dx++
    )
    {
        int y =
            baseY +
            (
                depth *
                dx *
                dx
            )
            /
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

// =====================================================
// SPARKLE
// =====================================================

void drawSparkle(
    int x,
    int y,
    int s
)
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

// =====================================================
// WAJAH ANIMASI
// =====================================================

void drawFace(int state)
{
    unsigned long t = millis();

    int cx = 64;
    int cy = 42;
    int r = 20;

    int ox = 0;
    int oy = 0;

    // Basah -> memantul
    if (state == 2)
    {
        oy =
            (int)round(
                3.0 *
                sin(
                    t / 140.0
                )
            );
    }

    // Lembab -> bergoyang
    else if (state == 1)
    {
        ox =
            (int)round(
                2.0 *
                sin(
                    t / 400.0
                )
            );
    }

    int fx = cx + ox;
    int fy = cy + oy;

    // Wajah
    display.drawCircle(
        fx,
        fy,
        r,
        SSD1306_WHITE
    );

    int eyeDX = 8;

    int eyeY =
        fy - 6;

    int lx =
        fx - eyeDX;

    int rx =
        fx + eyeDX;

    bool blink =
        (t % 2500) < 150;

    // =================================================
    // BASAH
    // =================================================

    if (state == 2)
    {
        bool happyEye =
            ((t / 1500) % 2) == 0;

        if (happyEye)
        {
            for (
                int dx = -3;
                dx <= 3;
                dx++
            )
            {
                int ey =
                    eyeY +
                    (dx * dx) / 4;

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
        if (
            (t / 350) % 2 == 0
        )
        {
            drawSparkle(
                cx - 30,
                cy - 14,
                3
            );
        }

        if (
            (t / 350) % 3 == 0
        )
        {
            drawSparkle(
                cx + 32,
                cy - 8,
                2
            );
        }

        if (
            (t / 350) % 2 == 1
        )
        {
            drawSparkle(
                cx + 28,
                cy + 16,
                3
            );
        }
    }

    // =================================================
    // LEMBAB
    // =================================================

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

        drawSmile(
            fx,
            fy + 6,
            8,
            3
        );
    }

    // =================================================
    // KERING
    // =================================================

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
            (t % 1400) /
            1400.0;

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

// =====================================================
// HEADER OLED
// =====================================================

void drawHeader()
{
    display.setTextColor(
        SSD1306_WHITE
    );

    display.setTextSize(1);

    // Baris atas:
    // L:36% pH:7.01 23.5C

    display.setCursor(
        0,
        0
    );

    display.print("L:");

    display.print(
        moisturePercent
    );

    display.print("%");

    // pH
    display.setCursor(
        45,
        0
    );

    display.print("pH:");

    display.print(
        pH,
        2
    );

    // Suhu
    display.setCursor(
        96,
        0
    );

    display.print(
        temperatureC,
        1
    );

    display.print("C");

    // Garis pembatas
    display.drawFastHLine(
        0,
        10,
        SCREEN_WIDTH,
        SSD1306_WHITE
    );
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    // =================================================
    // ADC
    // =================================================

    analogReadResolution(12);

    analogSetPinAttenuation(
        SOIL_PIN,
        ADC_11db
    );

    analogSetPinAttenuation(
        PH_PIN,
        ADC_11db
    );

    // =================================================
    // OLED
    // =================================================

    Wire.begin(
        OLED_SDA,
        OLED_SCL
    );

    if (
        !display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDR
        )
    )
    {
        Serial.println(
            "OLED tidak ditemukan!"
        );

        while (true)
        {
            delay(100);
        }
    }

    // =================================================
    // DS18B20
    // =================================================

    temperatureSensor.begin();

    // =================================================
    // SPLASH
    // =================================================

    display.clearDisplay();

    display.setTextColor(
        SSD1306_WHITE
    );

    display.setTextSize(2);

    display.setCursor(
        4,
        8
    );

    display.println(
        "ANIMASI"
    );

    display.setCursor(
        40,
        30
    );

    display.println(
        "v5"
    );

    display.setTextSize(1);

    display.setCursor(
        15,
        52
    );

    display.println(
        "Soil + pH + Temp"
    );

    display.display();

    delay(2500);

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "SMART FARM MONITORING"
    );

    Serial.println(
        "ESP32 + Soil + pH + DS18B20"
    );

    Serial.println(
        "OLED"
    );

    Serial.println(
        "================================"
    );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    unsigned long now =
        millis();

    // =================================================
    // BACA SENSOR SETIAP 2 DETIK
    // =================================================

    if (
        now - lastSensorRead >=
        SENSOR_INTERVAL
    )
    {
        lastSensorRead = now;

        readSensors();

        // =================================================
        // SERIAL MONITOR
        // =================================================

        Serial.println();

        Serial.println(
            "========== SENSOR DATA =========="
        );

        Serial.print(
            "Soil ADC       : "
        );

        Serial.println(
            sensorValue
        );

        Serial.print(
            "Kelembapan     : "
        );

        Serial.print(
            moisturePercent
        );

        Serial.println("%");

        Serial.print(
            "Status         : "
        );

        Serial.println(
            status
        );

        Serial.print(
            "pH ADC         : "
        );

        Serial.println(
            phADC
        );

        Serial.print(
            "pH Voltage     : "
        );

        Serial.print(
            phVoltage,
            3
        );

        Serial.println(" V");

        Serial.print(
            "pH             : "
        );

        Serial.println(
            pH,
            2
        );

        Serial.print(
            "Suhu DS18B20   : "
        );

        Serial.print(
            temperatureC,
            2
        );

        Serial.println(" C");

        Serial.println(
            "================================="
        );
    }

    // =================================================
    // OLED
    // =================================================

    display.clearDisplay();

    drawHeader();

    drawFace(
        faceState
    );

    display.display();

    // ~25 FPS
    delay(40);
}