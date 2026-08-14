#ifndef FUZZY_H
#define FUZZY_H

// =====================================================
// MODUL FUZZY LOGIC — POTANI
// -----------------------------------------------------
// Fuzzy Sugeno orde-nol dengan defuzzifikasi rata-rata
// terbobot (weighted average). Menggabungkan tiga input
// (kelembapan, pH, suhu) menjadi satu skor kesehatan
// media tanam 0..100 dan satu status akhir.
//
// Seluruh perhitungan berjalan di perangkat sehingga
// alat tetap memberi penilaian walau tanpa jaringan.
// =====================================================

#include <Arduino.h>

// Status kesehatan menyeluruh (bukan status kelembapan lama)
enum SoilStatus {
    STATUS_TINDAKAN  = 0,  // BUTUH TINDAKAN
    STATUS_PERHATIAN = 1,  // PERLU PERHATIAN
    STATUS_SEHAT     = 2   // TANAH SEHAT
};

// Derajat keanggotaan tiap himpunan untuk tiap variabel
struct FuzzyMembership {
    float moisture[3];  // [KERING, LEMBAB, BASAH]
    float ph[3];        // [ASAM, NETRAL, BASA]
    float suhu[3];      // [DINGIN, OPTIMAL, PANAS]
};

// Hasil lengkap inferensi fuzzy
struct FuzzyResult {
    float          score;        // skor akhir 0..100
    SoilStatus     status;       // status hasil pemetaan skor
    int            faceState;    // 0=sedih, 1=datar, 2=senyum (untuk OLED)
    bool           valid;        // false jika sum(alpha) == 0 (flag error)
    int            activeRules;  // jumlah aturan dengan firing strength > 0
    FuzzyMembership mu;          // derajat keanggotaan tiap himpunan
};

// Fungsi keanggotaan trapesium umum (segitiga = kasus b==c).
float trapmf(float x, float a, float b, float c, float d);

// Inferensi utama. Dapat dipanggil tanpa hardware.
FuzzyResult fuzzyInfer(float moisture, float ph, float suhu);

// Label status siap tampil ("TANAH SEHAT", dst.)
const char* statusLabel(SoilStatus s);

// Kata pendek status ("SEHAT", "PERHATIAN", "TINDAKAN") untuk payload
const char* statusShort(SoilStatus s);

// Mode verifikasi: mencetak tabel CSV ke serial untuk
// dibandingkan dengan hasil MATLAB/Python.
void runFuzzySelfTest();

#endif // FUZZY_H
