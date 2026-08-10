# 🌱 Smart Farm Monitoring — ESP32 + MQTT + OLED

Project IoT untuk **monitoring kondisi tanah dan lingkungan pertanian** (Food Estate) menggunakan:

- **ESP32** (board `esp32dev`, framework Arduino)
- **Soil Moisture Sensor** (kapasitif, ADC) — kelembapan tanah
- **pH Sensor** (analog) — tingkat keasaman tanah
- **DS18B20** (OneWire) — suhu tanah/lingkungan
- **OLED SSD1306 128x64** (I2C) — tampilan animasi status
- **HiveMQ Cloud** (MQTT over TLS) — pengiriman data ke cloud/backend

Data sensor dibaca setiap **2 detik**, ditampilkan di OLED dan Serial Monitor, lalu dikirim ke broker MQTT dalam format JSON.

---

## 📌 Daftar Isi

1. [Fitur](#-fitur)
2. [Cara Kerja (Alur Data)](#-cara-kerja-alur-data)
3. [Hardware & Wiring](#-hardware--wiring)
4. [Struktur Project](#-struktur-project)
5. [Prasyarat](#-prasyarat)
6. [Setup & Konfigurasi](#-setup--konfigurasi)
7. [Build, Upload & Monitor](#-build-upload--monitor)
8. [Protokol MQTT](#-protokol-mqtt)
9. [Kalibrasi Sensor](#-kalibrasi-sensor)
10. [Tampilan OLED](#-tampilan-oled)
11. [Output Serial Monitor](#-output-serial-monitor)
12. [Troubleshooting](#-troubleshooting)
13. [Keamanan & Keterbatasan](#-keamanan--keterbatasan)
14. [Roadmap Perbaikan](#-roadmap-perbaikan)
15. [Referensi](#-referensi)

---

## ✨ Fitur

| Fitur | Keterangan |
|---|---|
| 📶 WiFi STA | Koneksi ke WiFi 2.4 GHz, auto-reconnect saat putus |
| 📡 MQTT over TLS | Koneksi ke HiveMQ Cloud port `8883` via `WiFiClientSecure` |
| 💧 Soil Moisture | ADC 12-bit (0–100%), status **KERING / LEMBAB / BASAH** |
| 🧪 pH Tanah | Perhitungan pH dari tegangan ADC, dikalibrasi manual |
| 🌡️ DS18B20 | Suhu via OneWire, validasi koneksi sensor |
| 📺 OLED Animasi | Wajah animasi sesuai kondisi tanah (sedih/tenang/senang) + info sensor |
| 🔄 Auto-reconnect | WiFi & MQTT dicoba tersambung ulang di `loop()` |
| 📤 Payload JSON | Data dikirim ke HiveMQ Cloud tiap 2 detik |
| 🖥️ Serial Monitor | Log detail ADC, persen, status, pH, dan suhu |

---

## 🧠 Cara Kerja (Alur Data)

```
┌──────────────┐   ADC   ┌──────────┐          ┌─────────────────┐
│ Soil Moisture│ ───────▶│          │          │  OLED SSD1306   │
│ (GPIO35)     │         │          │ ────────▶│  (animasi wajah)│
├──────────────┤   ADC   │   ESP32  │          └─────────────────┘
│ pH Sensor    │ ───────▶│          │
│ (GPIO34)     │         │          │ ────────▶┌─────────────────┐
├──────────────┤ OneWire │          │  Serial  │ Serial Monitor  │
│ DS18B20      │ ───────▶│          │          └─────────────────┘
│ (GPIO5)      │         │          │
└──────────────┘         └────┬─────┘
                              │ MQTT (TLS 8883)
                              ▼
                    ┌─────────────────────┐
                    │  HiveMQ Cloud Broker │
                    │ smartfarm/sensor/data│
                    │ smartfarm/sensor/status
                    └─────────────────────┘
                              │
                              ▼
                    Backend / Dashboard / Database
```

Urutan kerja dalam `loop()`:

1. Cek koneksi WiFi → reconnect bila putus.
2. Cek koneksi MQTT → reconnect bila putus, lalu `mqttClient.loop()`.
3. Setiap **2 detik** (`SENSOR_INTERVAL`): baca semua sensor → tampilkan ke Serial → publish JSON ke MQTT.
4. Redraw OLED ±25 FPS (`delay(40)`).

---

## 🔌 Hardware & Wiring

### Daftar Komponen

| Komponen | Jumlah |
|---|---|
| ESP32 DevKit (module ESP32-WROOM-32) | 1 |
| OLED SSD1306 128x64 (I2C) | 1 |
| Sensor Soil Moisture kapasitif (AO) | 1 |
| Sensor pH meter analog (mis. seri SEN0161 / sejenisnya) | 1 |
| DS18B20 (waterproof/sensor suhu) | 1 |
| Resistor 4.7 kΩ (pull-up DS18B20, bila modul belum ada) | 1 |
| Kabel jumper / breadboard | secukupnya |

### Pin Mapping

| Komponen | Pin ESP32 | Keterangan |
|---|---|---|
| OLED SDA | **GPIO21** | I2C data |
| OLED SCL | **GPIO22** | I2C clock |
| OLED VCC / GND | 3.3V / GND | Alamat I2C `0x3C` |
| Soil Moisture (AO) | **GPIO35** | ADC input, 12-bit, attenuasi 11 dB |
| pH Sensor (analog out) | **GPIO34** | ADC input, 12-bit, attenuasi 11 dB |
| DS18B20 (DATA) | **GPIO5** | OneWire (butuh pull-up 4.7 kΩ ke VCC) |
| DS18B20 VDD / GND | 3.3–5V / GND | |

> ⚠️ **Catatan hardware penting:**
> - `GPIO34` dan `GPIO35` pada ESP32 adalah pin **input-only** (tanpa pull-up internal) — cocok untuk ADC.
> - Kedua pin **tidak 5V tolerant**. Pastikan output modul pH **tidak melebihi 3.3V** (banyak modul pH murah disupply 5V dan output-nya bisa > 3.3V — gunakan voltage divider jika perlu) untuk menghindari kerusakan permanen.
> - DS18B20 **wajib** ada resistor pull-up 4.7 kΩ antara DATA dan VCC, kecuali modul sudah menyediakannya.

### Skema Sederhana

```
  ESP32                     OLED SSD1306
┌─────────┐               ┌──────────────┐
│ GPIO21 ├───────────────▶│ SDA          │
│ GPIO22 ├───────────────▶│ SCL          │
│ 3.3V   ├───────────────▶│ VCC          │
│ GND    ├───────────────▶│ GND          │
└─────────┘               └──────────────┘

  ESP32                    Soil Moisture        pH Sensor
┌─────────┐               ┌────────────┐      ┌──────────┐
│ GPIO35 ├───────────────▶│ AO         │      │          │
│ GPIO34 ├───────────────▶│            │◀─────│ AO       │
│ 3.3V   ├───────────────▶│ VCC        │      │ VCC(+5V)*│
│ GND    ├───────────────▶│ GND        │      │ GND      │
└─────────┘               └────────────┘      └──────────┘
   * pastikan output ≤ 3.3V

  ESP32                    DS18B20
┌─────────┐               ┌──────────────┐
│ GPIO5  ├───4.7kΩ─▲─────▶│ DATA (kuning)│
│ 3.3V   ├───▲─────┘      │ VDD  (merah) │
│ GND    ├────────────────▶│ GND  (hitam) │
└─────────┘               └──────────────┘
```

---

## 🗂️ Struktur Project

```
Research IoT Food Estate/
├── platformio.ini          # Konfigurasi build PlatformIO
├── include/                # Header files (kosong, default PlatformIO)
├── lib/                    # Library lokal (kosong, default PlatformIO)
├── src/
│   └── main.cpp            # Kode utama ESP32 (semua logika di sini)
├── test/                   # Unit test (kosong, default PlatformIO)
├── .gitignore              # Mengabaikan .pio & file lokal VSCode
└── README.md               # Dokumen ini
```

---

## ⚙️ Prasyarat

- **PlatformIO Core** ≥ 6.x atau **VS Code + PlatformIO IDE**.
- Akun **HiveMQ Cloud** (free tier) dengan cluster yang sudah dibuat.
- ESP32 yang terhubung ke komputer via kabel USB.

---

## 🚀 Setup & Konfigurasi

### 1. `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
    paulstoffregen/OneWire
    milesburton/DallasTemperature
    knolleary/PubSubClient
```

### 2. Konfigurasi WiFi & MQTT

Semua konfigurasi berada di bagian atas `src/main.cpp`:

| Konstanta | Deskripsi |
|---|---|
| `WIFI_SSID` | Nama WiFi Anda (2.4 GHz) |
| `WIFI_PASSWORD` | Password WiFi |
| `MQTT_SERVER` | Host cluster HiveMQ Cloud (contoh: `xxx.s1.eu.hivemq.cloud`) |
| `MQTT_PORT` | Port TLS = `8883` |
| `MQTT_USER` | Username HiveMQ Cloud |
| `MQTT_PASSWORD` | Password HiveMQ Cloud |
| `MQTT_TOPIC_DATA` | Topik data sensor (`smartfarm/sensor/data`) |
| `MQTT_TOPIC_STATUS` | Topik status (`smartfarm/sensor/status`) |

```cpp
const char* WIFI_SSID = "NamaWiFiAnda";
const char* WIFI_PASSWORD = "PasswordWiFi";

const char* MQTT_SERVER = "xxxx.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;

const char* MQTT_USER = "username_hivemq";
const char* MQTT_PASSWORD = "password_hivemq";
```

> 🔒 Jangan commit kredensial asli ke repository publik — lihat [Keamanan & Keterbatasan](#-keamanan--keterbatasan).

### 3. TLS (transport security)

```cpp
espClient.setInsecure();   // testing: TLS tanpa verifikasi CA
```

Mode ini mengenkripsi data, tetapi **tidak memverifikasi identitas server** (rentan man-in-the-middle). Untuk produksi, ganti dengan sertifikat CA HiveMQ:

```cpp
static const char* root_ca = R"(-----BEGIN CERTIFICATE-----
  ... CA cert HiveMQ ...
-----END CERTIFICATE-----)";
espClient.setCACert(root_ca);
```

---

## 🔨 Build, Upload & Monitor

```bash
# Compile project
platformio run

# Upload firmware ke ESP32
platformio run --target upload

# Upload dengan port spesifik (macOS)
platformio run --target upload --upload-port /dev/cu.usbserial-0001

# Monitor serial
platformio device monitor --port /dev/cu.usbserial-0001 --baud 115200
```

Atau gunakan tombol **Build / Upload / Serial Monitor** di VSCode PlatformIO.

**Hasil build saat ini** (informasi memory):

```
RAM:  14.2% (used 46596 bytes from 327680 bytes)
Flash: 71.5% (used 937361 bytes from 1310720 bytes)
```

---

## 📡 Protokol MQTT

### Topik

| Topik | Arah | QoS | Retained | Isi |
|---|---|---|---|---|
| `smartfarm/sensor/data` | ESP32 → Broker | 0 | No | JSON data sensor tiap 2 detik |
| `smartfarm/sensor/status` | ESP32 → Broker | 0 | **Yes** | `online` saat device terhubung |

### Format Payload (JSON)

```json
{
  "device_id": "ESP32-001",
  "soil_adc": 2700,
  "moisture": 53,
  "status": "LEMBAB",
  "ph_adc": 3000,
  "ph_voltage": 2.417,
  "ph": 7.46,
  "temperature": 28.42
}
```

| Field | Tipe | Keterangan |
|---|---|---|
| `device_id` | string | ID device (`ESP32-001`, masih statis) |
| `soil_adc` | int | Raw ADC soil moisture (0–4095) |
| `moisture` | int | Kelembapan 0–100% |
| `status` | string | `KERING` / `LEMBAB` / `BASAH` |
| `ph_adc` | int | Raw ADC pH (0–4095) |
| `ph_voltage` | float | Tegangan pH (volt, 3 desimal) |
| `ph` | float | Nilai pH (2 desimal, 0–14) |
| `temperature` | float | Suhu °C (2 desimal) |

### Uji Subscribe (MQTTX / mosquitto)

```bash
mosquitto_sub -h <host-hivemq> -p 8883 \
  --cafile <ca.pem> -u <user> -P <pass> \
  -t 'smartfarm/sensor/#' -v
```

> Payload dibangun manual via string concatenation — cocok untuk payload kecil saat ini. Jika payload bertambah, pertimbangkan library **ArduinoJson**.

---

## 🧪 Kalibrasi Sensor

### 1. Soil Moisture

Kode menggunakan kalibrasi statis:

```cpp
const int dryValue = 4095;  // ADC saat sensor di udara kering
const int wetValue = 1500;  // ADC saat sensor dicelup air
```

Perhitungan: `moisture = map(adc, dryValue, wetValue, 0, 100)` lalu `constrain` ke 0–100.

- **KERING** → ADC besar (mendekati 4095)
- **BASAH** → ADC kecil (mendekati 1500)

> Setiap sensor/unit bisa berbeda. Kalibrasi ulang: catat ADC saat sensor di **udara kering** (→ `dryValue`) dan saat **direndam air** (→ `wetValue`), lalu update konstanta.

### 2. Sensor pH

Kode menggunakan rumus:

```cpp
phVoltage = phADC * (3.3 / 4095.0);
pH = 7.0 + ((PH_NEUTRAL_VOLTAGE - phVoltage) / PH_SLOPE);

const float PH_NEUTRAL_VOLTAGE = 2.50;  // tegangan saat pH = 7
const float PH_SLOPE           = 0.18;  // volt per unit pH
```

**Kalibrasi 2 titik (disarankan):**

1. Celup probe ke larutan **buffer pH 7** — ukur `phVoltage`, set `PH_NEUTRAL_VOLTAGE` = tegangan terukur.
2. Celup ke larutan **buffer pH 4** (atau 4.01) — hitung `PH_SLOPE = (V_pH7 - V_pH4) / (7 - 4)`.
3. Update kedua konstanta, rebuild, dan verifikasi dengan buffer ketiga (mis. pH 10).

> Nilai saat ini (`2.50` dan `0.18`) adalah nilai awal umum — **wajib dikalibrasi** dengan larutan buffer agar akurat.

### 3. DS18B20

Tidak perlu kalibrasi manual. Jika sensor tidak terdeteksi, cek:

- Pull-up 4.7 kΩ pada pin DATA.
- Koneksi VDD/GND benar.
- Serial akan menampilkan `WARNING: DS18B20 tidak terdeteksi!`.

---

## 📺 Tampilan OLED

OLED 128x64 dibagi menjadi 3 area:

```
┌──────────────────────────────┐
│  LEMBAB 53% 28.4C   ← header │
├──────────────────────────────┤
│                              │
│        ( .  . )              │
│         smile  ← wajah animasi│
│                              │
│  pH:7.46       28.4C ← info  │
└──────────────────────────────┘
```

| Area | Posisi | Isi |
|---|---|---|
| Header | Baris 0 | `STATUS moisture% suhuC` |
| Wajah | Tengah (64,42) r=20 | Animasi sesuai kondisi |
| Info | Baris 55 | `pH:x.xx` dan suhu |

### Animasi Wajah

| Kondisi | Kelembapan | Animasi |
|---|---|---|
| 😢 KERING | < 30% | Wajah sedih: alis khawatir, mulut cekung, **air mata jatuh** |
| 🙂 LEMBAB | 30–69% | Wajah tenang, **berkedip** setiap ±2.5 detik, bergoyang halus |
| 😄 BASAH | ≥ 70% | Wajah senang: **memantul**, senyum lebar, efek **sparkle** |

Splash screen `SMART FARM` + `MQTT` ditampilkan 2 detik saat boot.

---

## 🖥️ Output Serial Monitor

Contoh log saat berjalan normal (interval 2 detik):

```
========== SENSOR DATA ==========
Soil ADC       : 2700
Kelembapan     : 53%
Status         : LEMBAB
pH ADC         : 3000
pH Voltage     : 2.417 V
pH             : 7.46
Suhu           : 28.42 C
=================================

========== MQTT DATA ==========
{"device_id":"ESP32-001","soil_adc":2700,"moisture":53,"status":"LEMBAB","ph_adc":3000,"ph_voltage":2.417,"ph":7.46,"temperature":28.42}
MQTT Publish: BERHASIL
===============================
```

Log saat boot menampilkan status WiFi (IP + RSSI), koneksi MQTT, dan konfigurasi broker.

---

## 🐛 Troubleshooting

| Gejala | Kemungkinan Penyebab | Solusi |
|---|---|---|
| `WiFi gagal terhubung!` | SSID/password salah, sinyal lemah | Periksa kredensial & jarak ke router |
| `HiveMQ gagal terhubung` + state | Broker unreachable / kredensial salah | Cek host, port 8883, user/password. Lihat kode state MQTT di bawah |
| `MQTT state = 4` | Username/password salah | Periksa kredensial HiveMQ Cloud |
| `MQTT state = 5` | User tidak diizinkan akses topik | Periksa pengaturan user di HiveMQ |
| `WARNING: DS18B20 tidak terdeteksi!` | Wiring / pull-up salah | Cek pull-up 4.7 kΩ dan koneksi |
| `OLED tidak ditemukan!` | Alamat/GPIO salah, wiring lepas | Cek alamat `0x3C`, SDA=21, SCL=22 |
| Nilai pH tidak masuk akal | Belum dikalibrasi / tegangan > 3.3V | Lakukan kalibrasi buffer; cek tegangan output |
| Nilai moisture mentok 0/100 | Kalibrasi `dryValue`/`wetValue` salah | Kalibrasi ulang sesuai kondisi sensor |

### Kode State MQTT (`mqttClient.state()`)

| State | Arti |
|---|---|
| `-4` | Koneksi hilang |
| `-3` | Timeout connect |
| `-2` | Koneksi ditolak |
| `-1` | Terputus (disconnected) |
| `1` | Protokol tidak didukung |
| `2` | Client ID ditolak |
| `3` | Server tidak tersedia |
| `4` | Username/password salah |
| `5` | Tidak diotorisasi |

---

## 🔒 Keamanan & Keterbatasan

Keterbatasan yang diketahui pada versi saat ini:

1. **Kredensial hardcoded** di `main.cpp` (WiFi & MQTT) dan terdokumentasi di README — **berisiko** jika repo publik. Disarankan pindah ke `build_flags` di `platformio.ini` atau file yang di-`.gitignore`.
2. **`espClient.setInsecure()`** — TLS tanpa verifikasi CA, hanya untuk pengujian. Gunakan CA certificate untuk produksi.
3. **`connectMQTT()` blocking tanpa batas** — jika broker offline, device bisa terhenti di loop koneksi (tanpa watchdog, tidak ada auto-reset). Idealnya ada batas retry + backoff.
4. **Kalibrasi sensor masih nilai default** — pH dan soil moisture wajib dikalibrasi per unit.
5. **Tidak ada timestamp/NTP** di payload — backend tidak tahu kapan data dibaca.
6. **`device_id` statis** (`ESP32-001`) — tidak konsisten dengan client ID MQTT berbasis MAC.
7. **Tidak ada Last Will (LWT)** — jika device mati mendadak, status `online` (retained) akan tetap tersisa.
8. **Pembacaan sensor blocking** (`requestTemperatures()` menunggu konversi ±750 ms) — loop terhenti sementara tiap siklus.
9. **Payload JSON dibangun manual** — rawan error pada payload yang lebih kompleks.

---

## 📈 Roadmap Perbaikan

**Prioritas tinggi:**
- [ ] Pindahkan kredensial ke `build_flags` / file env (jangan di-commit)
- [ ] Ganti `setInsecure()` dengan CA certificate HiveMQ
- [ ] Batasi retry `connectMQTT()` + tambah watchdog
- [ ] Kalibrasi pH (buffer 2 titik) & soil moisture per unit

**Prioritas sedang:**
- [ ] Tambah NTP + `timestamp` di payload JSON
- [ ] Tambah MQTT Last Will (`offline`)
- [ ] Gunakan MAC address sebagai `device_id`
- [ ] Gunakan `analogReadMilliVolts()` untuk akurasi ADC
- [ ] Baca DS18B20 non-blocking (`setWaitForConversion(false)`)

**Prioritas rendah:**
- [ ] Pindah ke ArduinoJson
- [ ] Pin versi library di `lib_deps`
- [ ] Pecah `main.cpp` menjadi modul (`config.h`, `sensors.h`, `mqtt.h`, `display.h`)
- [ ] Unit test logika sensor (`pio test -e native`)
- [ ] OTA update

---

## 📚 Referensi

- [PlatformIO — ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [HiveMQ Cloud](https://www.hivemq.com/mqtt-cloud-broker/)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [OneWire](https://github.com/paulstoffregen/OneWire)
