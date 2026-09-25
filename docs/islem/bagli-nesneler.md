# Bağlı Nesneler

Kenar uzunluklarını ve köşe numaralarını yazdırıp sonra çizimi düzenleyen herkes için;
bu sayfayı bitirdiğinizde bir yazının neden çizgisiyle birlikte taşındığını, uzunluğun
neden kendi kendine yenilendiğini, bunu nasıl kapatacağınızı ve elle yerini
değiştirdiğiniz bir yazının başına ne geleceğini bileceksiniz.

## Bağlı nesne nedir

Bir kenarın uzunluğunu söyleyen yazı o kenar **hakkında** bir cümledir; kenar taşındığında
yazı yerinde kalırsa yanlış yerde yanlış sayıyı söyler. Bir köşenin numarası da köşeye
aittir, yazıldığı koordinata değil.

KentOSCad bu ilişkiyi bir **bağ** olarak kaydeder. Bağlı nesne (**bağımlı**) kaynağını
(**kaynak**), kaynağın hangi özelliğine bağlı olduğunu — bir halkanın bir **köşesi**, bir
**kenarı** ya da nesnenin **ortası** — nasıl yerleştiğini (hangi yan, ne kadar açıkta),
sözünün ne olduğunu (kendi yazısı, kenarın **uzunluğu**, ya da nesneden doldurulan bir
**kalıp**: sütunları ve ölçülen alanı, çevresi, uzunluğu) ve elle verdiğiniz **el
payını** bilir.

Bugün bağımlı nesne **yazıdır**: [`UZUNLUKYAZ`](../komutlar/uzunluk_yaz.md)'ın yazdığı
uzunluklar kenarlarına, [`KÖŞENUMARALA`](../komutlar/kose_numarala.md)'nın yazdığı
numaralar köşelerine, [`ETİKET`](../komutlar/label.md)'in yazdığı etiketler nesnenin
ortasına bağlı doğar. Serbest bir yazıyı [`BAĞLA`](../komutlar/bagla.md) bağlar,
[`BAĞÇÖZ`](../komutlar/bag_coz.md) çözer.

## Kaynak değişince ne olur

Kaynağı değiştiren **komut** — [`TAŞI`](../komutlar/move.md),
[`DÖNDÜR`](../komutlar/rotate.md), [`ÖLÇEKLE`](../komutlar/scale.md),
[`KÖŞETAŞI`](../komutlar/vertex_move.md), [`ALANDÜZENLE`](../komutlar/alan_duzenle.md),
bir tutamağı sürüklemek — bittiğinde bağlı yazılar aynı işlem içinde yenilenir:

| Değişiklik | Bağlı yazıya olan |
|---|---|
| Kaynak taşındı, döndü, ölçeklendi | Yazı aynı kenarın/köşenin yanına, aynı kurala göre yeniden yerleşir; kenar yazısı kenara paralel kalır |
| Kenar uzadı ya da kısaldı | `uzunluk` türünde yazının sayısı yeniden yazılır: `10,00 m` → `20,00 m` |
| Nesnenin biçimi değişti | Kalıplı yazının `{#alan}`, `{#cevre}`, `{#uzunluk}`'u yeniden ölçülür: `200,00 m²` → `300,00 m²`; ortadaki yazı yeni ortaya geçer |
| Bir sütunu değişti ([`ÖZNİTELİK`](../komutlar/attribute.md)) | Kalıplı yazının `{sutun}`'u yeni değerle yazılır; boşalan sütunun yazısı boş kalır |
| Kenara köşe eklendi, köşe silindi | Yazı en yakın kenara/köşeye yeniden bağlanır; komşusuna sıçramaz |
| Kaynak silindi | Bağlı yazılar da silinir; durum satırı "Silinen nesnelere bağlı N nesne de silindi" der |
| Bağlı yazı silindi | Yalnız yazı gider; kaynak etkilenmez |
| Yazının katmanı kilitli | Yazı yerinde kalır ve bu söylenir; tuvalde **kilitli: kaynağının gerisinde** işareti durur, öznitelik panelinde `bag` satırı **GÜNCEL DEĞİL** der. Bağı kenarı izlemeyi sürdürür — kenara köşe eklense de. Katmanın kilidi açıldığı anda yazı kaynağına **yetişir**: yeniden yerleşir, sayısı yeniden yazılır |

Bunların hepsi komutun **kendi geri alma adımı** içindedir: bir [`GERİAL`](../komutlar/undo.md)
hem çizgiyi hem yazısını geri getirir. Komut günlüğünde yalnız sizin verdiğiniz komut
yazılıdır; izleme o komutun yaptığı iştir, yeniden oynatma aynı sonucu verir.

## Yazıyı elle taşırsanız

Bağlı bir yazıyı kendiniz daha iyi okunacağı yere çekerseniz program bunu **el payı**
olarak saklar: kural yeri ile sizin yeriniz arasındaki fark, kenarın okuma doğrultusuna
göre ölçülür. Kaynak sonra taşınsa ya da dönse yazı kuralın yerine değil, **sizin
verdiğiniz kadar açığa** gider. Yazıyı ve kaynağını birlikte taşımak ilişkiyi değiştirmez.

Bağlı bir yazıyı tek başına **döndürmek** kalıcı değildir: kaynak bir sonraki kez
değişince yazı yine kenarına paralel yerleşir.

## Bağı kapatmak

- Yazdırırken: `UZUNLUKYAZ … bagla=hayır`, `KÖŞENUMARALA … bagla=hayır` serbest yazı
  üretir. Araçlar panelinde **bagla** anahtarını kapatın.
- Sonradan: yazıları seçip [`BAĞÇÖZ`](../komutlar/bag_coz.md). Yazı yerinde kalır.

Çizimdeki bütün bağlı yazıların, ölçülerin ve taramaların kaynaklarına göre güncel olup
olmadığını [`BAĞIMLILIK`](../komutlar/dependency.md) söyler; geride kalanı
`BAĞIMLILIK islem=yenile` kaynağına yetiştirir ([Bağımlılıklar ve
sonuçlar](../veri/bagimliliklar.md)).

## Dosyada

Bağlar `.pcad` dosyasında kendi bloğunda, iki ucu da kalıcı nesne anahtarıyla saklanır;
bağı olmayan bir çizimin dosyası bir bayt bile değişmez. Ayrıntı için
[proje dosyası](../veri/proje-dosyasi.md).

## Klavye

Bağ kurmak ve çözmek birer komuttur: komut satırından `BAĞLA`, `BAĞÇÖZ`; Araçlar
panelindeki kartta Tab ile alanlarda gezin, **kaynak** alanında **F4** sahneden seçmeyi
başlatır, **Esc** vazgeçer, kimliği doğrudan yazmak da olur.

## İlgili

- [İşlem araçları](README.md)
- [`BAĞLA`](../komutlar/bagla.md) · [`BAĞÇÖZ`](../komutlar/bag_coz.md)
- [`UZUNLUKYAZ`](../komutlar/uzunluk_yaz.md) · [`KÖŞENUMARALA`](../komutlar/kose_numarala.md)
- [Bileşenler](../baslangic/bilesenler.md) — sahneden seçme girdisi
