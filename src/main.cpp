#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <math.h>

// =====================================================
// KONFIGURASI WIFI
// =====================================================

const char* WIFI_SSID = "PersibJuaraaaa";
const char* WIFI_PASSWORD = "CodelabsPersib";

// =====================================================
// KONFIGURASI HIVEMQ CLOUD
// =====================================================

const char* MQTT_SERVER =
    "c9ad72fd2aa34c84857753c4437ad26f.s1.eu.hivemq.cloud";

const int MQTT_PORT = 8883;

// Isi dengan Credentials yang dibuat di HiveMQ Cloud
const char* MQTT_USER =
    "iot_foodestate";

const char* MQTT_PASSWORD =
    "Azela$@32";

// =====================================================
// MQTT TOPIC
// =====================================================

const char* MQTT_TOPIC_DATA =
    "smartfarm/sensor/data";

const char* MQTT_TOPIC_STATUS =
    "smartfarm/sensor/status";

// =====================================================
// MQTT CLIENT
// =====================================================

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

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

// Kalibrasi awal
//
// KERING -> ADC besar
// BASAH  -> ADC kecil

const int dryValue = 4095;
const int wetValue = 1500;

// =====================================================
// SENSOR pH
// =====================================================

// GPIO34 sudah digunakan Soil Moisture
// sehingga pH menggunakan GPIO35.

const int PH_PIN = 34;

// Rumus awal pH
//
// WAJIB dikalibrasi menggunakan
// larutan buffer pH.

const float PH_NEUTRAL_VOLTAGE = 2.50;
const float PH_SLOPE = 0.18;

// =====================================================
// DS18B20
// =====================================================

#define DS18B20_PIN 5

OneWire oneWire(DS18B20_PIN);

DallasTemperature temperatureSensor(
    &oneWire
);

// =====================================================
// DATA SENSOR
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

// Status
String status = "";

// 0 = kering
// 1 = lembab
// 2 = basah
int faceState = 1;

// =====================================================
// TIMER
// =====================================================

unsigned long lastSensorRead = 0;

const unsigned long SENSOR_INTERVAL = 2000;

// =====================================================
// WIFI
// =====================================================

void connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    Serial.println();
    Serial.print("Menghubungkan WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    int attempt = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        attempt < 30
    )
    {
        delay(500);

        Serial.print(".");

        attempt++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(
            "WiFi berhasil terhubung!"
        );

        Serial.print("IP ESP32: ");

        Serial.println(
            WiFi.localIP()
        );

        Serial.print("RSSI: ");

        Serial.print(
            WiFi.RSSI()
        );

        Serial.println(" dBm");
    }
    else
    {
        Serial.println(
            "WiFi gagal terhubung!"
        );
    }
}

// =====================================================
// MQTT / HIVEMQ CONNECT
// =====================================================

void connectMQTT()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    while (!mqttClient.connected())
    {
        Serial.println();
        Serial.println(
            "Menghubungkan ke HiveMQ Cloud..."
        );

        // Buat Client ID unik
        String clientId =
            "ESP32-SmartFarm-" +
            String(
                (uint32_t)ESP.getEfuseMac(),
                HEX
            );

        bool connected =
            mqttClient.connect(
                clientId.c_str(),
                MQTT_USER,
                MQTT_PASSWORD
            );

        if (connected)
        {
            Serial.println(
                "HiveMQ Cloud BERHASIL terhubung!"
            );

            Serial.print(
                "MQTT Host: "
            );

            Serial.println(
                MQTT_SERVER
            );

            Serial.print(
                "MQTT Port: "
            );

            Serial.println(
                MQTT_PORT
            );

            // Beritahu backend bahwa device online
            mqttClient.publish(
                MQTT_TOPIC_STATUS,
                "online",
                true
            );

            Serial.println(
                "Status online terkirim."
            );
        }
        else
        {
            Serial.print(
                "HiveMQ gagal terhubung."
            );

            Serial.print(
                " MQTT state = "
            );

            Serial.println(
                mqttClient.state()
            );

            delay(3000);
        }
    }
}

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
// BACA SENSOR pH
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
    // SENSOR pH
    // =================================================

    phADC =
        readPHRaw();

    phVoltage =
        phADC *
        (3.3 / 4095.0);

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
// MQTT PUBLISH SENSOR
// =====================================================

void publishSensorData()
{
    if (!mqttClient.connected())
    {
        Serial.println(
            "MQTT tidak terhubung."
        );

        return;
    }

    // =================================================
    // JSON PAYLOAD
    // =================================================

    String payload = "{";

    payload +=
        "\"device_id\":\"ESP32-001\"";

    payload +=
        ",\"soil_adc\":";

    payload +=
        String(sensorValue);

    payload +=
        ",\"moisture\":";

    payload +=
        String(moisturePercent);

    payload +=
        ",\"status\":\"";

    payload +=
        status;

    payload += "\"";

    payload +=
        ",\"ph_adc\":";

    payload +=
        String(phADC);

    payload +=
        ",\"ph_voltage\":";

    payload +=
        String(
            phVoltage,
            3
        );

    payload +=
        ",\"ph\":";

    payload +=
        String(
            pH,
            2
        );

    payload +=
        ",\"temperature\":";

    payload +=
        String(
            temperatureC,
            2
        );

    payload += "}";

    // =================================================
    // TAMPILKAN DI SERIAL
    // =================================================

    Serial.println();
    Serial.println(
        "========== MQTT DATA =========="
    );

    Serial.println(
        payload
    );

    // =================================================
    // PUBLISH
    // =================================================

    bool success =
        mqttClient.publish(
            MQTT_TOPIC_DATA,
            payload.c_str()
        );

    if (success)
    {
        Serial.println(
            "MQTT Publish: BERHASIL"
        );
    }
    else
    {
        Serial.println(
            "MQTT Publish: GAGAL"
        );
    }

    Serial.println(
        "==============================="
    );
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

    // =================================================
    // GERAKAN
    // =================================================

    if (state == 2)
    {
        // Basah -> memantul
        oy =
            (int)round(
                3.0 *
                sin(
                    t / 140.0
                )
            );
    }
    else if (state == 1)
    {
        // Lembab -> bergoyang
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

    // =================================================
    // WAJAH
    // =================================================

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

        // Senyum lebar
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

        // Senyum tipis
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
            (int)(
                p * 20
            );

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

    // Contoh:
    // LEMBAB 55% 28.4C

    String line =
        status +
        " " +
        String(
            moisturePercent
        ) +
        "% " +
        String(
            temperatureC,
            1
        ) +
        "C";

    int w =
        line.length() * 6;

    int x =
        (SCREEN_WIDTH - w) / 2;

    if (x < 0)
    {
        x = 0;
    }

    display.setCursor(
        x,
        0
    );

    display.print(
        line
    );

    display.drawFastHLine(
        0,
        10,
        SCREEN_WIDTH,
        SSD1306_WHITE
    );
}

// =====================================================
// TAMPILKAN DATA SENSOR DI OLED
// =====================================================

void drawSensorInfo()
{
    display.setTextColor(
        SSD1306_WHITE
    );

    display.setTextSize(1);

    display.setCursor(
        2,
        55
    );

    display.print(
        "pH:"
    );

    display.print(
        pH,
        2
    );

    display.setCursor(
        80,
        55
    );

    display.print(
        temperatureC,
        1
    );

    display.print(
        "C"
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
    // SPLASH SCREEN
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
        "SMART"
    );

    display.setCursor(
        4,
        30
    );

    display.println(
        "FARM"
    );

    display.setTextSize(1);

    display.setCursor(
        70,
        52
    );

    display.println(
        "MQTT"
    );

    display.display();

    delay(2000);

    // =================================================
    // WIFI
    // =================================================

    connectWiFi();

    // =================================================
    // TLS
    // =================================================

    // Untuk tahap testing.
    // Untuk produksi sebaiknya gunakan
    // CA certificate HiveMQ.

    espClient.setInsecure();

    // =================================================
    // MQTT
    // =================================================

    mqttClient.setServer(
        MQTT_SERVER,
        MQTT_PORT
    );

    // Payload JSON cukup kecil,
    // tetapi kita beri buffer lebih besar.

    mqttClient.setBufferSize(512);

    // =================================================
    // MQTT CONNECT
    // =================================================

    connectMQTT();

    // =================================================
    // SERIAL HEADER
    // =================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "SMART FARM MONITORING"
    );

    Serial.println(
        "ESP32 + Soil Moisture + pH + DS18B20"
    );

    Serial.println(
        "OLED + HiveMQ Cloud MQTT"
    );

    Serial.println(
        "========================================"
    );

    Serial.print(
        "HiveMQ Host: "
    );

    Serial.println(
        MQTT_SERVER
    );

    Serial.print(
        "HiveMQ Port: "
    );

    Serial.println(
        MQTT_PORT
    );

    Serial.print(
        "MQTT Topic: "
    );

    Serial.println(
        MQTT_TOPIC_DATA
    );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    // =================================================
    // WIFI
    // =================================================

    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        connectWiFi();
    }

    // =================================================
    // MQTT
    // =================================================

    if (
        !mqttClient.connected()
    )
    {
        connectMQTT();
    }

    // Harus dipanggil terus
    // untuk menjaga koneksi MQTT.

    mqttClient.loop();

    // =================================================
    // TIMER
    // =================================================

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

        // Baca semua sensor
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

        Serial.println(
            "%"
        );

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

        Serial.println(
            " V"
        );

        Serial.print(
            "pH             : "
        );

        Serial.println(
            pH,
            2
        );

        Serial.print(
            "Suhu           : "
        );

        Serial.print(
            temperatureC,
            2
        );

        Serial.println(
            " C"
        );

        Serial.println(
            "================================="
        );

        // =================================================
        // MQTT
        // =================================================

        publishSensorData();
    }

    // =================================================
    // OLED
    // =================================================

    display.clearDisplay();

    drawHeader();

    drawFace(
        faceState
    );

    drawSensorInfo();

    display.display();

    // sekitar 25 FPS
    delay(40);
}