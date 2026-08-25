#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""MPYY vektör paketini denetler ve iş listesinin doğru söylediğini kanıtlar.

Çalıştırma (depo kökünden):

    python3 scripts/ci-gate-mpyy-vektor.py

Çıkış kodu 0 ise paket tutarlıdır. Değilse her kusur kendi kimliğiyle yazılır.
"""
import json
import os
import re
import sys

KOK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "catalogs", "mpyy-vektor")
DEPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

# --- şemadan gelen sözlük. Kaynağı: src/core/src/style.cpp ---------------------
TIPLER = {
    "cizgi", "isaretci-cizgi", "tarak-cizgi", "dolgu", "cizgi-desen-dolgu",
    "nokta-desen-dolgu", "merkez-isaretci", "isaretci", "gorsel-dolgu",
    "gorsel-isaretci", "gorsel-cizgi", "yazi-isaretci",
}
SEKILLER = {
    "daire", "kare", "ucgen", "baklava", "yildiz", "arti", "carpi", "ok",
    "yarim-daire", "besgen", "altigen", "cizik",
}
YERLESIMLER = {"aralik", "tepe", "ilk", "son", "orta"}
BIRIMLER = {"kagit", "zemin", "piksel"}

# Bir katmanın hangi alanları taşıyabileceği. Buradan başka alan yazmak, çizicinin
# okumadığı bir şey yazmak demektir; sessizce yok sayılır ve gösterim eksik çizilir.
ALANLAR = {
    "tip", "sekil", "yerlesim", "birim", "boyut", "aralik", "aralik_y",
    "kaydirma", "faz", "aci", "renk", "dolgu_renk", "kalinlik", "desen",
    "yazi", "renk_kilidi", "gorsel",
}

RENK = re.compile(r"^#[0-9A-Fa-f]{8}$")


def yukle(yol):
    with open(yol, encoding="utf-8") as f:
        return json.load(f)


def main():
    raster = yukle(os.path.join(DEPO, "data/catalogs/mpyy/plan-gosterim.json"))
    vektor = yukle(os.path.join(KOK, "plan-gosterim.json"))

    resmi = {s["id"]: s for s in raster["stiller"]}
    yazilan = {s["id"]: s for s in vektor["stiller"]}
    kusur = []

    # --- 1. paket kendi içinde tutarlı mı --------------------------------------
    gorunen = set()
    for s in vektor["stiller"]:
        sid = s.get("id", "(kimliksiz)")
        if sid in gorunen:
            kusur.append(f"{sid}: aynı kimlik iki kez yazılmış")
        gorunen.add(sid)

        if sid not in resmi:
            kusur.append(f"{sid}: resmî katalogda böyle bir gösterim yok")

        # BİR SATIR İKİ YOLDAN VEKTÖRLEŞİR ve ikisi de geçerlidir:
        #
        #   `katmanlar` — sayılarla söylenmiş bir sembol katmanı yığını. TERCİH
        #                 EDİLEN yol: köşe döner, yeniden renklendirilir, çizgi
        #                 tipi olarak dışa aktarılır.
        #   `gorsel`    — bu paketteki bir SVG. Katman tiplerinin ifade edemediği
        #                 şekiller için; bir kuş, bir mercan, bir cami silueti.
        #
        # Rasterin (jpeg/png) burada yeri yoktur: bu paketin varlık sebebi
        # rasterden kurtulmaktır.
        katmanlar = s.get("katmanlar")
        svg_roller = s.get("gorsel") or {}
        svg_var = any(isinstance(ids, list) and ids for ids in svg_roller.values())
        if not katmanlar and not svg_var:
            kusur.append(f"{sid}: ne `katmanlar` ne `gorsel` var — bu satır vektörleştirilmemiş")
            continue
        if not katmanlar:
            continue

        for i, k in enumerate(katmanlar):
            yer = f"{sid}[{i}]"
            fazla = set(k) - ALANLAR
            if fazla:
                kusur.append(f"{yer}: tanınmayan alan {sorted(fazla)} — çizici bunu okumaz")
            if k.get("tip") not in TIPLER:
                kusur.append(f"{yer}: `tip` geçersiz: {k.get('tip')!r}")
            if "sekil" in k and k["sekil"] not in SEKILLER:
                kusur.append(f"{yer}: `sekil` geçersiz: {k['sekil']!r}")
            if "yerlesim" in k and k["yerlesim"] not in YERLESIMLER:
                kusur.append(f"{yer}: `yerlesim` geçersiz: {k['yerlesim']!r}")
            if "birim" in k and k["birim"] not in BIRIMLER:
                kusur.append(f"{yer}: `birim` geçersiz: {k['birim']!r}")
            for alan in ("renk", "dolgu_renk"):
                if alan in k and not RENK.match(str(k[alan])):
                    kusur.append(f"{yer}: `{alan}` #AARRGGBB olmalı: {k[alan]!r}")
            for alan in ("boyut", "aralik", "aralik_y", "kaydirma", "faz", "kalinlik", "aci"):
                if alan in k and not isinstance(k[alan], int):
                    kusur.append(f"{yer}: `{alan}` tam sayı olmalı: {k[alan]!r}")
            if k.get("tip") in {"isaretci-cizgi", "tarak-cizgi", "isaretci",
                                "merkez-isaretci"} and "sekil" not in k:
                kusur.append(f"{yer}: bu tip bir `sekil` ister")
            if "desen" in k:
                d = k["desen"]
                if (not isinstance(d, list) or len(d) < 2 or len(d) % 2
                        or any(not isinstance(x, (int, float)) or x <= 0 for x in d)):
                    kusur.append(f"{yer}: `desen` çift sayıda pozitif sayı olmalı: {d!r}")

        # Her satır yorumdur, ölçüm değil: yönetmelik bir resim basar, sayı vermez.
        if not s.get("belirsiz"):
            kusur.append(f"{sid}: `belirsiz: true` yok — bu satır bir okuma, bir alıntı değil")
        if "cizim-yorumu" not in (s.get("belirsiz_nedeni") or []):
            kusur.append(f"{sid}: `belirsiz_nedeni` içinde `cizim-yorumu` yok")
        if not s.get("kaynak"):
            kusur.append(f"{sid}: `kaynak` yok — hangi ek ve hangi bölüm olduğu yazılmalı")

    # --- 2. gorsel alanı gerçekten var olan bir dosyayı gösteriyor mu -----------
    bilinen = {g["id"] for g in vektor.get("gorseller", [])}
    for g in vektor.get("gorseller", []):
        yol = os.path.join(KOK, g.get("dosya", ""))
        if not os.path.isfile(yol):
            kusur.append(f"{g.get('id')}: dosya yok: {g.get('dosya')}")
        elif not str(g.get("dosya", "")).lower().endswith(".svg"):
            kusur.append(f"{g.get('id')}: bu pakette yalnız SVG olur: {g.get('dosya')}")
    for s in vektor["stiller"]:
        for k in s.get("katmanlar") or []:
            if "gorsel" in k and k["gorsel"] not in bilinen:
                kusur.append(f"{s['id']}: `gorsel` tanınmıyor: {k['gorsel']!r}")
        # `gorsel` iki biçimde gelir: rol -> kimlik listesi (satır düzeyinde) ya
        # da katman içinde tek kimlik. Burası satır düzeyini denetler.
        # `gorsel` altında iki tür anahtar vardır: bir ROL (tarama, sembol,
        # cizgi_tipi) bir kimlik listesi taşır; bir ÖLÇÜ (…_boyut, …_aralik)
        # mikrometre cinsinden tek bir sayıdır ve resmin ne kadar büyük ve ne
        # sıklıkta basılacağını söyler.
        for rol, ids in (s.get("gorsel") or {}).items():
            if rol.endswith("_boyut") or rol.endswith("_aralik"):
                if not isinstance(ids, int) or ids <= 0:
                    kusur.append(f"{s['id']}: `gorsel.{rol}` pozitif tam sayı olmalı (µm)")
                continue
            if not isinstance(ids, list):
                kusur.append(f"{s['id']}: `gorsel.{rol}` bir liste olmalı")
                continue
            for i in ids:
                if i not in bilinen:
                    kusur.append(f"{s['id']}: `gorsel.{rol}` tanınmıyor: {i!r}")

    # --- 3. iş listesi doğru mu söylüyor ---------------------------------------
    liste = os.path.join(KOK, "YAPILACAKLAR.md")
    isaretli, isaretsiz = set(), set()
    if os.path.isfile(liste):
        with open(liste, encoding="utf-8") as f:
            for satir in f:
                m = re.match(r"- \[([ xX])\] `([^`]+)`", satir)
                if m:
                    (isaretli if m.group(1).lower() == "x" else isaretsiz).add(m.group(2))

        # İŞARETLİ AMA YAZILMAMIŞ: iş listesinin bittiğini söylediği, paketin
        # taşımadığı satır. Kaçamağın tam olarak alacağı biçim budur.
        for sid in sorted(isaretli - set(yazilan)):
            kusur.append(f"{sid}: iş listesinde işaretli ama pakette yok")
        for sid in sorted(isaretli & set(yazilan)):
            y = yazilan[sid]
            if not y.get("katmanlar") and not any(
                    isinstance(v, list) and v for v in (y.get("gorsel") or {}).values()):
                kusur.append(f"{sid}: iş listesinde işaretli ama ne `katmanlar` ne `gorsel` var")

        # YAZILMIŞ AMA İŞARETSİZ: zararsız ama liste yalan söylüyor.
        for sid in sorted(set(yazilan) - isaretli):
            y = yazilan[sid]
            if y.get("katmanlar") or any(
                    isinstance(v, list) and v for v in (y.get("gorsel") or {}).values()):
                kusur.append(f"{sid}: pakette var ama iş listesinde işaretsiz")

    # --- 4. atlananlar gerekçeli mi -------------------------------------------
    atlanan = os.path.join(KOK, "ATLANANLAR.md")
    gerekce = set()
    if os.path.isfile(atlanan):
        with open(atlanan, encoding="utf-8") as f:
            for satir in f:
                m = re.match(r"- `([^`]+)`\s*—\s*(\S.*)", satir)
                if m and len(m.group(2).strip()) >= 20:
                    gerekce.add(m.group(1))

    # --- rapor -----------------------------------------------------------------
    def vektorlesmis(row):
        if row.get("katmanlar"):
            return True
        return any(isinstance(v, list) and v for v in (row.get("gorsel") or {}).values())

    bitmis = sum(1 for s in vektor["stiller"] if vektorlesmis(s))
    toplam = len(resmi)
    print(f"resmî katalog : {toplam} gösterim")
    print(f"vektörleşen   : {bitmis}")
    print(f"gerekçeli atlanan: {len(gerekce)}")
    print(f"kalan         : {toplam - bitmis - len(gerekce)}")
    print()

    if kusur:
        for k in kusur:
            print("KUSUR:", k, file=sys.stderr)
        print(f"\n{len(kusur)} kusur.", file=sys.stderr)
        return 1

    print("Paket tutarlı: her yazılan satır şemaya uyuyor ve iş listesi doğru söylüyor.")
    if bitmis + len(gerekce) < toplam:
        print(f"İŞ BİTMEDİ: {toplam - bitmis - len(gerekce)} gösterim kaldı.")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
