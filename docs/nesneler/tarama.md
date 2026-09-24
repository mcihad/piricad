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

## Bağlar

Seçilen nesnelerden çizilen tarama o nesnelere **bağlıdır**
([Bağlı tarama](../komutlar/hatch.md#bağlı-tarama)). Bağlar halkalarda değil ayrı bir
**tarama bağ tablosunda** durur: her taramanın sınır nesneleri, kalıcı anahtarlarıyla,
ve her birinin kopuk olup olmadığı. Hangi halkanın delik olduğu saklanmaz; her
yeniden kuruluşta sınırların iç içeliğinden bulunur. Bağlı taraması olmayan bir çizim
bu tablo için hiçbir şey ödemez: parmak izi de dosyası da aynıdır.

## Nasıl çizilir

Tarama, stiliyle çizilir: komut nesneyi oluştururken katmanın rengiyle bir sembol
kurar — sınır çizgisi, dolu tarama için bir dolgu katmanı, desenli tarama için aile
başına bir **çizgi deseni dolgusu** (açı, aralık ve **faz**: çizgilerin dünya başlangıcına
göre kafesteki yeri, taramanın başlangıç noktasından hesaplanır). Kare yolu her nesnede
olduğu gibi tek bir stil numarası okur; desen çerçeve başında hesaplanmaz. Çizgiler
**yere bağlıdır**: ekranın değil zeminin kafesindedir, görünüm kaydırılınca yerinde
kalır. Ekranda iki buçuk pikselden sık düşen desen çizgi çizgi değil, ortalama tonuyla
dolu çizilir.

## Yakalama noktaları

Sınır halkalarının köşe, orta nokta, en yakın ve kesişim noktaları; alanla aynı.
Desen çizgilerine yakalanılmaz.

## Ölçüler

**Alan** ve **çevre** sınır halkalarından, alanla aynı şekilde: dış alan eksi adalar.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `8` (`core.hatch`). GeoPackage'a çokgen olarak yazılır.

**DXF'te desen çizdiği çizgilerle gider.** Yazarken sınır kenar döngüsü olarak, desen
adı, açısı, ölçeği ve **desen tanım çizgileriyle** (grup 78 ve ardından: açı, geçtiği
nokta, aralık, kesik dizisi — çizimin biriminde, döndürülmüş ve ölçeklenmiş) `HATCH`
olur. Ölçek (grup 41) çizimin birimine göre yazılır: katalogdaki sayılar metrik desen
dosyasının (acadiso.pat) sayılarıdır, çizim birimi olarak okunup ölçekle çarpılınca
zemindeki aralığı verir. Kendi deseninizde grup 41 aralığın kendisidir (DXF'in
kullanıcı deseni kuralı). Böylece başka bir program deseni kendi desen dosyasına
bakmadan, bu programın ekranda ve kâğıtta çizdiği aralıkla ve aynı başlangıçtan çizer.

Okurken önce ad, açı ve birimine göre ölçek alınır, aileler katalogdan adla bulunur;
dosya desenin kendi çizgilerini taşıyorsa bunlar geçer: aralık, açı ve başlangıç
dosyadaki gibidir ve **katalogda olmayan bir desen de çizilir** (transkriptte "N
taramanın deseni dosyadaki kendi çizgileriyle okundu" denir). Çizgileri olmayan ya
da grup 78'in söylediğini tutmayan bir kayıtta katalog geçer; katalogda da yoksa sınır
ve ad korunur, desen çizilmez (`düşürme:` satırı söyler). İkili (binary) DXF'te desen
çizgileri okunmaz, katalog geçer.

Aralık zeminde milimetreye yuvarlanır, modelin her uzunluğu gibi: 1/500'de bir çizgi
aralığı kâğıtta 1/500 milimetreden daha az sapar.

Bağlar proje dosyasında kendi bloğunda (`0x008F`, bağ başına 24 bayt: taramanın ve
sınır nesnesinin anahtarı, kopukluk) durur; blok yalnız bağlı bir tarama varsa yazılır.
Dosyada olmayan bir nesneye işaret eden bağ okunurken kopuk sayılır. DXF'in "ilişkili"
işareti (grup 71) gidiş-dönüşte korunur ama bir bağ değildir.

## Komutlar

```
TARAMA noktalar=0,0 20,0 20,20 0,20 desen=ANSI31 olcek=1000
```

[TARAMA](../komutlar/hatch.md).

## Sınırlar

Ailelerin **kesik dizisi** bu sürümde çizilmez (korunur ve DXF'e adla gider);
kesikli desenler düz çizgilerle görünür. Gradyan tarama dolu tarama olarak okunur.
Bir yay ya da spline kenarlı DXF sınırı çizgi parçalarına bölünerek gelir.
