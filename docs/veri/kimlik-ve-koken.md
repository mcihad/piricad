# Nesne kimliği ve kökeni

Bir parsel haritada, öznitelik tablosunda, seçimde, onu adlandıran etikette ve onu ölçen
ölçüde **aynı nesnedir**. Bu sayfa o nesnenin kimliğinin neyle değişmediğini, neyle yeni
bir nesnenin doğduğunu ve yeni doğan bir nesnenin nereden geldiğini nasıl bildiğini
anlatır.

## Kalıcı kimlik

Her nesnenin bir **kimliği** vardır: öznitelik tablosunun `fid` sütunu, öznitelik
panelinin `kimlik` satırı, komutlardaki `nesneler=12` ve `nesne=12`. Kimlik bir kez
verilir ve **hiçbir zaman başka bir nesneye verilmez** — silinmiş bir parselin kimliği
de boşta kalır. "Bu parsel hangisiydi?" hukuki bir sorudur ve cevabı değişmemelidir.

Kimliği **değiştirmeyen**, yalnız biçimi değiştiren işler:

| İş | Ne olur |
|---|---|
| Tutamaktan köşe çekmek, [KÖŞETAŞI](../komutlar/vertex_move.md), [ESNET](../komutlar/stretch.md) | Aynı nesnenin köşesi taşınır |
| [TAŞI](../komutlar/move.md), [DÖNDÜR](../komutlar/rotate.md), [ÖLÇEKLE](../komutlar/scale.md), [AYNALA](../komutlar/mirror.md) | Aynı nesne yer değiştirir |
| Değer yazmak, katmanını ya da stilini değiştirmek | Aynı nesnenin özelliği değişir |
| Geri alma ve yineleme | Aynı nesne eski hâline döner |
| Kaydedip açmak | Aynı kimlikle geri gelir |

Bu işlerden sonra nesnenin **tablo satırı, değerleri, seçimdeki yeri, ona bağlı etiket,
ölçü ve tarama** onunla kalır; kaydedip açınca da.

## Yeni nesne doğuran işler

Bazı işler var olan nesneden **yeni** bir nesne yapar; yeni nesnenin yeni bir kimliği
olur. Bir parçanın değerleri genellikle kaynağından kopyalanır ("bir şeyin parçası yine
o şeydir"), ama kimliği kopyalanmaz:

- [İFRAZ](../komutlar/split_parcel.md) ve [ALANİFRAZ](../komutlar/split_area.md): parsel iki
  yeni parsel olur, eskisi çizimden kalkar.
- [TEVHİT](../komutlar/merge.md) ve [BİRLEŞTİR](../komutlar/combine.md): birleşenlerin
  yerine yeni bir alan.
- [BUDA](../komutlar/trim.md), [BÖL](../komutlar/split.md), [KIR](../komutlar/break.md),
  [UZAT](../komutlar/extend.md): nesnenin kendi türünde kalabilen ilk parçası nesnenin
  kendisidir, öbür parçalar yenidir.
- [UÇUCA](../komutlar/join.md), [YUVARLA ve PAH](../komutlar/fillet.md) (iki nesne
  arasındaki köşe parçası), [PATLAT](../komutlar/explode.md), [OFSET](../komutlar/offset.md),
  [SINIR](../komutlar/boundary.md), [ALANAÇEVİR](../komutlar/to_area.md).
- [KOPYALA](../komutlar/copy.md), [DİZİ](../komutlar/array.md) ve `kopya=evet`.
- Analiz araçlarının çıktıları: [TAMPON](../komutlar/tampon.md), alan oluşturma ([işlem araçları](../islem/README.md)),
  [UZUNLUKYAZ](../komutlar/uzunluk_yaz.md), [KÖŞENUMARALA](../komutlar/kose_numarala.md).

## Köken

Yeni doğan her nesne **kökenini** bilir: onu hangi işin ürettiği ve hangi nesnelerden
üretildiği. Köken çizimle birlikte saklanır, kaydedip açınca geri gelir, onu üreten
işle birlikte geri alınır.

| Nerede görünür | Nasıl |
|---|---|
| Öznitelik paneli | `koken` satırı, **GEÇMİŞ** rozetiyle: `İFRAZ ← 1 (silinmiş)` |
| [NESNEBİLGİ](../komutlar/entity_info.md) | `kökeni: TEVHİT (kaynak: nesne 2, 3)`; yapılandırılmış cevapta `koken` (`islem`, `ad`, `kaynaklar`) |
| Kaynak nesnede | NESNEBİLGİ `bundan türetilen: nesne 14, 15` der (`turetilen`) |

**Silinmiş kaynak da adıyla kalır.** İFRAZ ebeveyn parseli çizimden kaldırır; kimlik
hiç yeniden verilmediği için parçaların kökeni yine tam o parseli gösterir ve "artık
çizimde değil" diye söyler. İki ifraz ve bir tevhit sonra bugünkü parselden ilk
parsele kadar zincir böyle izlenir.

Bir analiz aracı çıktısının kaynağını ayrı ayrı biliyorsa onu yazar: ayrı ayrı çizilen
tamponların her biri kendi kuyusunu, `ALANOLUŞTUR`'un her alanı onu çizen çizgileri,
`UZUNLUKYAZ`'ın her yazısı ölçtüğü nesneyi bilir. Birleşik bir tampon, çizildiği bütün
nesneleri kaynak sayar.

**Köken bir bağ değildir.** Bir parseli izleyen etiket, ölçen ölçü ve dolduran tarama
parsel değişince güncellenir ([bağlı nesneler](../islem/bagli-nesneler.md)). Köken ise
geçmiştir: tampon, kuyuların o anki yerinden çizildi ve kuyular taşınınca kendiliğinden
taşınmaz.

## Kilitli katman ve dış referans

Kilitli bir katmandaki nesnenin yalnız geometrisi değil **değeri, katmanı, stili ve
yazısı** da kilitlidir: ÖZNİTELİK, tablodan yazma, KATMANAT, STİLKOPYALA, STİL ve
YAZIDÜZENLE onu adıyla reddeder. Bir [dış referansın](../komutlar/xref.md) parçası bu
çizimin değil kendi dosyasınındır; buradan bir değer yazılamaz ve ileti referansın
adını, dosyasını ve `DIŞREFERANS islem=bagla` yolunu söyler.

## Dosyada

Kimlikler, silinmiş nesnelerin satırlarıyla birlikte [proje dosyasına](proje-dosyasi.md)
yazılır. Kökenler kendi bloğundadır; kökeni olmayan bir çizim bu bloğu hiç yazmaz ve
eski bir sürüm bloğu tanımasa da dosyayı açar — yalnız kökenleri görmez.

## Bu sürümde henüz olmayanlar

- İçe aktarılan bir dosyanın (DXF, GeoPackage) kaynağı — dosya yolu, katman, dosyadaki
  kimliği — nesnenin kökeni olarak kaydedilmez; DXF'in kendi kimliği `kaynak_kimlik`
  sütununa yazılır. Veri kaynağı kaydı F-02'nin sonraki aşamasında gelecek.
- Salt okunur bir kaynaktaki nesneyi düzenleme isteği bugün adıyla reddedilir; aynı
  istekte **yerel kopya** oluşturma önerisi sonraki aşamada gelecek.
