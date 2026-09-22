# Bir ada boyunca parsel cepheleri — döngüyle, tek Ctrl+Z.
#
# Bu dosya olduğu gibi çalışır:
#     make run-script SCRIPT=tests/journal/ornek-ada.py
#
# Aynı çizimi elle yapmak seksen küsur köşe yazmak demektir; JSON betiğinde de
# öyledir, çünkü orada döngü yoktur. Ayrıntı: docs/betik/python.md

x0, y0 = 485300.000, 4310200.000
cephe, derinlik, adet = 20.0, 30.0, 8

cad.run("KATMAN PARSEL")

for i in range(adet):
    sol = x0 + i * cephe
    sag = sol + cephe
    alt, ust = y0, y0 + derinlik
    cad.run(f"ÇİZGİ {sol:.3f},{alt:.3f} {sag:.3f},{alt:.3f} "
            f"{sag:.3f},{ust:.3f} {sol:.3f},{ust:.3f} {sol:.3f},{alt:.3f}")

# Adanın kuzeyinden geçen yol: iki kenar çizgisi.
cad.run("KATMAN YOL")
yol = y0 + derinlik + 8.0
bati, dogu = x0 - 10.0, x0 + adet * cephe + 10.0
cad.run(f"ÇİZGİ {bati:.3f},{yol:.3f} {dogu:.3f},{yol:.3f}")
cad.run(f"ÇİZGİ {bati:.3f},{yol + 10:.3f} {dogu:.3f},{yol + 10:.3f}")

print(f"Python: {adet} parsel, {cad.entity_count()} nesne, {cad.layer_count()} katman")
print(f"Kum havuzu: {cad.sandbox()} · KRS: {cad.crs()}")
