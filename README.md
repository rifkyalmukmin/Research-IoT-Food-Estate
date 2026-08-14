# Potani — Soil Probe Fuzzy (ESP32)

Firmware ESP32 untuk **Potani**, soil probe genggam bagi penanam rumahan.
Alat ditancapkan sebentar ke media tanam, membaca tiga sensor (kelembapan,
pH, suhu), lalu menilai kondisi tanah memakai **fuzzy logic on-device** dan
menampilkannya di OLED serta mengirimkannya ke HiveMQ Cloud lewat MQTT/TLS.

Karya lomba **GEMASTIK — Piranti Cerdas, Sistem Benam & IoT**. Inferensi fuzzy
berjalan langsung di perangkat, sehingga hasil tetap tampil di layar walau tanpa
jaringan.

---

## Metode fuzzy

**Sugeno orde-nol** dengan defuzzifikasi **rata-rata terbobot** (bukan Mamdani,
bukan centroid). Tiga input × tiga himpunan → **27 aturan**. Firing strength
memakai operator **MIN** (AND), agregasi singleton yang sama memakai **MAX**.

Acuan: Pradana, Ichsan, & Akbar (2023) — fuzzy 3 input, 27 aturan pada
mikrokontroler. Potani berbeda pada metode (Sugeno, lebih ringan untuk perangkat
baterai) dan rentang himpunan (disesuaikan untuk sayuran media tanam terbatas).

Tahapan ada di [`src/fuzzy.cpp`](src/fuzzy.cpp): fuzzifikasi → basis aturan →
inferensi → defuzzifikasi. Titik fungsi keanggotaan ada di
[`src/config.h`](src/config.h) (`MF_MOISTURE`, `MF_PH`, `MF_TEMP`).

---

## Struktur file

```
src/
  main.cpp          setup, loop, state machine sesi
  config.h          pin, konstanta, kalibrasi, titik fuzzy (TANPA kredensial)
  secrets.h         kredensial WiFi & MQTT (di .gitignore)
  secrets.example.h template kredensial
  sensors.cpp/.h    pembacaan & konversi tiga sensor
  fuzzy.cpp/.h       seluruh logika fuzzy + runFuzzySelfTest()
  display.cpp/.h    penggambaran OLED per state
  network.cpp/.h    WiFi, MQTT (LWT), NTP, antrean offline
tools/
  fuzzy_reference.py pembanding fuzzy di komputer (numpy)
```

---

## Cara build

Butuh [PlatformIO](https://platformio.org/).

```bash
pio run -e esp32dev            # kompilasi
pio run -e esp32dev -t upload  # unggah ke board
pio device monitor -b 115200   # buka serial monitor
```

### Mengisi secrets

```bash
cp src/secrets.example.h src/secrets.h
```

Lalu isi `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_SERVER`, `MQTT_USER`,
`MQTT_PASSWORD`. File `src/secrets.h` tidak ikut di-commit.

---

## Pola kerja berbasis sesi

State machine: `IDLE → READING → STABILIZING → INFERENCE → DISPLAY → PUBLISH`.
Tombol fisik belum terpasang, jadi sesi dipicu lewat **perintah serial**:

| Perintah  | Aksi                                             |
|-----------|--------------------------------------------------|
| `measure` | Mulai satu sesi pengukuran                       |
| `selftest`| Jalankan verifikasi fuzzy (cetak tabel CSV)      |

Kestabilan: 5 pembacaan pH terakhir, dianggap stabil bila selisih max−min
< 0.1 pH selama ≥ 3 detik. Batas waktu 30 detik; bila lewat, hasil tetap
dipakai tapi payload ditandai `"stable": false`.

Bila publish gagal / offline, payload masuk **antrean melingkar 20 entri** di
RAM dan dikirim ulang saat koneksi pulih. Hasil pengukuran **tetap tampil di
OLED tanpa jaringan**.

---

## Mode verifikasi fuzzy

Di serial monitor ketik `selftest`. Firmware mencetak tabel CSV (input, derajat
keanggotaan tiap himpunan, jumlah aturan aktif, skor, status) untuk 12 kasus uji.

Bandingkan dengan implementasi referensi di komputer:

```bash
python tools/fuzzy_reference.py          # tabel CSV kasus uji yang sama
python tools/fuzzy_reference.py 55 6.5 26 # satu kasus: moisture ph suhu
```

Kedua keluaran memakai header dan urutan kolom yang sama, jadi bisa langsung
disalin ke spreadsheet untuk menghitung selisih error pada bab pengujian.

---

## Payload MQTT

Topik data: `smartfarm/sensor/data`. Topik status (retained, dengan LWT):
`smartfarm/sensor/status` (`online` / `offline`).

```json
{
  "device_id": "ESP32-001",
  "timestamp": "2026-08-14T14:23:05+07:00",
  "soil_adc": 2700,
  "moisture": 53.2,
  "ph_adc": 3000,
  "ph_voltage": 0.417,
  "ph": 6.48,
  "ph_valid": true,
  "temperature": 28.42,
  "health_score": 87,
  "status": "SEHAT",
  "stable": true,
  "membership": {
    "moisture": {"kering": 0.0, "lembab": 0.88, "basah": 0.0},
    "ph":       {"asam": 0.0, "netral": 0.98, "basa": 0.0},
    "suhu":     {"dingin": 0.0, "optimal": 0.67, "panas": 0.0}
  }
}
```

`timestamp` bernilai `null` bila NTP gagal sinkron. `membership` dapat dimatikan
lewat `#define PUBLISH_MEMBERSHIP 0` di `config.h`. `status` kini berisi status
kesehatan menyeluruh (`SEHAT` / `PERHATIAN` / `TINDAKAN`), bukan status
kelembapan lama (`KERING` / `LEMBAB` / `BASAH`).

---

## Himpunan fuzzy

| Variabel | Himpunan | Bentuk | (a, b, c, d) |
|---|---|---|---|
| Kelembapan (%) | KERING | Trapesium turun | 0, 0, 25, 40 |
| Kelembapan (%) | LEMBAB | Segitiga | 30, 55, 75 |
| Kelembapan (%) | BASAH | Trapesium naik | 65, 80, 100, 100 |
| pH | ASAM | Trapesium turun | 0, 0, 5.0, 6.0 |
| pH | NETRAL | Segitiga | 5.5, 6.5, 7.5 |
| pH | BASA | Trapesium naik | 7.0, 8.0, 14, 14 |
| Suhu (°C) | DINGIN | Trapesium turun | 0, 0, 16, 20 |
| Suhu (°C) | OPTIMAL | Segitiga | 18, 26, 32 |
| Suhu (°C) | PANAS | Trapesium naik | 30, 34, 60, 60 |

Pemetaan skor → status (tumpang tindih, ambil keanggotaan tertinggi):

| Skor | Status |
|---|---|
| 0–40 | BUTUH TINDAKAN |
| 35–70 | PERLU PERHATIAN |
| 65–100 | TANAH SEHAT |

---

## Basis aturan (27 singleton Sugeno)

`RULE_TABLE[kelembapan][pH][suhu]` — nilai singleton skor 0..100.
pH menyimpang diberi penalti lebih besar daripada suhu.

| Kelembapan | pH | Suhu DINGIN | Suhu OPTIMAL | Suhu PANAS |
|---|---|---|---|---|
| KERING | ASAM | 22 | 42 | 18 |
| KERING | NETRAL | 52 | 76 | 52 |
| KERING | BASA | 24 | 42 | 20 |
| LEMBAB | ASAM | 45 | 70 | 45 |
| LEMBAB | NETRAL | 80 | **95** | 80 |
| LEMBAB | BASA | 45 | 70 | 45 |
| BASAH | ASAM | 20 | 42 | 15 |
| BASAH | NETRAL | 52 | 76 | 52 |
| BASAH | BASA | 26 | 42 | 17 |

Skor 95 (baris tebal) adalah kondisi ideal: ketiganya di himpunan tengah.

---

## Kalibrasi (default, perlu diganti)

Di `config.h`:

- `SOIL_DRY`, `SOIL_WET` — batas ADC kelembapan.
- `V_NETRAL` (0.400 V), `SLOPE` (0.180 V/pH) — kalibrasi pH; **wajib**
  dikalibrasi dengan buffer pH 4 dan pH 7 sebelum dipakai serius. Tegangan pH
  dibaca lewat `analogReadMilliVolts()` (terkalibrasi ESP32). Bila tegangan di
  luar `PH_MIN_VALID_VOLTAGE`..`PH_MAX_VALID_VOLTAGE`, pH tidak dipaksa 0 —
  nilai valid terakhir dipertahankan dan payload menandai `"ph_valid": false`.

Pemetaan pin (mengikuti repo, tidak diubah): kelembapan GPIO35, pH GPIO27,
DS18B20 GPIO5, OLED I2C SDA GPIO21 / SCL GPIO22.
