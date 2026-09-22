# Bir ada boyunca parsel cepheleri — döngüyle, tek Ctrl+Z.
#
# Bu dosya olduğu gibi çalışır:
#     make run-script SCRIPT=tests/journal/ornek-ada.py
#
# Aynı çizimi elle yapmak seksen küsur köşe yazmak demektir; JSON betiğinde de
# öyledir, çünkü orada döngü yoktur. Ayrıntı: docs/betik/python.md

x0, y0 = 485300.000, 4310200.000
cephe, derinlik, adet = 20.0, 30.0, 8

# İki yazma yolu, aynı komut veri yolu. `cad.run` komut satırının kendisidir —
# metre cinsinden, tek gramer. `cad.<komut>` ise komut kaydından ÜRETİLMİŞ
# çağrılabilir: anahtar kelimeleri İngilizce, koordinatları milimetre.
cad.run("KATMAN PARSEL")

for i in range(adet):
    sol = x0 + i * cephe
    sag = sol + cephe
    alt, ust = y0, y0 + derinlik
    cad.run(f"ÇİZGİ {sol:.3f},{alt:.3f} {sag:.3f},{alt:.3f} "
            f"{sag:.3f},{ust:.3f} {sol:.3f},{ust:.3f} {sol:.3f},{alt:.3f}")

# Adanın kuzeyinden geçen yol: iki kenar çizgisi, bu kez üretilmiş çağrıyla.
cad.run("KATMAN YOL")
yol = int((y0 + derinlik + 8.0) * 1000)
bati, dogu = int((x0 - 10.0) * 1000), int((x0 + adet * cephe + 10.0) * 1000)
cad.line(points=[[bati, yol], [dogu, yol]])
cad.line(points=[[bati, yol + 10000], [dogu, yol + 10000]])

# Ada ortasına bir röper dairesi, yine üretilmiş çağrıyla.
cad.run("KATMAN RÖPER")
orta = [int((x0 + adet * cephe / 2) * 1000), int((y0 + derinlik / 2) * 1000)]
cad.circle_draw(center=orta, rim=[orta[0] + 3000, orta[1]])

print(f"Python: {adet} parsel, {cad.doc.entity_count()} nesne, "
      f"{cad.doc.layer_count()} katman")
print(f"Kum havuzu: {cad.sandbox()} · KRS: {cad.doc.crs()}")
print(f"Üretilmiş çağrılabilir sayısı: {len(cad.__all__)}")
