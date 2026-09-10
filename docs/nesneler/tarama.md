# Tarama

## Nedir

Kapalı bir sınırın içini dolu ya da çizgi deseniyle dolduran nesne: imar lekesi,
malzeme taraması, orman alanı. DXF `HATCH` budur.

## Nasıl saklanır

Halkalar **sınır döngüleridir**, alanla aynı kuralla: dış halka önce, adalar sonra.
Desen **yükte** durur: adı (`SOLID`, `ANSI31`, `NET`…), açısı, ölçeği (kesin bir oran),
başlangıç noktası ve deseni çizen **çizgi aileleri** — her aile bir açı, taban noktası,
bir çizgiden ötekine kayma ve kesik dizisi. Aileler adla
[desen kataloğundan](../komutlar/hatch.md#desen-kataloğu) alınır ve nesnede saklanır;
katalog değişse çizim değişmez.

## Nasıl çizilir

Tarama, stiliyle çizilir: komut nesneyi oluştururken katmanın rengiyle bir sembol
kurar — sınır çizgisi, dolu tarama için bir dolgu katmanı, desenli tarama için aile
başına bir **çizgi deseni dolgusu** (açı ve aralık). Kare yolu her nesnede olduğu
gibi tek bir stil numarası okur; desen çerçeve başında hesaplanmaz.

## Yakalama noktaları

Sınır halkalarının köşe, orta nokta, en yakın ve kesişim noktaları; alanla aynı.
Desen çizgilerine yakalanılmaz.

## Ölçüler

**Alan** ve **çevre** sınır halkalarından, alanla aynı şekilde: dış alan eksi adalar.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `8` (`core.hatch`). DXF `HATCH` sınırı, adı, açısı ve
ölçeğiyle gelir; aileler katalogdan adla bulunur, katalogda olmayan bir desenin sınırı
ve adı korunur ama deseni çizilmez (`düşürme:` satırı söyler). Yazarken sınır kenar
döngüsü olarak, desen adı, açısı ve ölçeğiyle `HATCH` olur; desen tanım çizgileri
yazılmaz, AutoCAD deseni kendi kataloğundan adla bulur. GeoPackage'a çokgen olarak
yazılır.

## Komutlar

```
TARAMA noktalar=0,0 20,0 20,20 0,20 desen=ANSI31 olcek=1000
```

[TARAMA](../komutlar/hatch.md).

## Sınırlar

Ailelerin **kesik dizisi** bu sürümde çizilmez (korunur ve DXF'e adla gider);
kesikli desenler düz çizgilerle görünür. Gradyan tarama dolu tarama olarak okunur.
Bir yay ya da spline kenarlı DXF sınırı çizgi parçalarına bölünerek gelir.
