#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# MPYY gösterim eklerini makine okunur kataloğa çeviren çıkarım aracı.
#
# NEDEN VAR: CLAUDE.md 5.13 ve .claude/data.md R1 — mevzuat değeri C++'a giremez;
# bir mevzuat değişikliği veri sürümüdür, yeniden derleme değil. Kataloğun elle
# yazılması hem sürdürülemez hem denetlenemez olurdu: 1.500'ü aşkın satır, 897
# gömülü görsel. Bu betik kaynağı okur, katalogları üretir; sonuç yeniden
# üretilebilirdir (aynı kaynak -> bayt birebir aynı JSON).
#
# NE YAPMAZ: yorum yapmaz. Okunamayan bir renk, çözülemeyen bir satır, belirsiz
# bir ad uydurulmaz; satır `belirsiz: true` ve bir gerekçe koduyla işaretlenir.
# Uydurulmuş bir gösterim, imzalanan bir imar planına yanlış renk yazar.
#
# YALNIZ STANDART KÜTÜPHANE kullanır: docx/xlsx birer ZIP + XML'dir, bu iş için
# üçüncü parti bağımlılık gerekmez (Article 2.7'nin "hazır kütüphane" testini
# zipfile + xml.etree zaten geçiyor).
#
# Kullanım:
#   python3 scripts/mpyy-cikar.py --kaynak <dizin> [--depo <kok>] [--gorsel-yok]
#
# Kaynak dizin, Resmî Gazete'den indirilen ek dosyalarını içerir; depoya girmez.

import argparse
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
import zipfile

# ----------------------------------------------------------------- künye ----
# Bu sabitler kaynağın kendi metninden okunmuştur; betik bunları uydurmaz.
# EK-1a/1c/1ç/1d/1e: gövde metninde "(Değişik:RG-22/1/2026-33145)" damgası var.
# EK-1b: damga YOK; dosyanın docProps/core.xml'i 2014-06-10'da değiştirilmiş
#        olduğunu söylüyor, yani yönetmeliğin 14/6/2014-29030 sayılı ilk hâli.
# EK-2 : "(Değişik:RG-17/5/2017-30069)" damgası A1 hücresinde.
YONETMELIK = "Mekânsal Planlar Yapım Yönetmeliği (MPYY)"
RG_ILK = {"tarih": "2014-06-14", "rg": "RG-14/6/2014-29030"}
RG_2026 = {"tarih": "2026-01-22", "rg": "RG-22/1/2026-33145"}
RG_2017 = {"tarih": "2017-05-17", "rg": "RG-17/5/2017-30069"}

PAKET_SURUMU = "0.2.0"
SEMA_SURUMU_GOSTERIM = 2
SEMA_SURUMU_DETAY = 1
SEMA_SURUMU_STANDART = 1
LISANS = "resmi-mevzuat-metni"
ONAY = "BEKLİYOR"

# Kaynak dosya adları. Resmî Gazete'nin verdiği adlar bozuk kodlamalı olabildiği
# için ek kimliği dosya adından değil, bu tablodan gelir.
#
# Kimlik ön eki ek harfinden DEĞİL, planın resmî kısaltmasından türetilir:
# "EK-1c" ve "EK-1ç" ASCII'ye katlandığında ikisi de "ek-1c" olur ve kimlikler
# çakışırdı. MSP/ÇDP/NİP/UİP kısaltmaları EK-1e'nin kendi kullandığı kodlardır.
EKLER = [
    ("EK-1a", "ortak", "Ek-1a.docx", "ORTAK GÖSTERİMLER", RG_2026),
    ("EK-1b", "msp", "EK-1b Mek+ónsal Strateji Planlar¦- G+Âsterimleri.docx",
     "MEKÂNSAL STRATEJİ PLANI GÖSTERİMLERİ", RG_ILK),
    ("EK-1c", "cdp", "Ek-1c.docx", "ÇEVRE DÜZENİ PLANI GÖSTERİMLERİ", RG_2026),
    ("EK-1ç", "nip", "Ek-1ç.docx", "NAZIM İMAR PLANI GÖSTERİMLERİ", RG_2026),
    ("EK-1d", "uip", "Ek-1d.docx", "UYGULAMA İMAR PLANI GÖSTERİMLERİ", RG_2026),
]
EK_1E = ("EK-1e", "EK-1e.docx", "MEKÂNSAL PLANLAR DETAY KATALOGLARI", RG_2026)
EK_2 = ("EK-2", "EK-2.xlsx",
        "FARKLI NÜFUS GRUPLARINDA ASGARİ SOSYAL VE TEKNİK ALTYAPI ALANLARINA "
        "İLİŞKİN STANDARTLAR VE ASGARİ ALAN BÜYÜKLÜKLERİ TABLOSU", RG_2017)

# ------------------------------------------------------------ XML adları ----
W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
A = "{http://schemas.openxmlformats.org/drawingml/2006/main}"
R = "{http://schemas.openxmlformats.org/officeDocument/2006/relationships}"
V = "{urn:schemas-microsoft-com:vml}"
S = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
PR = "{http://schemas.openxmlformats.org/package/2006/relationships}"

# ------------------------------------------------------- Türkçe katlama ----
# CLAUDE.md 5.6: Türkçe metinde ASCII büyük/küçük dönüşümü kullanılamaz (i/I
# noktalı-noktasız). Kimlik üretiminde kullanılan katlama tablosu burada, açıkça.
KATLAMA = {
    "Ç": "C", "ç": "c", "Ğ": "G", "ğ": "g", "İ": "I", "ı": "i",
    "Ö": "O", "ö": "o", "Ş": "S", "ş": "s", "Ü": "U", "ü": "u",
    "Â": "A", "â": "a", "Î": "I", "î": "i", "Û": "U", "û": "u",
    "Ê": "E", "ê": "e", "Ô": "O", "ô": "o", "É": "E", "é": "e",
    "–": "-", "—": "-", "’": "", "‘": "", "“": "", "”": "",
}
ASCII_KUCUK = {chr(c): chr(c + 32) for c in range(ord("A"), ord("Z") + 1)}


def katla(metin):
    """Türkçe harfleri ASCII karşılığına indirir; başka hiçbir şeyi değiştirmez."""
    return "".join(KATLAMA.get(ch, ch) for ch in metin)


def slug(metin, azami=72):
    """Kalıcı kimlik parçası. data.md R5: kimlik sonsuza dek sabittir."""
    out = []
    for ch in katla(metin):
        ch = ASCII_KUCUK.get(ch, ch)
        if ("a" <= ch <= "z") or ("0" <= ch <= "9"):
            out.append(ch)
        else:
            out.append("-")
    s = re.sub(r"-+", "-", "".join(out)).strip("-")
    if len(s) > azami:
        kesme = s[:azami].rfind("-")
        s = s[:kesme] if kesme > azami // 2 else s[:azami]
        s = s.strip("-")
    return s or "adsiz"


# ------------------------------------------------------------ renk okuma ----
RGB_DESENI = re.compile(r"(?<![0-9/])([0-9]{1,3})\s*/\s*([0-9]{1,3})\s*/\s*([0-9]{1,3})(?![0-9/])")
SAYDAM_DESENI = re.compile(r"%\s*([0-9]{1,3})\s*[Ss]aydam")
SEFFAF_DESENI = re.compile(r"ŞEFFAF|Şeffaf|şeffaf")
# "ÇİZGİ KALINLIĞI: 0.8 mm" — yalnız hücrenin BAŞINDA ve araya harf girmeden.
# "ÇİZGİ KALINLIĞIı: Cephe Çizgisi 2.5 mm, Kaldırım ... 0.3 mm" gibi çok değerli
# metinler bilerek eşleşmez: hangi kalınlığın hangi çizgiye ait olduğu ancak
# uzman gözüyle ayrılır, betik bunu tahmin etmez.
KALINLIK_DESENI = re.compile(r"^ÇİZGİ\s*KALINLIĞI\s*ı?\s*:?\s*([0-9]+(?:[.,][0-9]+)?)\s*mm")


def renkleri_bul(metin):
    """Metindeki bütün R/G/B üçlülerini sırayla döndürür. 255'i aşan değer atılır."""
    bulunan = []
    for m in RGB_DESENI.finditer(metin):
        r, g, b = (int(m.group(i)) for i in (1, 2, 3))
        if r > 255 or g > 255 or b > 255:
            continue
        bulunan.append("#%02X%02X%02X" % (r, g, b))
    return bulunan


def saydamlik_bul(metin):
    m = SAYDAM_DESENI.search(metin)
    if not m:
        return None
    deger = int(m.group(1))
    return deger if 0 <= deger <= 100 else None


def kalinlik_um_bul(metin):
    """Kâğıt mikrometresi (1000 = 1 mm). model.md R20: kalınlık kâğıt ölçüsüdür."""
    m = KALINLIK_DESENI.match(metin.strip())
    if not m:
        return None
    ham = m.group(1).replace(",", ".")
    tam, _, kesir = ham.partition(".")
    kesir = (kesir + "000")[:3]
    return int(tam) * 1000 + int(kesir)


# --------------------------------------------------------- docx yardımı ----
class Belge:
    """Bir .docx dosyası: gövde XML'i, ilişki tablosu ve gömülü görseller."""

    def __init__(self, yol, gorsel_havuzu):
        self.yol = yol
        self.zip = zipfile.ZipFile(yol)
        self.kok = ET.fromstring(self.zip.read("word/document.xml"))
        self.govde = self.kok.find(W + "body")
        self.iliskiler = {}
        try:
            rels = ET.fromstring(self.zip.read("word/_rels/document.xml.rels"))
        except KeyError:
            rels = None
        if rels is not None:
            for rel in rels:
                self.iliskiler[rel.get("Id")] = rel.get("Target")
        self.havuz = gorsel_havuzu

    def gorsel_kimligi(self, rid, ek):
        """rId -> içerik hash'inden türetilmiş kalıcı sembol kimliği."""
        hedef = self.iliskiler.get(rid)
        if not hedef:
            return None
        ad = "word/" + hedef.lstrip("/") if not hedef.startswith("word/") else hedef
        ad = ad.replace("word/../", "")
        try:
            veri = self.zip.read(ad)
        except KeyError:
            return None
        return self.havuz.ekle(veri, os.path.splitext(ad)[1].lower(), ek)


class GorselHavuzu:
    """Aynı sembol birden çok satırda geçer; tek dosya, tek kimlik tutulur."""

    def __init__(self):
        self.kayitlar = {}

    def ekle(self, veri, uzanti, ek):
        sha = hashlib.sha256(veri).hexdigest()
        kimlik = "gorsel-" + sha[:16]
        kayit = self.kayitlar.get(kimlik)
        if kayit is None:
            kayit = {
                "id": kimlik,
                "dosya": "semboller/" + sha[:16] + uzanti,
                "sha256": sha,
                "bayt": len(veri),
                "tur": uzanti.lstrip("."),
                "ekler": set(),
                "_veri": veri,
            }
            self.kayitlar[kimlik] = kayit
        kayit["ekler"].add(ek)
        return kimlik

    def liste(self, yalniz=None):
        out = []
        for kimlik in sorted(self.kayitlar):
            k = self.kayitlar[kimlik]
            if yalniz is not None and kimlik not in yalniz:
                continue
            out.append({
                "id": k["id"],
                "dosya": k["dosya"],
                "sha256": k["sha256"],
                "bayt": k["bayt"],
                "tur": k["tur"],
                "ekler": sorted(k["ekler"]),
            })
        return out

    def yaz(self, dizin):
        os.makedirs(dizin, exist_ok=True)
        yazilan = 0
        for kimlik in sorted(self.kayitlar):
            k = self.kayitlar[kimlik]
            hedef = os.path.join(dizin, os.path.basename(k["dosya"]))
            veri = k["_veri"]
            if os.path.exists(hedef):
                with open(hedef, "rb") as f:
                    if f.read() == veri:
                        continue
            with open(hedef, "wb") as f:
                f.write(veri)
            yazilan += 1
        return yazilan


def hucre_metni(hucre):
    """Paragraflar tek boşlukla birleşir; hücre içi satır sonu ad değildir."""
    parcalar = []
    for p in hucre.iter(W + "p"):
        t = "".join(t.text or "" for t in p.iter(W + "t")).strip()
        if t:
            parcalar.append(t)
    return re.sub(r"\s+", " ", " ".join(parcalar)).strip()


def hucre_gorselleri(hucre, belge, ek):
    """Hücredeki görseller, belge sırasıyla. DrawingML ve VML birlikte taranır."""
    kimlikler = []
    for el in hucre.iter():
        rid = None
        if el.tag == A + "blip":
            rid = el.get(R + "embed")
        elif el.tag == V + "imagedata":
            rid = el.get(R + "id")
        if not rid:
            continue
        kimlik = belge.gorsel_kimligi(rid, ek)
        if kimlik and kimlik not in kimlikler:
            kimlikler.append(kimlik)
    return kimlikler


def hucre_acikligi(hucre):
    tcpr = hucre.find(W + "tcPr")
    span, birlesik = 1, ""
    if tcpr is not None:
        g = tcpr.find(W + "gridSpan")
        if g is not None:
            try:
                span = int(g.get(W + "val"))
            except (TypeError, ValueError):
                span = 1
        v = tcpr.find(W + "vMerge")
        if v is not None:
            birlesik = v.get(W + "val") or "devam"
            if birlesik == "restart":
                birlesik = "bas"
    return span, birlesik


def satir_hucreleri(satir, belge, ek):
    """Satırı (grid, span, metin, görseller, birleşim) dörtlülerine açar."""
    out = []
    grid = 0
    for hucre in satir:
        if hucre.tag != W + "tc":
            continue
        span, birlesik = hucre_acikligi(hucre)
        out.append({
            "grid": grid,
            "span": span,
            "metin": hucre_metni(hucre),
            "gorseller": hucre_gorselleri(hucre, belge, ek),
            "birlesik": birlesik,
        })
        grid += span
    return out


def tablolar(govde):
    return [el for el in govde if el.tag == W + "tbl"]


def satirlar(tablo):
    return [r for r in tablo if r.tag == W + "tr"]


# ------------------------------------------- EK-1a/1b/1c/1ç/1d çıkarımı ----
BASLIK_ROLLERI = {
    "ÇİZGİ TİPİ": "cizgi_tipi",
    "SINIR": "cizgi_tipi",
    "SEMBOL": "sembol",
    "TARAMA": "tarama",
    "ALAN RENK KODU (RGB)": "alan_renk",
}


def gosterim_ekini_cikar(belge, ek, on_ek, plan_adi, kaynak_damga, gunluk):
    """Bir gösterim ekinin bütün satırlarını, uydurmadan, sırayla çıkarır."""
    tbl = tablolar(belge.govde)
    if not tbl:
        gunluk.append("%s: belgede tablo yok" % ek)
        return []
    ana = max(tbl, key=lambda t: len(satirlar(t)))
    rows = satirlar(ana)

    # Başlık satırı: gösterim sütunlarını adlandıran ilk satır.
    baslik_indeksi = None
    basliklar = {}
    for i, satir in enumerate(rows):
        hucreler = satir_hucreleri(satir, belge, ek)
        metinler = [h["metin"] for h in hucreler]
        if any(m in BASLIK_ROLLERI for m in metinler if m):
            baslik_indeksi = i
            for h in hucreler:
                basliklar[h["grid"]] = h["metin"]
            break
    if baslik_indeksi is None:
        gunluk.append("%s: sütun başlığı satırı bulunamadı" % ek)
        return []

    sonuc = []
    bolum1 = ""
    bolum2 = ""
    birlesim = {}          # grid -> son 'bas' hücresi
    grup_kimligi = {}      # grid -> o grubun ilk satırının kimliği
    kullanilan = {}
    sayac = 0

    for satir in rows[baslik_indeksi + 1:]:
        hucreler = satir_hucreleri(satir, belge, ek)
        if not hucreler:
            continue
        dolu = [h for h in hucreler if h["metin"] or h["gorseller"]]

        # Tek hücreli satır: bölüm başlığı (ör. SINIRLAR). Boşsa ayraçtır.
        if len(hucreler) == 1:
            if hucreler[0]["metin"]:
                bolum1 = hucreler[0]["metin"]
                bolum2 = ""
                birlesim.clear()
                grup_kimligi.clear()
            continue

        # İki hücreli ve ikincisi tamamen boş + geniş: alt bölüm başlığı.
        if (len(hucreler) == 2 and hucreler[1]["span"] >= 2
                and not hucreler[1]["metin"] and not hucreler[1]["gorseller"]):
            bolum2 = hucreler[0]["metin"]
            birlesim.clear()
            grup_kimligi.clear()
            continue

        if not dolu:
            continue

        # ---- veri satırı --------------------------------------------------
        sayac += 1
        belirsiz_nedeni = []
        sutunlar = []
        rol_hucre = {}
        ek_sutun_sayaci = 0

        for h in hucreler:
            kaynak = h
            if h["birlesik"] == "devam":
                onceki = birlesim.get(h["grid"])
                if onceki is not None:
                    kaynak = onceki
            elif h["birlesik"] == "bas":
                birlesim[h["grid"]] = h

            baslik = basliklar.get(h["grid"], "")
            if h["grid"] == 0:
                rol = "ad"
            elif baslik in BASLIK_ROLLERI:
                rol = BASLIK_ROLLERI[baslik]
            else:
                ek_sutun_sayaci += 1
                rol = "sutun_%d" % h["grid"]

            kayit = {
                "grid": h["grid"],
                "rol": rol,
                "baslik": baslik,
                "metin": kaynak["metin"],
                "gorseller": list(kaynak["gorseller"]),
                "birlesik": h["birlesik"],
            }
            sutunlar.append(kayit)
            rol_hucre.setdefault(rol, kayit)

        ad = rol_hucre.get("ad", {}).get("metin", "")
        if not ad:
            belirsiz_nedeni.append("ad-bos")

        temel = "%s-%s" % (on_ek, slug(ad or "adsiz"))
        kullanilan[temel] = kullanilan.get(temel, 0) + 1
        kimlik = temel if kullanilan[temel] == 1 else "%s-%d" % (temel, kullanilan[temel])

        # Birleşik hücre grubunun ilk satırı grubun kimliğini taşır.
        ad_hucre = next((h for h in hucreler if h["grid"] == 0), None)
        grup = ""
        if ad_hucre is not None:
            if ad_hucre["birlesik"] == "bas":
                grup_kimligi[0] = kimlik
                grup = kimlik
            elif ad_hucre["birlesik"] == "devam":
                grup = grup_kimligi.get(0, "")

        satir_kaydi = {
            "id": kimlik,
            "ad": ad,
            "ek": ek,
            "kaynak": "%s, %s (%s), %s" % (
                YONETMELIK, ek, kaynak_damga["rg"],
                " > ".join(x for x in (bolum1, bolum2) if x) or plan_adi),
        }
        bolum_yolu = [x for x in (bolum1, bolum2) if x]
        if bolum_yolu:
            satir_kaydi["bolum"] = bolum_yolu
        if grup:
            satir_kaydi["grup"] = grup

        # ---- görünüm: yalnız kaynakta yazan ------------------------------
        cizgi = {}
        dolgu = {}
        renk_secenekleri = []
        gorsel = {}
        sutun_metinleri = {}
        simge_renk = None

        for kayit in sutunlar:
            rol = kayit["rol"]
            metin = kayit["metin"]
            if kayit["gorseller"] and rol in ("cizgi_tipi", "sembol", "tarama"):
                gorsel.setdefault(rol, [])
                for g in kayit["gorseller"]:
                    if g not in gorsel[rol]:
                        gorsel[rol].append(g)
            if not metin:
                continue
            renkler = renkleri_bul(metin)
            tekil = []
            for r in renkler:
                if r not in tekil:
                    tekil.append(r)
            # Gösterim sütunlarındaki düz metin (ör. "_ _RA_ _", "KKKK KK KK")
            # olduğu gibi saklanır; ne olduğu hakkında hüküm verilmez.
            if rol in ("cizgi_tipi", "sembol", "tarama") and not tekil:
                sutun_metinleri[rol] = metin
            if rol == "alan_renk":
                if SEFFAF_DESENI.search(metin):
                    dolgu["seffaf"] = True
                if len(tekil) == 1:
                    dolgu["renk"] = tekil[0]
                    saydam = saydamlik_bul(metin)
                    if saydam is not None:
                        dolgu["saydamlik_yuzde"] = saydam
                elif len(tekil) > 1:
                    belirsiz_nedeni.append("alan-renk-coklu")
                    for r in tekil:
                        renk_secenekleri.append(
                            {"rol": rol, "renk": r, "not": metin})
                elif not SEFFAF_DESENI.search(metin):
                    belirsiz_nedeni.append("alan-renk-cozulemedi")
            elif rol == "cizgi_tipi":
                if len(tekil) == 1:
                    cizgi["renk"] = tekil[0]
                elif len(tekil) > 1:
                    belirsiz_nedeni.append("cizgi-renk-coklu")
                    for r in tekil:
                        renk_secenekleri.append({"rol": rol, "renk": r, "not": metin})
            elif rol == "sembol":
                if len(tekil) == 1:
                    simge_renk = tekil[0]
                elif len(tekil) > 1:
                    belirsiz_nedeni.append("simge-renk-coklu")
                    for r in tekil:
                        renk_secenekleri.append({"rol": rol, "renk": r, "not": metin})

        if cizgi:
            satir_kaydi["cizgi"] = cizgi
        if dolgu:
            satir_kaydi["dolgu"] = dolgu
        if simge_renk:
            satir_kaydi["simge_renk"] = simge_renk
        if gorsel:
            satir_kaydi["gorsel"] = {k: gorsel[k] for k in sorted(gorsel)}
        if sutun_metinleri:
            satir_kaydi["sutun_metinleri"] = {k: sutun_metinleri[k]
                                              for k in sorted(sutun_metinleri)}
        if renk_secenekleri:
            satir_kaydi["renk_secenekleri"] = renk_secenekleri

        gorunum_var = bool(cizgi or dolgu or gorsel or renk_secenekleri
                           or sutun_metinleri or simge_renk)
        if not gorunum_var:
            belirsiz_nedeni.append("gosterim-sutunlari-bos")

        satir_kaydi["sutunlar"] = sutunlar
        if belirsiz_nedeni:
            satir_kaydi["belirsiz"] = True
            satir_kaydi["belirsiz_nedeni"] = sorted(set(belirsiz_nedeni))
        sonuc.append(satir_kaydi)

    gunluk.append("%s: %d satır" % (ek, len(sonuc)))
    return sonuc


# --------------------------------------------------------- EK-1e kartlar ----
EK1E_BASLIK_KUMESI = {"SEMBOL", "TARAMA", "RGB", "TİPİ"}
EK1E_GEOMETRI = {"ALAN", "NOKTA", "ÇİZGİ", "SINIR", "ALAN/ NOKTA", "ALAN/NOKTA",
                 "ALAN/ ÇİZGİ", "NOKTA/ ÇİZGİ"}
PLAN_KODLARI = {"MSP", "ÇDP", "NİP", "UİP"}


def _ek1e_baslik_satiri_mi(metinler):
    dolu = [m for m in metinler if m]
    if not dolu:
        return False
    for m in dolu:
        if m in EK1E_BASLIK_KUMESI:
            continue
        if m.startswith("TİPİ"):
            continue
        return False
    return True


def ek1e_cikar(belge, ek, kaynak_damga, gunluk):
    """EK-1e kart tablolarını çıkarır. Tanınmayan satır atılmaz, kaydedilir."""
    kartlar = []
    bekleyen = []
    bolum = ""
    detay_sinifi = ""
    kullanilan = {}
    komsu_bekleyen = {}

    ogeler = list(belge.govde)
    for el in ogeler:
        if el.tag == W + "p":
            t = re.sub(r"\s+", " ", "".join(x.text or "" for x in el.iter(W + "t"))).strip()
            if t:
                bekleyen.append(t)
            continue
        if el.tag != W + "tbl":
            continue

        rows = satirlar(el)
        if not rows:
            bekleyen = []
            continue
        acilmis = [satir_hucreleri(r, belge, ek) for r in rows]
        ilk_metinler = [h["metin"] for h in acilmis[0]]

        kart_mi = any("DETAY SINIFI" in [h["metin"] for h in hs] for hs in acilmis)
        if not kart_mi:
            # Bölüm başlığı tablosu (ör. "I- MEKÂNSAL STRATEJİ PLANI DETAY KATALOĞU")
            if ilk_metinler and ilk_metinler[0]:
                bolum = ilk_metinler[0]
                detay_sinifi = ""
            bekleyen = []
            continue

        kart = ek1e_kart_oku(acilmis, ek, bolum, bekleyen, kaynak_damga,
                             detay_sinifi, kullanilan)
        detay_sinifi = kart.pop("_sinif_tasima", detay_sinifi)
        komsu_bekleyen[kart["id"]] = list(bekleyen)
        kartlar.append(kart)
        bekleyen = []

    # Ad paragrafı olmayan kartın komşusunu görebilmesi için: bir sonraki kartın
    # bekleyen paragrafları aday olarak kaydedilir. Betik SEÇİM YAPMAZ.
    for i, kart in enumerate(kartlar):
        if "ad-paragrafi-yok" in kart.get("belirsiz_nedeni", []):
            if i + 1 < len(kartlar):
                adaylar = komsu_bekleyen.get(kartlar[i + 1]["id"], [])
                if adaylar:
                    kart["ad_adaylari"] = list(adaylar)

    gunluk.append("%s: %d kart" % (ek, len(kartlar)))
    return kartlar


def ek1e_kart_oku(acilmis, ek, bolum, bekleyen, kaynak_damga, devralinan_sinif,
                  kullanilan):
    belirsiz_nedeni = []
    ad = ""
    ad_adaylari = []
    detay_sinifi = devralinan_sinif
    detay_alt_sinifi = ""
    gosterim_bloklari = []
    plan_notlari = None
    cozulmemis = []

    # Kart adı: tablonun içindeki tek hücreli ilk satır ya da tablodan önceki
    # paragraf(lar). Hiçbiri yoksa ya da birden fazlaysa: belirsiz.
    if acilmis and len(acilmis[0]) == 1 and acilmis[0][0]["metin"]:
        ad = acilmis[0][0]["metin"]
    elif len(bekleyen) == 1:
        ad = bekleyen[0]
    elif len(bekleyen) == 0:
        belirsiz_nedeni.append("ad-paragrafi-yok")
    else:
        ad = " ".join(bekleyen)
        ad_adaylari = list(bekleyen)
        belirsiz_nedeni.append("ad-paragraf-sayisi-belirsiz")

    i = 0
    gosterim_basladi = False
    aktif_geometri = []
    while i < len(acilmis):
        hucreler = acilmis[i]
        metinler = [h["metin"] for h in hucreler]
        ilk = metinler[0] if metinler else ""

        if ilk == "DETAY SINIFI":
            deger = metinler[1] if len(metinler) > 1 else ""
            if deger:
                detay_sinifi = deger
            i += 1
            continue
        if ilk == "DETAY ALT SINIFI":
            detay_alt_sinifi = metinler[1] if len(metinler) > 1 else ""
            i += 1
            continue
        if len(hucreler) == 1 and "GÖSTERİM" in ilk:
            gosterim_basladi = True
            i += 1
            continue
        if len(hucreler) == 1 and i == 0 and ilk:
            i += 1
            continue
        if any("GEOMETRİ TİPİ" in m for m in metinler):
            plan_notlari = {"sutunlar": metinler, "satirlar": []}
            i += 1
            while i < len(acilmis):
                sat = [h["metin"] for h in acilmis[i]]
                gors = [h["gorseller"] for h in acilmis[i]]
                if any(g for g in gors):
                    plan_notlari["satirlar"].append(
                        [{"metin": m, "gorseller": g} for m, g in zip(sat, gors)])
                else:
                    plan_notlari["satirlar"].append(
                        [{"metin": m, "gorseller": []} for m in sat])
                i += 1
            continue
        if gosterim_basladi and all((m in EK1E_GEOMETRI) or not m for m in metinler) \
                and any(metinler):
            # Grup satırı: hangi sütun aralığının ALAN'a, hangisinin SINIR'a ait
            # olduğunu kaynağın kendi gridSpan'inden okuruz. İki ayrı "RGB"
            # sütunu ancak böyle ayrılabilir.
            aktif_geometri = [{"etiket": h["metin"], "grid": h["grid"],
                               "span": h["span"]} for h in hucreler if h["metin"]]
            i += 1
            continue
        if gosterim_basladi and _ek1e_baslik_satiri_mi(metinler):
            nitelikli = []
            for h in hucreler:
                grup = ""
                for g in aktif_geometri:
                    if g["grid"] <= h["grid"] < g["grid"] + g["span"]:
                        grup = g["etiket"]
                        break
                nitelikli.append("%s/%s" % (grup, h["metin"]) if grup else h["metin"])
            blok = {"geometri": [g["etiket"] for g in aktif_geometri],
                    "sutunlar": nitelikli, "satirlar": []}
            i += 1
            while i < len(acilmis):
                sonraki = [h["metin"] for h in acilmis[i]]
                if _ek1e_baslik_satiri_mi(sonraki):
                    break
                if any("GEOMETRİ TİPİ" in m for m in sonraki):
                    break
                if all((m in EK1E_GEOMETRI) or not m for m in sonraki) and any(sonraki):
                    break
                blok["satirlar"].append([
                    {"metin": h["metin"], "gorseller": list(h["gorseller"])}
                    for h in acilmis[i]])
                i += 1
            gosterim_bloklari.append(blok)
            continue

        if any(m or h["gorseller"] for m, h in zip(metinler, hucreler)):
            cozulmemis.append([{"metin": h["metin"], "gorseller": list(h["gorseller"])}
                               for h in hucreler])
        i += 1

    if cozulmemis:
        belirsiz_nedeni.append("cozulmemis-satir")
    if not gosterim_bloklari:
        belirsiz_nedeni.append("gosterim-blogu-yok")

    temel = "ek1e-%s" % slug(ad or (detay_alt_sinifi or "adsiz"))
    kullanilan[temel] = kullanilan.get(temel, 0) + 1
    kimlik = temel if kullanilan[temel] == 1 else "%s-%d" % (temel, kullanilan[temel])

    kart = {
        "id": kimlik,
        "ad": ad,
        "ek": ek,
        "bolum": bolum,
        "detay_sinifi": detay_sinifi,
        "detay_alt_sinifi": detay_alt_sinifi,
        "kaynak": "%s, %s (%s), %s" % (
            YONETMELIK, ek, kaynak_damga["rg"], bolum or ek),
    }
    if ad_adaylari:
        kart["ad_adaylari"] = ad_adaylari

    # ---- türetilenler: yalnız tek anlamlı olanlar --------------------------
    renkler = []
    seffaf = False
    for blok in gosterim_bloklari:
        for satir in blok["satirlar"]:
            for sutun_no, hucre in enumerate(satir):
                baslik = blok["sutunlar"][sutun_no] if sutun_no < len(blok["sutunlar"]) else ""
                metin = hucre["metin"]
                if not metin:
                    continue
                if SEFFAF_DESENI.search(metin):
                    seffaf = True
                for r in renkleri_bul(metin):
                    kayit = {"rol": baslik or ("sutun_%d" % sutun_no), "renk": r,
                             "kaynak_metin": metin}
                    saydam = saydamlik_bul(metin)
                    if saydam is not None:
                        kayit["saydamlik_yuzde"] = saydam
                    if kayit not in renkler:
                        renkler.append(kayit)

    planlar = []
    kalinliklar = []
    if plan_notlari:
        for satir in plan_notlari["satirlar"]:
            if not satir:
                continue
            plan = satir[0]["metin"]
            if plan in PLAN_KODLARI and plan not in planlar:
                planlar.append(plan)
            for sutun_no, hucre in enumerate(satir):
                baslik = (plan_notlari["sutunlar"][sutun_no]
                          if sutun_no < len(plan_notlari["sutunlar"]) else "")
                um = kalinlik_um_bul(hucre["metin"])
                if um is not None:
                    kalinliklar.append({
                        "plan": plan,
                        "rol": baslik or ("sutun_%d" % sutun_no),
                        "kalinlik_um": um,
                        "kaynak_metin": hucre["metin"],
                    })

    if gosterim_bloklari:
        kart["gosterim"] = gosterim_bloklari
    if plan_notlari:
        kart["plan_notlari"] = plan_notlari
    if planlar:
        kart["planlar"] = planlar
    if renkler:
        kart["renkler"] = renkler
    if seffaf:
        kart["seffaf"] = True
    if kalinliklar:
        kart["cizgi_kalinliklari"] = kalinliklar
    if cozulmemis:
        kart["cozulmemis_satirlar"] = cozulmemis
    if belirsiz_nedeni:
        kart["belirsiz"] = True
        kart["belirsiz_nedeni"] = sorted(set(belirsiz_nedeni))
    kart["_sinif_tasima"] = detay_sinifi
    return kart


# ------------------------------------------------------------ EK-2 tablo ----
# m²/kişi sütunlarında ondalık ayırıcı '.' veya ',' olabilir (0.5, 2.00, 10,00).
# Asgari birim alan sütunlarında '.' BİNLER ayırıcısıdır (1.500-3.000 = 1500-3000
# m²). İki kural, iki ayrı sütun ailesi içindir ve ikisi de kaynağın kendi
# yazımından okunmuştur; karışık bir hücre çözülmez, ham hâliyle kalır.
KISI_DESENI = re.compile(r"^([0-9]+)(?:[.,]([0-9]{1,3}))?$")
ARALIK_DESENI = re.compile(r"^([0-9]{1,3}(?:\.[0-9]{3})*|[0-9]+)"
                           r"(?:\s*-\s*([0-9]{1,3}(?:\.[0-9]{3})*|[0-9]+))?$")


def kisi_basi_binde(metin):
    """m²/kişi -> binde tam sayı (0.5 -> 500). Kayan nokta saklanmaz."""
    m = KISI_DESENI.match(metin.strip())
    if not m:
        return None
    kesir = (m.group(2) or "")
    kesir = (kesir + "000")[:3]
    return int(m.group(1)) * 1000 + int(kesir)


def alan_araligi(metin):
    """'1.500-3.000' -> (1500, 3000); '3000' -> (3000, 3000)."""
    m = ARALIK_DESENI.match(metin.strip())
    if not m:
        return None
    az = int(m.group(1).replace(".", ""))
    cok = int(m.group(2).replace(".", "")) if m.group(2) else az
    if cok < az:
        return None
    return az, cok


def _hucre_ref(sutun, satir):
    return "%s%d" % (sutun, satir)


def _sutun_araligi(bas, son):
    """Tek harfli sütunlar yeter: EK-2 tablosu A..K arasındadır."""
    return [chr(c) for c in range(ord(bas), ord(son) + 1)]


def _birlesimleri_coz(hucreler, birlesimler):
    """Birleşik hücrenin değeri kapsadığı bütün gözlere yayılır.

    Bu bir yorum değil, biçimin kendi anlamıdır: A5:B12 birleşimi 'EĞİTİM
    TESİSLERİ ALANI' başlığının 5-12. satırları kapsadığını söyler. Yayılan
    değerler işaretlenir ki tüketici bunun satıra özel değil, gruba ait bir
    değer olduğunu bilsin.
    """
    yayilan = {}
    for ref in birlesimler:
        if ":" not in ref:
            continue
        bas, son = ref.split(":", 1)
        m1 = re.match(r"^([A-Z]+)([0-9]+)$", bas)
        m2 = re.match(r"^([A-Z]+)([0-9]+)$", son)
        if not m1 or not m2 or len(m1.group(1)) != 1 or len(m2.group(1)) != 1:
            continue
        deger = hucreler.get(bas, "")
        if not deger:
            continue
        for sutun in _sutun_araligi(m1.group(1), m2.group(1)):
            for satir in range(int(m1.group(2)), int(m2.group(2)) + 1):
                hedef = _hucre_ref(sutun, satir)
                if hedef == bas:
                    continue
                if hedef not in hucreler:
                    hucreler[hedef] = deger
                    yayilan[hedef] = bas
    return yayilan


def ek2_cikar(yol, gunluk):
    z = zipfile.ZipFile(yol)
    paylasilan = []
    if "xl/sharedStrings.xml" in z.namelist():
        kok = ET.fromstring(z.read("xl/sharedStrings.xml"))
        for si in kok:
            paylasilan.append("".join(t.text or "" for t in si.iter(S + "t")))
    sheet = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
    hucreler = {}
    for row in sheet.find(S + "sheetData"):
        for c in row:
            ref = c.get("r")
            t = c.get("t")
            v = c.find(S + "v")
            isel = c.find(S + "is")
            if t == "s" and v is not None:
                deger = paylasilan[int(v.text)]
            elif isel is not None:
                deger = "".join(x.text or "" for x in isel.iter(S + "t"))
            elif v is not None:
                deger = v.text or ""
            else:
                deger = ""
            deger = re.sub(r"\s+", " ", deger).strip()
            if deger:
                hucreler[ref] = deger

    birlesimler = []
    mc = sheet.find(S + "mergeCells")
    if mc is not None:
        birlesimler = [m.get("ref") for m in mc if m.get("ref")]
    birlesimler.sort()
    yayilan = _birlesimleri_coz(hucreler, birlesimler)

    # Nüfus grupları başlıkları: D3, F3, H3, J3; her grup (m²/kişi, asgari alan).
    gruplar = []
    for harf_kisi, harf_alan, kaynak in (("D", "E", "D3"), ("F", "G", "F3"),
                                         ("H", "I", "H3"), ("J", "K", "J3")):
        etiket = hucreler.get(kaynak, "")
        gruplar.append({
            "id": "nufus-" + slug(etiket or kaynak),
            "etiket": etiket,
            "_kisi": harf_kisi,
            "_alan": harf_alan,
        })

    # Satır grupları: A sütunundaki başlık aşağı taşınır, B ara başlık, C kalem.
    kalemler = []
    ust = ""
    orta = ""
    kullanilan = {}
    for satir_no in range(5, 38):
        a = hucreler.get("A%d" % satir_no, "")
        b = hucreler.get("B%d" % satir_no, "")
        c = hucreler.get("C%d" % satir_no, "")
        # A5:B12 gibi çok sütunlu birleşimler aynı metni B'ye de yayar; aynı
        # metin iki kez ad olmaz.
        if b == a:
            b = ""
        if c == a or c == b:
            c = ""
        if a:
            ust = a
            orta = ""
        if b:
            orta = b
        ad = c or b or a
        if not ad and not any(hucreler.get("%s%d" % (h, satir_no))
                              for h in "DEFGHIJK"):
            continue

        belirsiz_nedeni = []
        if not ad:
            belirsiz_nedeni.append("kalem-adi-bos")
        temel = "ek2-" + slug(ad or ("satir-%d" % satir_no))
        kullanilan[temel] = kullanilan.get(temel, 0) + 1
        kimlik = temel if kullanilan[temel] == 1 else "%s-%d" % (temel, kullanilan[temel])

        degerler = []
        for g in gruplar:
            kisi_ref = "%s%d" % (g["_kisi"], satir_no)
            alan_ref = "%s%d" % (g["_alan"], satir_no)
            kisi_metin = hucreler.get(kisi_ref, "")
            alan_metin = hucreler.get(alan_ref, "")
            kayit = {"nufus_grubu": g["id"]}
            if kisi_metin:
                kayit["m2_kisi_metin"] = kisi_metin
                binde = kisi_basi_binde(kisi_metin)
                if binde is None:
                    belirsiz_nedeni.append("m2-kisi-sayiya-cevrilemedi")
                else:
                    kayit["m2_kisi_binde"] = binde
                if kisi_ref in yayilan:
                    kayit["m2_kisi_birlesik_hucre"] = yayilan[kisi_ref]
            if alan_metin:
                kayit["asgari_alan_metin"] = alan_metin
                aralik = alan_araligi(alan_metin)
                if aralik is None:
                    belirsiz_nedeni.append("asgari-alan-sayiya-cevrilemedi")
                else:
                    kayit["asgari_alan_m2_en_az"] = aralik[0]
                    kayit["asgari_alan_m2_en_cok"] = aralik[1]
                if alan_ref in yayilan:
                    kayit["asgari_alan_birlesik_hucre"] = yayilan[alan_ref]
            if len(kayit) > 1:
                degerler.append(kayit)

        kalem = {
            "id": kimlik,
            "ad": ad,
            "kaynak": "%s, EK-2 (%s)" % (YONETMELIK, RG_2017["rg"]),
        }
        if ust and ust != ad:
            kalem["ust_grup"] = ust
        if orta and orta != ad:
            kalem["ara_grup"] = orta
        kalem["satir"] = satir_no
        if degerler:
            kalem["degerler"] = degerler
        else:
            belirsiz_nedeni.append("deger-yok")
        if belirsiz_nedeni:
            kalem["belirsiz"] = True
            kalem["belirsiz_nedeni"] = sorted(set(belirsiz_nedeni))
        kalemler.append(kalem)

    aciklamalar = []
    for satir_no in range(40, 60):
        metin = hucreler.get("A%d" % satir_no, "")
        if metin:
            aciklamalar.append(metin)

    for g in gruplar:
        g.pop("_kisi")
        g.pop("_alan")

    gunluk.append("EK-2: %d kalem, %d açıklama" % (len(kalemler), len(aciklamalar)))
    return gruplar, kalemler, aciklamalar


# ------------------------------------------------------------------ yazma ----
def json_yaz(yol, veri):
    os.makedirs(os.path.dirname(yol), exist_ok=True)
    metin = json.dumps(veri, ensure_ascii=False, indent=2) + "\n"
    with open(yol, "w", encoding="utf-8", newline="\n") as f:
        f.write(metin)
    return len(metin.encode("utf-8"))


def sema_dogrula(kok, sema_yolu, gunluk):
    """jsonschema kuruluysa doğrular; değilse sessizce geçmez, bildirir."""
    try:
        import jsonschema
    except ImportError:
        gunluk.append("şema doğrulaması atlandı: jsonschema kurulu değil")
        return True
    with open(sema_yolu, encoding="utf-8") as f:
        sema = json.load(f)
    try:
        jsonschema.validate(kok, sema)
    except jsonschema.ValidationError as hata:
        gunluk.append("ŞEMA HATASI %s: %s" % (os.path.basename(sema_yolu), hata))
        return False
    gunluk.append("şema OK: %s" % os.path.basename(sema_yolu))
    return True


def main():
    ap = argparse.ArgumentParser(description="MPYY gösterim eklerini kataloğa çevirir.")
    ap.add_argument("--kaynak", required=True, help="Resmî ek dosyalarının bulunduğu dizin")
    ap.add_argument("--depo", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    help="Depo kökü (varsayılan: bu betiğin üst dizini)")
    ap.add_argument("--gorsel-yok", action="store_true",
                    help="Sembol görsellerini diske yazma; katalog satırları yine üretilir")
    args = ap.parse_args()

    kaynak = args.kaynak
    hedef = os.path.join(args.depo, "data", "catalogs", "mpyy")
    sema_dizini = os.path.join(args.depo, "data", "catalogs", "schema")
    gunluk = []
    havuz = GorselHavuzu()

    for dosya in [e[2] for e in EKLER] + [EK_1E[1], EK_2[1]]:
        if not os.path.exists(os.path.join(kaynak, dosya)):
            sys.stderr.write("Kaynak dosya yok: %s\n" % os.path.join(kaynak, dosya))
            return 2

    # ---- EK-1a..1d gösterimleri ------------------------------------------
    stiller = []
    plan_turleri = []
    for ek, on_ek, dosya, plan_adi, damga in EKLER:
        belge = Belge(os.path.join(kaynak, dosya), havuz)
        stiller.extend(gosterim_ekini_cikar(belge, ek, on_ek, plan_adi, damga, gunluk))
        plan_turleri.append({
            "id": on_ek,
            "ad": plan_adi,
            "ek": ek,
            "yururluk": damga["rg"],
            "published": damga["tarih"],
        })

    kullanilan_gorseller = set()
    for s in stiller:
        for liste in s.get("gorsel", {}).values():
            kullanilan_gorseller.update(liste)

    belirsiz_stiller = [s for s in stiller if s.get("belirsiz")]
    rgb_var = sum(1 for s in stiller if "renk" in s.get("dolgu", {}))
    seffaf_var = sum(1 for s in stiller if s.get("dolgu", {}).get("seffaf"))

    gosterim = {
        "schema_version": SEMA_SURUMU_GOSTERIM,
        "package_version": PAKET_SURUMU,
        "id": "mpyy-plan-gosterimleri",
        "source": (
            "%s, EK-1 Gösterimler. EK-1a Ortak Gösterimler, EK-1c Çevre Düzeni Planı "
            "Gösterimleri, EK-1ç Nazım İmar Planı Gösterimleri, EK-1d Uygulama İmar "
            "Planı Gösterimleri: %s ile değişik hâl. EK-1b Mekânsal Strateji Planı "
            "Gösterimleri: kaynak dosyanın metninde değişiklik damgası YOKTUR; "
            "yönetmeliğin %s sayılı ilk hâli esas alınmıştır. Her satırın kendi ek "
            "ve bölüm atfı satırın 'kaynak' alanındadır."
        ) % (YONETMELIK, RG_2026["rg"], RG_ILK["rg"]),
        "published": RG_2026["tarih"],
        "licence": LISANS,
        "aciklama": (
            "MPYY gösterim eklerinin makine okunur hâli. Satırlar "
            "scripts/mpyy-cikar.py ile resmî ek dosyalarından çıkarılmıştır; elle "
            "düzenlenmez. Renk, çizgi kalınlığı, tarama ve simge yalnız bu dosyadan "
            "okunur, C++ içinde hiçbir mevzuat değeri yoktur (CLAUDE.md 5.13). "
            "Kaynakta okunamayan değer uydurulmaz: satır 'belirsiz' işaretlenir."
        ),
        "kapsam": {
            "durum": "satirlar-cikarildi-uzman-onayi-bekliyor",
            "iceren": [
                "EK-1a Ortak Gösterimler, EK-1b Mekânsal Strateji Planı, EK-1c Çevre "
                "Düzeni Planı, EK-1ç Nazım İmar Planı, EK-1d Uygulama İmar Planı "
                "gösterim satırları; toplam %d satır." % len(stiller),
                "Satır başına: ad, bölüm yolu, ek atfı, kaynak sütunlarının ham metni "
                "ve gömülü görsel kimlikleri.",
                "Alan renk kodu (RGB) çözülebilen %d satırda 'dolgu.renk' olarak, "
                "ŞEFFAF yazan %d satırda 'dolgu.seffaf' olarak verilmiştir."
                % (rgb_var, seffaf_var),
                "Çizgi tipi, sembol ve tarama görselleri 'semboller/' altında, "
                "içerik SHA-256'sından türetilmiş kalıcı adla.",
            ],
            "eksikler": [
                "Eşleme kuralları ('kurallar') boştur: hangi nesnenin hangi gösterim "
                "satırını alacağı yönetmelik ekinden okunamaz, plan türü ve öznitelik "
                "şemasıyla birlikte uzman kararıdır.",
                "Görseller ham bitmap/metafile'dır; çizgi deseni ve tarama deseni "
                "motorun indeks uzayına ('cizgi_desenleri', 'tarama_desenleri') "
                "çevrilmemiştir. Bu çeviri sembol atlası işidir ve ayrı bir sürümdür.",
                "'simge' alanı (sembol atlası indeksi) hiçbir satırda yoktur; atlas "
                "henüz üretilmemiştir. Sembolün kendisi 'gorsel.sembol' altındadır.",
                "Çizgi kalınlığı EK-1a..1d eklerinde verilmemiştir; kalınlıklar "
                "EK-1e Detay Kataloğu'ndadır (detay-katalogu.json).",
                "EK-1b'de 2. ve 3. sütunun başlığı kaynakta boştur; içerikleri "
                "'sutunlar' altında 'sutun_1'/'sutun_2' rolüyle ham hâlde durur.",
                "'belirsiz' işaretli %d satır vardır; gerekçe kodları satırın "
                "'belirsiz_nedeni' alanındadır." % len(belirsiz_stiller),
            ],
            "onay": ONAY,
            "sonraki_surum": (
                "0.3.0 — uzman onayı sonrası eşleme kuralları ve sembol atlası "
                "indeksleri (CLAUDE.md 6.11)."
            ),
        },
        "plan_turleri": plan_turleri,
        "cizgi_desenleri": [
            {"id": "surekli", "ad": "Sürekli çizgi", "indeks": 0},
            {"id": "kesik", "ad": "Kesik çizgi", "indeks": 1},
            {"id": "noktali", "ad": "Noktalı çizgi", "indeks": 2},
            {"id": "kesik-noktali", "ad": "Kesik-noktalı çizgi", "indeks": 3},
        ],
        "tarama_desenleri": [
            {"id": "yok", "ad": "Taramasız (düz dolgu)", "indeks": 0},
        ],
        "gorseller": havuz.liste(kullanilan_gorseller),
        "stiller": stiller,
        "kurallar": [],
    }

    # ---- EK-1e detay kataloğu --------------------------------------------
    belge_1e = Belge(os.path.join(kaynak, EK_1E[1]), havuz)
    detaylar = ek1e_cikar(belge_1e, EK_1E[0], EK_1E[3], gunluk)
    for d in detaylar:
        d.pop("_sinif_tasima", None)
    detay_gorseller = set()

    def _gorsel_topla(dugum):
        if isinstance(dugum, dict):
            for anahtar, deger in dugum.items():
                if anahtar == "gorseller" and isinstance(deger, list):
                    detay_gorseller.update(deger)
                else:
                    _gorsel_topla(deger)
        elif isinstance(dugum, list):
            for oge in dugum:
                _gorsel_topla(oge)

    _gorsel_topla(detaylar)
    belirsiz_detaylar = [d for d in detaylar if d.get("belirsiz")]

    detay_katalogu = {
        "schema_version": SEMA_SURUMU_DETAY,
        "package_version": PAKET_SURUMU,
        "id": "mpyy-detay-katalogu",
        "source": "%s, EK-1e Mekânsal Planlar Detay Katalogları (%s)" % (
            YONETMELIK, RG_2026["rg"]),
        "published": RG_2026["tarih"],
        "licence": LISANS,
        "aciklama": (
            "MPYY EK-1e Detay Kataloğu'nun makine okunur hâli. Her kart bir detayı "
            "anlatır: detay sınıfı, alt sınıfı, gösterim matrisi ve plan türü başına "
            "geometri tipi, tarama ve sınır açıklaması. scripts/mpyy-cikar.py ile "
            "üretilir; elle düzenlenmez."
        ),
        "kapsam": {
            "durum": "kartlar-cikarildi-uzman-onayi-bekliyor",
            "iceren": [
                "%d detay kartı." % len(detaylar),
                "Kart başına gösterim matrisi ham hâliyle ('gosterim'), plan türü "
                "notları ('plan_notlari') ve çözülebilen türetmeler ('renkler', "
                "'cizgi_kalinliklari', 'planlar').",
                "Çizgi kalınlığı, yalnız hücre 'ÇİZGİ KALINLIĞI: <sayı> mm' ile "
                "başlıyorsa mikrometreye çevrilmiştir (1000 = 1 mm).",
            ],
            "eksikler": [
                "Detay kodu YOKTUR: EK-1e resmî metni detaylara sayısal kod vermez, "
                "kart adı + detay sınıfı ile anar. Kimlikler ad üzerinden üretilmiştir.",
                "Birden çok kalınlık içeren açıklama hücreleri ('Cephe Çizgisi 2.5 mm, "
                "Kaldırım ... 0.3 mm') çevrilmemiştir; ham metin 'plan_notlari' "
                "içindedir.",
                "Tarama ve sınır tipi tarifleri serbest metindir; desen üreticisine "
                "çevrilmeleri ayrı bir iştir.",
                "'belirsiz' işaretli %d kart vardır." % len(belirsiz_detaylar),
            ],
            "onay": ONAY,
            "sonraki_surum": "0.3.0 — detay kartı -> gösterim satırı eşlemesi.",
        },
        "gorseller": havuz.liste(detay_gorseller),
        "detaylar": detaylar,
    }

    # ---- EK-2 asgari standartlar -----------------------------------------
    gruplar, kalemler, aciklamalar = ek2_cikar(os.path.join(kaynak, EK_2[1]), gunluk)
    belirsiz_kalemler = [k for k in kalemler if k.get("belirsiz")]
    standartlar = {
        "schema_version": SEMA_SURUMU_STANDART,
        "package_version": PAKET_SURUMU,
        "id": "mpyy-asgari-standartlar",
        "source": "%s, EK-2 %s (%s)" % (YONETMELIK, EK_2[2], RG_2017["rg"]),
        "published": RG_2017["tarih"],
        "licence": LISANS,
        "aciklama": (
            "MPYY EK-2 tablosu: nüfus gruplarına göre asgari sosyal ve teknik "
            "altyapı alanı standartları. TAKS/KAKS komşusu mevzuat değerleridir ve "
            "C++'a giremez (CLAUDE.md 5.13). m²/kişi değerleri binde tam sayı olarak "
            "saklanır (0.5 -> 500); kayan nokta saklanmaz."
        ),
        "kapsam": {
            "durum": "tablo-cikarildi-uzman-onayi-bekliyor",
            "iceren": [
                "%d altyapı kalemi, %d nüfus grubu." % (len(kalemler), len(gruplar)),
                "Kaynak metin her değerin yanında ('m2_kisi_metin', "
                "'asgari_alan_metin') saklanır; çevrilmiş değer denetlenebilir.",
                "Tablonun 13 maddelik açıklama bloğu birebir.",
            ],
            "eksikler": [
                "'Ünit başına (110) m²', 'Yatak başına (130) m²' gibi birim bağımlı "
                "hücreler sayıya çevrilmemiştir; ham metin olarak durur ve satır "
                "'belirsiz' işaretlidir.",
                "Kaynak tabloda '150.001 - 500.000' ile '501.000 +' grupları arasında "
                "500.001-500.999 aralığı tanımsızdır. Bu kaynağın kendi yazımıdır, "
                "düzeltilmemiştir.",
                "'belirsiz' işaretli %d kalem vardır." % len(belirsiz_kalemler),
            ],
            "onay": ONAY,
            "sonraki_surum": "0.3.0 — birim bağımlı hücrelerin uzman onaylı çevirisi.",
        },
        "nufus_gruplari": gruplar,
        "kalemler": kalemler,
        "aciklamalar": aciklamalar,
    }

    # ---- diske yaz --------------------------------------------------------
    json_yaz(os.path.join(hedef, "plan-gosterim.json"), gosterim)
    json_yaz(os.path.join(hedef, "detay-katalogu.json"), detay_katalogu)
    json_yaz(os.path.join(hedef, "asgari-standartlar.json"), standartlar)

    if args.gorsel_yok:
        gunluk.append("semboller: yazılmadı (--gorsel-yok)")
    else:
        gerekli = kullanilan_gorseller | detay_gorseller
        havuz.kayitlar = {k: v for k, v in havuz.kayitlar.items() if k in gerekli}
        yazilan = havuz.yaz(os.path.join(hedef, "semboller"))
        toplam = sum(v["bayt"] for v in havuz.kayitlar.values())
        gunluk.append("semboller: %d ayrık görsel (%d KB), %d dosya yazıldı"
                      % (len(havuz.kayitlar), toplam // 1024, yazilan))

    tamam = True
    tamam &= sema_dogrula(gosterim, os.path.join(sema_dizini, "plan-gosterim.schema.json"), gunluk)
    tamam &= sema_dogrula(detay_katalogu,
                          os.path.join(sema_dizini, "detay-katalogu.schema.json"), gunluk)
    tamam &= sema_dogrula(standartlar,
                          os.path.join(sema_dizini, "asgari-standartlar.schema.json"), gunluk)

    for satir in gunluk:
        print(satir)
    print("belirsiz: %d gösterim satırı, %d detay kartı, %d standart kalemi"
          % (len(belirsiz_stiller), len(belirsiz_detaylar), len(belirsiz_kalemler)))
    return 0 if tamam else 1


if __name__ == "__main__":
    sys.exit(main())
