#!/usr/bin/env python3
"""
Pembanding fuzzy Potani (referensi komputer).

Menjalankan logika fuzzy Sugeno orde-nol yang SAMA PERSIS dengan
firmware ESP32 (src/fuzzy.cpp), memakai numpy saja. Dipakai untuk
memverifikasi keluaran perangkat pada bab pengujian proposal:
jalankan `selftest` di ESP32, jalankan skrip ini, lalu bandingkan
kolom score/status untuk menghitung selisih error.

Pemakaian:
    python fuzzy_reference.py            # cetak tabel CSV kasus uji
    python fuzzy_reference.py 55 6.5 26  # satu kasus (moisture ph suhu)
"""

import sys
import numpy as np

# ---- Titik fungsi keanggotaan (harus sama dengan config.h) ----
# (a, b, c, d)
MF_MOISTURE = [
    (0.0, 0.0, 25.0, 40.0),    # KERING
    (30.0, 55.0, 55.0, 75.0),  # LEMBAB
    (65.0, 80.0, 100.0, 100.0) # BASAH
]
MF_PH = [
    (0.0, 0.0, 5.0, 6.0),      # ASAM
    (5.5, 6.5, 6.5, 7.5),      # NETRAL
    (7.0, 8.0, 14.0, 14.0)     # BASA
]
MF_TEMP = [
    (0.0, 0.0, 16.0, 20.0),    # DINGIN
    (18.0, 26.0, 26.0, 32.0),  # OPTIMAL
    (30.0, 34.0, 60.0, 60.0)   # PANAS
]
MF_STATUS = [
    (0.0, 0.0, 20.0, 40.0),    # BUTUH TINDAKAN
    (35.0, 52.0, 52.0, 70.0),  # PERLU PERHATIAN
    (65.0, 85.0, 100.0, 100.0) # TANAH SEHAT
]

# RULE_TABLE[moisture][ph][suhu] -> singleton 0..100 (sama dengan fuzzy.cpp)
RULE_TABLE = [
    [[22, 42, 18], [52, 76, 52], [24, 42, 20]],  # KERING
    [[45, 70, 45], [80, 95, 80], [45, 70, 45]],  # LEMBAB
    [[20, 42, 15], [52, 76, 52], [26, 42, 17]],  # BASAH
]

STATUS_SHORT = ["TINDAKAN", "PERHATIAN", "SEHAT"]


def trapmf(x, s):
    a, b, c, d = s
    left = 1.0 if b == a else (x - a) / (b - a)
    right = 1.0 if d == c else (d - x) / (d - c)
    y = min(min(left, 1.0), right)
    return float(np.clip(y, 0.0, 1.0))


def fuzzify(value, sets):
    return [trapmf(value, s) for s in sets]


def score_to_status(score):
    mus = [trapmf(score, s) for s in MF_STATUS]
    return int(np.argmax(mus))


def infer(moisture, ph, suhu):
    mu_m = fuzzify(moisture, MF_MOISTURE)
    mu_p = fuzzify(ph, MF_PH)
    mu_t = fuzzify(suhu, MF_TEMP)

    alpha_by_z = np.zeros(101)
    active = 0
    for m in range(3):
        if mu_m[m] <= 0:
            continue
        for p in range(3):
            if mu_p[p] <= 0:
                continue
            for t in range(3):
                if mu_t[t] <= 0:
                    continue
                alpha = min(mu_m[m], mu_p[p], mu_t[t])  # operator MIN (AND)
                if alpha <= 0:
                    continue
                active += 1
                z = RULE_TABLE[m][p][t]
                if alpha > alpha_by_z[z]:            # operator MAX (singleton sama)
                    alpha_by_z[z] = alpha

    den = alpha_by_z.sum()
    if den <= 0:
        return dict(mu_m=mu_m, mu_p=mu_p, mu_t=mu_t,
                    active=active, score=0.0, valid=False, status=0)

    num = float((alpha_by_z * np.arange(101)).sum())
    score = num / den
    return dict(mu_m=mu_m, mu_p=mu_p, mu_t=mu_t,
                active=active, score=score, valid=True,
                status=score_to_status(score))


CASES = [
    ("ideal",           55.0, 6.50, 26.0),
    ("kering",          20.0, 6.50, 26.0),
    ("asam",            55.0, 4.50, 26.0),
    ("panas",           55.0, 6.50, 38.0),
    ("kering+asam",     20.0, 4.50, 26.0),
    ("basah+panas",     85.0, 6.50, 40.0),
    ("tiga_menyimpang", 15.0, 4.00, 38.0),
    ("overlap_moist",   35.0, 6.50, 26.0),
    ("overlap_ph",      55.0, 5.75, 26.0),
    ("overlap_suhu",    55.0, 6.50, 19.0),
    ("overlap_semua",   35.0, 5.75, 19.0),
    ("basa_tinggi",     55.0, 8.50, 26.0),
]

HEADER = ("case,moisture,ph,suhu,"
          "mu_kering,mu_lembab,mu_basah,"
          "mu_asam,mu_netral,mu_basa,"
          "mu_dingin,mu_optimal,mu_panas,"
          "active_rules,score,valid,status")


def row(name, moisture, ph, suhu):
    r = infer(moisture, ph, suhu)
    return (f"{name},{moisture:.2f},{ph:.2f},{suhu:.2f},"
            f"{r['mu_m'][0]:.3f},{r['mu_m'][1]:.3f},{r['mu_m'][2]:.3f},"
            f"{r['mu_p'][0]:.3f},{r['mu_p'][1]:.3f},{r['mu_p'][2]:.3f},"
            f"{r['mu_t'][0]:.3f},{r['mu_t'][1]:.3f},{r['mu_t'][2]:.3f},"
            f"{r['active']},{r['score']:.2f},{1 if r['valid'] else 0},"
            f"{STATUS_SHORT[r['status']]}")


def main():
    if len(sys.argv) == 4:
        m, p, t = (float(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]))
        print(HEADER)
        print(row("cli", m, p, t))
        return
    print(HEADER)
    for name, m, p, t in CASES:
        print(row(name, m, p, t))


if __name__ == "__main__":
    main()
