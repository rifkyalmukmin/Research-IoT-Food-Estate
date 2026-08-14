#include "network.h"
#include "config.h"
#include "secrets.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

static WiFiClientSecure espClient;
static PubSubClient     mqttClient(espClient);

static bool ntpSynced = false;

// -----------------------------------------------------
// ANTREAN OFFLINE — array melingkar di RAM (20 entri)
// -----------------------------------------------------
static String queue[QUEUE_CAPACITY];
static int    qHead  = 0;   // indeks entri terlama
static int    qCount = 0;

static void queuePush(const String &payload) {
    int tail = (qHead + qCount) % QUEUE_CAPACITY;
    queue[tail] = payload;
    if (qCount < QUEUE_CAPACITY) {
        qCount++;
    } else {
        // penuh: timpa yang terlama
        qHead = (qHead + 1) % QUEUE_CAPACITY;
    }
}

int queuedCount() { return qCount; }

// =====================================================
// WIFI
// =====================================================
bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

static void connectWiFi() {
    if (wifiConnected()) return;

    Serial.print(F("Menghubungkan WiFi: "));
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        delay(500);
        Serial.print(F("."));
        attempt++;
    }
    Serial.println();

    if (wifiConnected()) {
        Serial.print(F("WiFi terhubung. IP: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("WiFi gagal terhubung (mode offline)."));
    }
}

// =====================================================
// MQTT / HIVEMQ dengan Last Will and Testament
// =====================================================
bool mqttConnected() { return mqttClient.connected(); }

static bool connectMQTT() {
    if (!wifiConnected())     return false;
    if (mqttClient.connected()) return true;

    Serial.println(F("Menghubungkan ke HiveMQ Cloud..."));

    String clientId = "Potani-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    // LWT: broker otomatis mengumumkan "offline" (retained) bila
    // perangkat mati mendadak.
    bool connected = mqttClient.connect(
        clientId.c_str(),
        MQTT_USER,
        MQTT_PASSWORD,
        MQTT_TOPIC_STATUS,   // will topic
        0,                   // will QoS
        true,                // will retained
        "offline"            // will message
    );

    if (connected) {
        Serial.println(F("HiveMQ terhubung."));
        // umumkan online (retained)
        mqttClient.publish(MQTT_TOPIC_STATUS, "online", true);
    } else {
        Serial.print(F("HiveMQ gagal. state="));
        Serial.println(mqttClient.state());
    }
    return connected;
}

bool isOnline() { return wifiConnected() && mqttClient.connected(); }

// =====================================================
// NTP (WIB)
// =====================================================
void syncTimeNTP() {
    if (!wifiConnected()) {
        Serial.println(F("NTP dilewati: WiFi belum siap."));
        return;
    }
    configTime(NTP_GMT_OFFSET_SEC, NTP_DST_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
        ntpSynced = true;
        Serial.println(F("NTP tersinkron (WIB)."));
    } else {
        ntpSynced = false;
        Serial.println(F("NTP gagal sinkron. timestamp = null."));
    }
}

bool getTimestamp(char *buf, size_t len) {
    if (!ntpSynced) return false;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) return false;
    // ISO 8601 dengan offset WIB (+07:00)
    strftime(buf, len, "%Y-%m-%dT%H:%M:%S+07:00", &timeinfo);
    return true;
}

// =====================================================
// PAYLOAD JSON
// =====================================================
void buildPayload(String &out, const SensorReading &s, const FuzzyResult &f, bool stable) {
    char ts[40];
    bool hasTs = getTimestamp(ts, sizeof(ts));

    out = "{";
    out += "\"device_id\":\"" DEVICE_ID "\"";

    out += ",\"timestamp\":";
    if (hasTs) { out += "\""; out += ts; out += "\""; }
    else       { out += "null"; }

    out += ",\"soil_adc\":";   out += String(s.soilADC);
    out += ",\"moisture\":";   out += String(s.moisture, 1);
    out += ",\"ph_adc\":";     out += String(s.phADC);
    out += ",\"ph_voltage\":"; out += String(s.phVoltage, 3);
    out += ",\"ph\":";         out += String(s.ph, 2);
    out += ",\"ph_valid\":";   out += (s.phValid ? "true" : "false");
    out += ",\"temperature\":";out += String(s.temperature, 2);
    out += ",\"health_score\":"; out += String((int)round(f.score));
    out += ",\"status\":\"";   out += statusShort(f.status); out += "\"";
    out += ",\"stable\":";     out += (stable ? "true" : "false");

#if PUBLISH_MEMBERSHIP
    out += ",\"membership\":{";
    out += "\"moisture\":{\"kering\":" + String(f.mu.moisture[0], 2) +
           ",\"lembab\":" + String(f.mu.moisture[1], 2) +
           ",\"basah\":"  + String(f.mu.moisture[2], 2) + "}";
    out += ",\"ph\":{\"asam\":" + String(f.mu.ph[0], 2) +
           ",\"netral\":" + String(f.mu.ph[1], 2) +
           ",\"basa\":"   + String(f.mu.ph[2], 2) + "}";
    out += ",\"suhu\":{\"dingin\":" + String(f.mu.suhu[0], 2) +
           ",\"optimal\":" + String(f.mu.suhu[1], 2) +
           ",\"panas\":"   + String(f.mu.suhu[2], 2) + "}";
    out += "}";
#endif

    out += "}";
}

// Kirim satu payload mentah. true jika berhasil.
static bool rawPublish(const String &payload) {
    if (!mqttClient.connected()) return false;
    return mqttClient.publish(MQTT_TOPIC_DATA, payload.c_str());
}

// Coba kirim ulang isi antrean saat koneksi pulih.
static void flushQueue() {
    while (qCount > 0 && mqttClient.connected()) {
        if (rawPublish(queue[qHead])) {
            qHead = (qHead + 1) % QUEUE_CAPACITY;
            qCount--;
            Serial.print(F("Antrean terkirim. Sisa: "));
            Serial.println(qCount);
        } else {
            break;  // gagal, hentikan; coba lagi nanti
        }
    }
}

bool publishReading(const SensorReading &s, const FuzzyResult &f, bool stable) {
    String payload;
    buildPayload(payload, s, f, stable);

    Serial.println(F("---- PAYLOAD ----"));
    Serial.println(payload);

    if (isOnline() && rawPublish(payload)) {
        Serial.println(F("MQTT publish: BERHASIL"));
        return true;
    }

    // gagal / offline -> antrean
    queuePush(payload);
    Serial.print(F("MQTT publish gagal. Masuk antrean. Total: "));
    Serial.println(qCount);
    return false;
}

// =====================================================
// INIT & LOOP
// =====================================================
void networkInit() {
    // HiveMQ Cloud memakai TLS. Untuk kesederhanaan demo,
    // sertifikat tidak diverifikasi.
    // TODO: pasang root CA HiveMQ dengan espClient.setCACert(...)
    espClient.setInsecure();

    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(1024);  // payload + membership bisa panjang

    connectWiFi();
    syncTimeNTP();
    connectMQTT();
}

void networkLoop() {
    if (wifiConnected()) {
        if (!mqttClient.connected()) {
            connectMQTT();
        }
        mqttClient.loop();
        if (mqttClient.connected()) {
            flushQueue();
        }
    } else {
        // coba sambung ulang WiFi tanpa memblokir terlalu lama
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry > 5000) {
            lastRetry = millis();
            connectWiFi();
            if (wifiConnected()) {
                if (!ntpSynced) syncTimeNTP();
                connectMQTT();
            }
        }
    }
}
