#ifndef CONFIG_H
#define CONFIG_H

// =====================================================
// KONFIGURASI PERANGKAT POTANI
// -----------------------------------------------------
// Semua pin, konstanta, dan nilai kalibrasi ada di sini.
// TIDAK ADA kredensial di file ini. Kredensial WiFi/MQTT
// berada di secrets.h (masuk .gitignore).
// =====================================================

#include <Arduino.h>

// -----------------------------------------------------
// IDENTITAS PERANGKAT
// -----------------------------------------------------
#define DEVICE_ID "ESP32-001"

// -----------------------------------------------------
// PEMETAAN PIN (JANGAN DIUBAH)
// -----------------------------------------------------
static const int SOIL_PIN    = 35;  // Kelembapan  -> GPIO35 (ADC1)
static const int PH_PIN      = 27;  // pH          -> GPIO27 (ADC2), sesuai repo
// PERINGATAN: GPIO27 = ADC2. Pada ESP32, ADC2 dipakai driver WiFi,
// sehingga pembacaan pH bisa gagal/0 saat WiFi aktif. Pin dipertahankan
// sesuai repo. Bila pH kacau saat online, pindahkan pH ke pin ADC1
// (GPIO32/33/34/36/39) — ini satu-satunya solusi tuntas.
#define DS18B20_PIN            5     // Suhu DS18B20 -> GPIO5
#define OLED_SDA              21     // OLED I2C SDA -> GPIO21
#define OLED_SCL              22     // OLED I2C SCL -> GPIO22

// Tombol fisik (belum terpasang). Sementara alur dipicu
// lewat perintah serial "measure".
// TODO: ganti dengan tombol fisik GPIO<n>
#define BUTTON_PIN           -1

// -----------------------------------------------------
// OLED
// -----------------------------------------------------
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT        64
#define OLED_ADDR            0x3C

// -----------------------------------------------------
// ADC / SAMPLING (JANGAN DIUBAH JUMLAHNYA)
// -----------------------------------------------------
#define ADC_MAX             4095.0f
#define ADC_VREF            3.3f
static const int SOIL_SAMPLES    = 10;  // 10x
static const int SOIL_SAMPLE_MS  = 3;   // jeda 3 ms
static const int PH_SAMPLES      = 20;  // 20x
static const int PH_SAMPLE_MS    = 2;   // jeda 2 ms

// -----------------------------------------------------
// KALIBRASI KELEMBAPAN
// -----------------------------------------------------
// KERING -> ADC besar, BASAH -> ADC kecil.
// NILAI DEFAULT, akan diganti setelah kalibrasi lapangan.
static const int SOIL_DRY = 4095;  // ADC saat media kering
static const int SOIL_WET = 1500;  // ADC saat media jenuh air

// -----------------------------------------------------
// KALIBRASI pH (mengikuti repo, commit 09df392)
// -----------------------------------------------------
// NILAI DEFAULT, WAJIB dikalibrasi dengan buffer pH 4 dan pH 7.
// Tegangan pH dibaca lewat analogReadMilliVolts() (terkalibrasi
// ESP32), bukan asumsi linear 0-3.3 V.
// pH = 7.0 + (V_NETRAL - Vukur) / SLOPE
static const float V_NETRAL = 0.400f;  // tegangan saat pH 7 (titik awal)
static const float SLOPE    = 0.180f;  // Volt per unit pH

// Batas tegangan yang masih dianggap masuk akal. Di luar rentang ini
// pH TIDAK dipaksa 0; nilai pH valid terakhir dipertahankan.
static const float PH_MIN_VALID_VOLTAGE = 0.05f;
static const float PH_MAX_VALID_VOLTAGE = 1.70f;

// -----------------------------------------------------
// MQTT TOPIC (kredensial ada di secrets.h)
// -----------------------------------------------------
#define MQTT_TOPIC_DATA    "smartfarm/sensor/data"
#define MQTT_TOPIC_STATUS  "smartfarm/sensor/status"
#define MQTT_PORT           8883

// -----------------------------------------------------
// NTP (waktu WIB, UTC+7)
// -----------------------------------------------------
#define NTP_SERVER         "id.pool.ntp.org"
#define NTP_GMT_OFFSET_SEC (7 * 3600)  // WIB
#define NTP_DST_OFFSET_SEC  0

// -----------------------------------------------------
// PAYLOAD
// -----------------------------------------------------
// Sertakan detail derajat keanggotaan pada payload MQTT.
// Set ke 0 untuk memperkecil ukuran payload.
#define PUBLISH_MEMBERSHIP 1

// Kapasitas antrean offline (array melingkar di RAM)
#define QUEUE_CAPACITY 20

// -----------------------------------------------------
// PARAMETER SESI / STATE MACHINE
// -----------------------------------------------------
#define DISPLAY_HOLD_MS        15000UL  // tahan hasil 15 detik
#define STABILITY_WINDOW       5        // simpan 5 pembacaan pH terakhir
#define STABILITY_THRESHOLD    0.1f     // selisih max-min < 0.1 pH
#define STABILITY_MIN_MS       3000UL   // stabil minimal 3 detik
#define STABILITY_TIMEOUT_MS   30000UL  // batas maksimum 30 detik
#define READING_SAMPLE_MS      400UL    // jeda antar sampel saat READING

// =====================================================
// TITIK FUNGSI KEANGGOTAAN FUZZY
// -----------------------------------------------------
// Semua himpunan disimpan sebagai (a, b, c, d) trapesium.
// Segitiga direpresentasikan dengan b == c.
// Nilai ini HARUS sama dengan grafik di proposal dan
// dengan rentang ideal pada crop_ideal_ranges.json.
// =====================================================

struct FuzzySet {
    float a;
    float b;
    float c;
    float d;
};

// Indeks himpunan: 0 = tepi bawah, 1 = tengah, 2 = tepi atas
enum { SET_LOW = 0, SET_MID = 1, SET_HIGH = 2 };

// Kelembapan (%): KERING / LEMBAB / BASAH
static const FuzzySet MF_MOISTURE[3] = {
    {  0.0f,  0.0f, 25.0f, 40.0f},   // KERING (trapesium turun)
    { 30.0f, 55.0f, 55.0f, 75.0f},   // LEMBAB (segitiga)
    { 65.0f, 80.0f,100.0f,100.0f}    // BASAH  (trapesium naik)
};

// pH: ASAM / NETRAL / BASA
static const FuzzySet MF_PH[3] = {
    {  0.0f,  0.0f,  5.0f,  6.0f},   // ASAM   (trapesium turun)
    {  5.5f,  6.5f,  6.5f,  7.5f},   // NETRAL (segitiga)
    {  7.0f,  8.0f, 14.0f, 14.0f}    // BASA   (trapesium naik)
};

// Suhu (derajat C): DINGIN / OPTIMAL / PANAS
static const FuzzySet MF_TEMP[3] = {
    {  0.0f,  0.0f, 16.0f, 20.0f},   // DINGIN  (trapesium turun)
    { 18.0f, 26.0f, 26.0f, 32.0f},   // OPTIMAL (segitiga)
    { 30.0f, 34.0f, 60.0f, 60.0f}    // PANAS   (trapesium naik)
};

// Himpunan keluaran untuk pemetaan skor -> status.
// Sengaja saling tumpang tindih (lihat proposal).
static const FuzzySet MF_STATUS[3] = {
    {  0.0f,  0.0f, 20.0f, 40.0f},   // BUTUH TINDAKAN (0..40)
    { 35.0f, 52.0f, 52.0f, 70.0f},   // PERLU PERHATIAN (35..70)
    { 65.0f, 85.0f,100.0f,100.0f}    // TANAH SEHAT (65..100)
};

#endif // CONFIG_H
