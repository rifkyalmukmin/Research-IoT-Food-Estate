#include "fuzzy.h"
#include "config.h"
#include <math.h>

// =====================================================
// BASIS ATURAN — 27 aturan (3 x 3 x 3)
// -----------------------------------------------------
// Sugeno orde-nol: tiap aturan menghasilkan satu nilai
// singleton (skor 0..100).
//
// RULE_TABLE[kelembapan][ph][suhu]
//   indeks 0 = KERING / ASAM   / DINGIN
//   indeks 1 = LEMBAB / NETRAL / OPTIMAL
//   indeks 2 = BASAH  / BASA   / PANAS
//
// Panduan pengisian:
//   - tiga parameter tengah (LEMBAB,NETRAL,OPTIMAL) : 95
//   - dua tengah, satu menyimpang                   : 70..80
//   - satu tengah, dua menyimpang                   : 40..55
//   - tidak ada di tengah                           : 15..30
//
// pH yang menyimpang diberi penalti lebih besar
// dibanding suhu, karena pH yang tidak sesuai
// mengunci ketersediaan hara dan lebih sulit dipulihkan.
// =====================================================
const uint8_t RULE_TABLE[3][3][3] = {
    // ---- KELEMBAPAN = KERING ----
    {
        //  DINGIN OPTIMAL PANAS
        {   22,    42,    18 },  // pH ASAM
        {   52,    76,    52 },  // pH NETRAL
        {   24,    42,    20 }   // pH BASA
    },
    // ---- KELEMBAPAN = LEMBAB ----
    {
        {   45,    70,    45 },  // pH ASAM
        {   80,    95,    80 },  // pH NETRAL
        {   45,    70,    45 }   // pH BASA
    },
    // ---- KELEMBAPAN = BASAH ----
    {
        {   20,    42,    15 },  // pH ASAM
        {   52,    76,    52 },  // pH NETRAL
        {   26,    42,    17 }   // pH BASA
    }
};

// =====================================================
// FUNGSI KEANGGOTAAN TRAPESIUM
// -----------------------------------------------------
// Tepi kiri (a==b) atau tepi kanan (c==d) yang datar
// ditangani sebagai bahu (shoulder) sehingga trapesium
// naik/turun dan segitiga cukup satu fungsi ini.
// =====================================================
float trapmf(float x, float a, float b, float c, float d) {
    float leftSlope  = (b == a) ? 1.0f : (x - a) / (b - a);
    float rightSlope = (d == c) ? 1.0f : (d - x) / (d - c);
    float y = fminf(fminf(leftSlope, 1.0f), rightSlope);
    if (y < 0.0f) y = 0.0f;
    if (y > 1.0f) y = 1.0f;
    return y;
}

// =====================================================
// TAHAP 1: FUZZIFIKASI
// -----------------------------------------------------
// Tiap nilai sensor diubah menjadi derajat keanggotaan
// 0..1 pada beberapa himpunan sekaligus (tumpang tindih).
// =====================================================
static void fuzzify(float moisture, float ph, float suhu, FuzzyMembership &mu) {
    for (int i = 0; i < 3; i++) {
        mu.moisture[i] = trapmf(moisture, MF_MOISTURE[i].a, MF_MOISTURE[i].b,
                                MF_MOISTURE[i].c, MF_MOISTURE[i].d);
        mu.ph[i]       = trapmf(ph, MF_PH[i].a, MF_PH[i].b,
                                MF_PH[i].c, MF_PH[i].d);
        mu.suhu[i]     = trapmf(suhu, MF_TEMP[i].a, MF_TEMP[i].b,
                                MF_TEMP[i].c, MF_TEMP[i].d);
    }
}

// =====================================================
// PEMETAAN SKOR -> STATUS
// -----------------------------------------------------
// Rentang keluaran sengaja tumpang tindih. Label akhir
// dipilih dari himpunan dengan derajat keanggotaan
// tertinggi pada nilai skor tersebut.
// =====================================================
static SoilStatus scoreToStatus(float score) {
    float best = -1.0f;
    int   idx  = STATUS_PERHATIAN;
    for (int i = 0; i < 3; i++) {
        float m = trapmf(score, MF_STATUS[i].a, MF_STATUS[i].b,
                         MF_STATUS[i].c, MF_STATUS[i].d);
        if (m > best) {
            best = m;
            idx  = i;
        }
    }
    return (SoilStatus)idx;
}

// =====================================================
// INFERENSI FUZZY LENGKAP
// -----------------------------------------------------
// Tahap 1 fuzzifikasi -> Tahap 2 basis aturan ->
// Tahap 3 inferensi (MIN) -> Tahap 4 defuzzifikasi.
// =====================================================
FuzzyResult fuzzyInfer(float moisture, float ph, float suhu) {
    FuzzyResult res;

    // -------- TAHAP 1: FUZZIFIKASI --------
    fuzzify(moisture, ph, suhu, res.mu);

    // -------- TAHAP 3: INFERENSI --------
    // Untuk tiap aturan, kekuatan aturan (firing strength)
    // dihitung dengan operator MIN karena semua aturan
    // memakai penghubung AND:
    //     alpha = min(mu_kelembapan, mu_ph, mu_suhu)
    //
    // Aturan dengan alpha 0 dilewati (hemat komputasi).
    //
    // Aturan yang menghasilkan singleton (z) sama
    // diagregasi dengan operator MAX sebelum defuzzifikasi.
    float alphaByZ[101];
    for (int i = 0; i <= 100; i++) alphaByZ[i] = 0.0f;

    int activeRules = 0;

    for (int m = 0; m < 3; m++) {
        if (res.mu.moisture[m] <= 0.0f) continue;
        for (int p = 0; p < 3; p++) {
            if (res.mu.ph[p] <= 0.0f) continue;
            for (int t = 0; t < 3; t++) {
                if (res.mu.suhu[t] <= 0.0f) continue;

                // operator MIN (penghubung AND)
                float alpha = res.mu.moisture[m];
                if (res.mu.ph[p]   < alpha) alpha = res.mu.ph[p];
                if (res.mu.suhu[t] < alpha) alpha = res.mu.suhu[t];

                if (alpha <= 0.0f) continue;
                activeRules++;

                uint8_t z = RULE_TABLE[m][p][t];  // singleton Sugeno
                // operator MAX untuk singleton yang sama
                if (alpha > alphaByZ[z]) alphaByZ[z] = alpha;
            }
        }
    }

    // -------- TAHAP 4: DEFUZZIFIKASI --------
    // Rata-rata terbobot: skor = sum(alpha_i * z_i) / sum(alpha_i)
    float num = 0.0f;
    float den = 0.0f;
    for (int z = 0; z <= 100; z++) {
        if (alphaByZ[z] > 0.0f) {
            num += alphaByZ[z] * (float)z;
            den += alphaByZ[z];
        }
    }

    res.activeRules = activeRules;

    if (den <= 0.0f) {
        // Seharusnya tidak terjadi bila himpunan tumpang tindih benar.
        res.score     = 0.0f;
        res.valid     = false;
        res.status    = STATUS_TINDAKAN;
        res.faceState = 0;
        return res;
    }

    res.score  = num / den;
    res.valid  = true;
    res.status = scoreToStatus(res.score);

    // Pemetaan status -> wajah OLED
    switch (res.status) {
        case STATUS_SEHAT:     res.faceState = 2; break;  // senyum
        case STATUS_PERHATIAN: res.faceState = 1; break;  // datar
        default:               res.faceState = 0; break;  // sedih
    }

    return res;
}

// =====================================================
// LABEL STATUS
// =====================================================
const char* statusLabel(SoilStatus s) {
    switch (s) {
        case STATUS_SEHAT:     return "TANAH SEHAT";
        case STATUS_PERHATIAN: return "PERLU PERHATIAN";
        default:               return "BUTUH TINDAKAN";
    }
}

const char* statusShort(SoilStatus s) {
    switch (s) {
        case STATUS_SEHAT:     return "SEHAT";
        case STATUS_PERHATIAN: return "PERHATIAN";
        default:               return "TINDAKAN";
    }
}

// =====================================================
// MODE VERIFIKASI FUZZY (runFuzzySelfTest)
// -----------------------------------------------------
// Menjalankan kasus uji tetap lalu mencetak tabel CSV
// (mudah disalin ke spreadsheet) berisi: input, derajat
// keanggotaan tiap himpunan, jumlah aturan aktif, skor,
// dan status. Dibandingkan dengan hasil MATLAB/Python.
// =====================================================
struct SelfTestCase {
    const char* name;
    float moisture;
    float ph;
    float suhu;
};

void runFuzzySelfTest() {
    // 12 kasus: ideal, satu menyimpang, dua menyimpang,
    // tiga menyimpang, dan beberapa titik tumpang tindih.
    static const SelfTestCase cases[] = {
        { "ideal",              55.0f, 6.50f, 26.0f },
        { "kering",             20.0f, 6.50f, 26.0f },
        { "asam",               55.0f, 4.50f, 26.0f },
        { "panas",              55.0f, 6.50f, 38.0f },
        { "kering+asam",        20.0f, 4.50f, 26.0f },
        { "basah+panas",        85.0f, 6.50f, 40.0f },
        { "tiga_menyimpang",    15.0f, 4.00f, 38.0f },
        { "overlap_moist",      35.0f, 6.50f, 26.0f },
        { "overlap_ph",         55.0f, 5.75f, 26.0f },
        { "overlap_suhu",       55.0f, 6.50f, 19.0f },
        { "overlap_semua",      35.0f, 5.75f, 19.0f },
        { "basa_tinggi",        55.0f, 8.50f, 26.0f }
    };
    const int n = sizeof(cases) / sizeof(cases[0]);

    Serial.println();
    Serial.println(F("===== FUZZY SELF TEST (CSV) ====="));
    // Baris header CSV
    Serial.println(F(
        "case,moisture,ph,suhu,"
        "mu_kering,mu_lembab,mu_basah,"
        "mu_asam,mu_netral,mu_basa,"
        "mu_dingin,mu_optimal,mu_panas,"
        "active_rules,score,valid,status"));

    for (int i = 0; i < n; i++) {
        FuzzyResult r = fuzzyInfer(cases[i].moisture, cases[i].ph, cases[i].suhu);

        char line[220];
        snprintf(line, sizeof(line),
            "%s,%.2f,%.2f,%.2f,"
            "%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,"
            "%d,%.2f,%d,%s",
            cases[i].name, cases[i].moisture, cases[i].ph, cases[i].suhu,
            r.mu.moisture[0], r.mu.moisture[1], r.mu.moisture[2],
            r.mu.ph[0], r.mu.ph[1], r.mu.ph[2],
            r.mu.suhu[0], r.mu.suhu[1], r.mu.suhu[2],
            r.activeRules, r.score, r.valid ? 1 : 0,
            statusShort(r.status));

        Serial.println(line);
    }
    Serial.println(F("===== END SELF TEST ====="));
    Serial.println();
}
