#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# MPYY katalog paketinin golden özeti. scripts/ci-gate-mpyy.sh bunu üretir ve
# tests/golden/mpyy/beklenen.txt ile karşılaştırır.
#
# Özet BİLEREK okunabilirdir: bir platform ya da bir çıkarım değişikliği
# uyuşmadığında fark, "hash tutmadı" değil, hangi satırın hangi rengi kaybettiğini
# söylemelidir (tests/golden/README.md ile aynı gerekçe).
#
# Referans satırlar burada değil, kaynak veride yaşar: bu betik yalnız SAYAR ve
# ADI VERİLEN satırların değerini yazar. Hiçbir mevzuat değeri bu dosyada sabit
# değildir (CLAUDE.md 5.13) — adlar birer arama anahtarıdır.

import hashlib
import json
import os
import sys

# Denetlenecek referans satırlar: kimlikleri sabittir (data.md R5), değerleri
# katalogdan okunur. Beş ekten de birer örnek, biri çok tonlu grup satırı.
REFERANS_STILLER = [
    "ortak-ulke-siniri",
    "ortak-organize-sanayi-bolgesi",
    "ortak-orman-alani",
    "msp-yerlesmeler-ve-kademelenmesi",
    "msp-yerlesmeler-ve-kademelenmesi-2",
    "msp-yerlesmeler-ve-kademelenmesi-3",
    "cdp-kentsel-meskun-yerlesik-alan",
    "cdp-erisme-kontrollu-karayolu-otoyol",
    "nip-merkezi-is-alani-mia",
    "nip-mevcut-konut-alani-brut-yogunluguna-gore",
    "uip-ticaret-alani",
    "uip-anaokulu-alani",
    "uip-park",
    "uip-akaryakit-ve-servis-istasyonu-alani",
]
REFERANS_DETAYLAR = [
    "ek1e-plan-siniri",
    "ek1e-aile-sagligi-merkezi",
    "ek1e-cami",
]
REFERANS_KALEMLER = [
    "ek2-anaokulu",
    "ek2-park",
    "ek2-teknik-altyapi-yol-ve-otopark-haric",
]


def oku(yol):
    with open(yol, encoding="utf-8") as f:
        return json.load(f)


def sha256(yol):
    h = hashlib.sha256()
    with open(yol, "rb") as f:
        for parca in iter(lambda: f.read(65536), b""):
            h.update(parca)
    return h.hexdigest()


def stil_satiri(s):
    parcalar = ["ad=" + s.get("ad", "")]
    parcalar.append("ek=" + s.get("ek", ""))
    parcalar.append("dolgu=" + s.get("dolgu", {}).get("renk", "-"))
    parcalar.append("seffaf=" + ("1" if s.get("dolgu", {}).get("seffaf") else "0"))
    parcalar.append("saydamlik=" + str(s.get("dolgu", {}).get("saydamlik_yuzde", "-")))
    parcalar.append("cizgi=" + s.get("cizgi", {}).get("renk", "-"))
    parcalar.append("simge=" + s.get("simge_renk", "-"))
    gorsel = s.get("gorsel", {})
    for rol in ("cizgi_tipi", "sembol", "tarama"):
        liste = gorsel.get(rol, [])
        parcalar.append("%s=%s" % (rol, ",".join(liste) if liste else "-"))
    parcalar.append("belirsiz=" + ("1" if s.get("belirsiz") else "0"))
    return " ".join(parcalar)


def main():
    dizin = sys.argv[1]
    cikti = []

    # ------------------------------------------------------ plan gösterimleri --
    g = oku(os.path.join(dizin, "plan-gosterim.json"))
    cikti.append("paket %s %s sema=%d" % (g["id"], g["package_version"], g["schema_version"]))
    cikti.append("paket-published %s" % g["published"])
    cikti.append("gosterim-satir %d" % len(g["stiller"]))
    ekler = {}
    for s in g["stiller"]:
        ekler[s["ek"]] = ekler.get(s["ek"], 0) + 1
    for ek in sorted(ekler):
        cikti.append("gosterim-ek %s %d" % (ek, ekler[ek]))
    cikti.append("gosterim-dolgu-renk %d"
                 % sum(1 for s in g["stiller"] if "renk" in s.get("dolgu", {})))
    cikti.append("gosterim-seffaf %d"
                 % sum(1 for s in g["stiller"] if s.get("dolgu", {}).get("seffaf")))
    cikti.append("gosterim-cizgi-renk %d"
                 % sum(1 for s in g["stiller"] if "renk" in s.get("cizgi", {})))
    cikti.append("gosterim-simge-renk %d"
                 % sum(1 for s in g["stiller"] if "simge_renk" in s))
    cikti.append("gosterim-gorsel %d" % len(g["gorseller"]))
    cikti.append("gosterim-kural %d" % len(g["kurallar"]))

    nedenler = {}
    for s in g["stiller"]:
        for n in s.get("belirsiz_nedeni", []):
            nedenler[n] = nedenler.get(n, 0) + 1
    cikti.append("gosterim-belirsiz %d"
                 % sum(1 for s in g["stiller"] if s.get("belirsiz")))
    for n in sorted(nedenler):
        cikti.append("gosterim-belirsiz-neden %s %d" % (n, nedenler[n]))
    for s in g["stiller"]:
        if s.get("belirsiz"):
            cikti.append("gosterim-belirsiz-satir %s %s" % (s["id"], ",".join(s["belirsiz_nedeni"])))

    stiller = {s["id"]: s for s in g["stiller"]}
    for kimlik in REFERANS_STILLER:
        s = stiller.get(kimlik)
        cikti.append("gosterim-referans %s %s" % (kimlik, stil_satiri(s) if s else "YOK"))

    # ---------------------------------------------------------- detay kataloğu --
    d = oku(os.path.join(dizin, "detay-katalogu.json"))
    cikti.append("detay-paket %s %s sema=%d"
                 % (d["id"], d["package_version"], d["schema_version"]))
    cikti.append("detay-kart %d" % len(d["detaylar"]))
    cikti.append("detay-gorsel %d" % len(d["gorseller"]))
    cikti.append("detay-renkli %d" % sum(1 for x in d["detaylar"] if "renkler" in x))
    cikti.append("detay-kalinlikli %d"
                 % sum(1 for x in d["detaylar"] if "cizgi_kalinliklari" in x))
    cikti.append("detay-belirsiz %d" % sum(1 for x in d["detaylar"] if x.get("belirsiz")))
    for x in d["detaylar"]:
        if x.get("belirsiz"):
            cikti.append("detay-belirsiz-kart %s %s" % (x["id"], ",".join(x["belirsiz_nedeni"])))
    detaylar = {x["id"]: x for x in d["detaylar"]}
    for kimlik in REFERANS_DETAYLAR:
        x = detaylar.get(kimlik)
        if not x:
            cikti.append("detay-referans %s YOK" % kimlik)
            continue
        renkler = ";".join("%s=%s" % (r["rol"], r["renk"]) for r in x.get("renkler", []))
        kalin = ";".join("%s/%s=%d" % (k["plan"], k["rol"], k["kalinlik_um"])
                         for k in x.get("cizgi_kalinliklari", []))
        cikti.append("detay-referans %s planlar=%s renkler=%s kalinlik=%s"
                     % (kimlik, ",".join(x.get("planlar", [])) or "-",
                        renkler or "-", kalin or "-"))

    # ------------------------------------------------------ asgari standartlar --
    a = oku(os.path.join(dizin, "asgari-standartlar.json"))
    cikti.append("standart-paket %s %s sema=%d"
                 % (a["id"], a["package_version"], a["schema_version"]))
    cikti.append("standart-published %s" % a["published"])
    cikti.append("standart-kalem %d" % len(a["kalemler"]))
    cikti.append("standart-nufus-grubu %d" % len(a["nufus_gruplari"]))
    for ng in a["nufus_gruplari"]:
        cikti.append("standart-grup %s %s" % (ng["id"], ng["etiket"]))
    cikti.append("standart-aciklama %d" % len(a["aciklamalar"]))
    cikti.append("standart-belirsiz %d" % sum(1 for k in a["kalemler"] if k.get("belirsiz")))
    for k in a["kalemler"]:
        if k.get("belirsiz"):
            cikti.append("standart-belirsiz-kalem %s %s" % (k["id"], ",".join(k["belirsiz_nedeni"])))
    kalemler = {k["id"]: k for k in a["kalemler"]}
    for kimlik in REFERANS_KALEMLER:
        k = kalemler.get(kimlik)
        if not k:
            cikti.append("standart-referans %s YOK" % kimlik)
            continue
        degerler = ";".join(
            "%s:%s/%s-%s" % (v["nufus_grubu"], v.get("m2_kisi_binde", "-"),
                             v.get("asgari_alan_m2_en_az", "-"),
                             v.get("asgari_alan_m2_en_cok", "-"))
            for v in k.get("degerler", []))
        cikti.append("standart-referans %s %s" % (kimlik, degerler or "-"))

    # -------------------------------------------------------------- bayt izleri --
    # Elle düzenlemeyi yakalayan tek şey budur: üretilmiş dosyanın özeti.
    for ad in ("plan-gosterim.json", "detay-katalogu.json", "asgari-standartlar.json"):
        cikti.append("sha256 %s %s" % (ad, sha256(os.path.join(dizin, ad))))

    # Sembol dosyaları: sayı ve toplam bayt. Her birinin adı zaten içeriğinin
    # hash'i olduğundan ad listesi ayrıca doğrulanmaz.
    semboller = os.path.join(dizin, "semboller")
    if os.path.isdir(semboller):
        adlar = sorted(os.listdir(semboller))
        toplam = sum(os.path.getsize(os.path.join(semboller, x)) for x in adlar)
        cikti.append("sembol-dosya %d" % len(adlar))
        cikti.append("sembol-bayt %d" % toplam)
        h = hashlib.sha256()
        for x in adlar:
            h.update(x.encode("utf-8"))
            h.update(b"\n")
        cikti.append("sembol-liste-sha256 %s" % h.hexdigest())
    else:
        cikti.append("sembol-dosya 0")

    print("\n".join(cikti))
    return 0


if __name__ == "__main__":
    sys.exit(main())
