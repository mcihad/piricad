# MPYY Plan Gösterimleri Veri Paketi

Şehir plancısı ve harita mühendisi için; bu sayfayı bitirdiğinizde PiriCAD'in Mekânsal
Planlar Yapım Yönetmeliği gösterimlerini nereden okuduğunu, paketin hangi Resmî Gazete
sürümüne dayandığını, neyin çıkarıldığını, neyin **bilerek eksik** bırakıldığını ve
paketi kaynağından nasıl yeniden üreteceğinizi bileceksiniz.

> **Bu paket henüz uzman onayından geçmedi.** Üç katalogun da `kapsam.onay` alanında
> `BEKLİYOR` yazar. Harita mühendisi veya şehir plancısı imzası alınmadan bu paket
> resmî bir plan paftasında kullanılmaz. Onaydan sonra bu uyarı kaldırılacak ve
> `package_version` artırılacaktır.

## Paket nerede durur

| Dosya | İçerik |
|---|---|
| `data/catalogs/mpyy/plan-gosterim.json` | EK-1a, EK-1b, EK-1c, EK-1ç, EK-1d gösterim satırları |
| `data/catalogs/mpyy/detay-katalogu.json` | EK-1e Detay Kataloğu kartları |
| `data/catalogs/mpyy/asgari-standartlar.json` | EK-2 asgari sosyal ve teknik altyapı standartları |
| `data/catalogs/mpyy/semboller/` | Eklerin gömülü çizgi tipi, sembol ve tarama görselleri |

Şemalar `data/catalogs/schema/` altındadır: `plan-gosterim.schema.json`,
`detay-katalogu.schema.json`, `asgari-standartlar.schema.json`.

Bu değerlerin **hiçbiri programın içine gömülü değildir**. Yönetmelik değiştiğinde
PiriCAD yeniden derlenmez; yalnız bu dosyalar değişir.

## Hangi sürüm yürürlükte

Her ek kendi Resmî Gazete damgasını taşır ve paket bu damgayı olduğu gibi kaydeder.

| Ek | Kapsam | Dayandığı Resmî Gazete | Yayım |
|---|---|---|---|
| EK-1a | Ortak Gösterimler | RG-22/1/2026-33145 | 22.01.2026 |
| EK-1b | Mekânsal Strateji Planı Gösterimleri | değişiklik damgası **yok** — RG-14/6/2014-29030 | 14.06.2014 |
| EK-1c | Çevre Düzeni Planı Gösterimleri | RG-22/1/2026-33145 | 22.01.2026 |
| EK-1ç | Nazım İmar Planı Gösterimleri | RG-22/1/2026-33145 | 22.01.2026 |
| EK-1d | Uygulama İmar Planı Gösterimleri | RG-22/1/2026-33145 | 22.01.2026 |
| EK-1e | Mekânsal Planlar Detay Katalogları | RG-22/1/2026-33145 | 22.01.2026 |
| EK-2 | Asgari altyapı standartları tablosu | RG-17/5/2017-30069 | 17.05.2017 |

**EK-1b neden ayrı:** elinizdeki EK-1b dosyasının metninde `(Değişik:RG-…)` damgası
geçmiyor, oysa diğer beş ekte geçiyor. Dosyanın kendi belge özelliklerinde son
değiştirilme tarihi 10.06.2014'tür; yönetmeliğin yayımından dört gün öncesi. Bu iki
bulgu birlikte, EK-1b'nin yönetmeliğin ilk hâli olduğunu ve sonradan
değiştirilmediğini gösterir. Tespit uydurulmamış, kaydedilmiştir: paketin `source`
alanında aynen yazar. Mekânsal strateji planı çizerken EK-1b'nin güncel hâlini
Resmî Gazete'den teyit etmeniz beklenir.

## Ne çıkarıldı

| Ek | Gösterim satırı |
|---|---|
| EK-1a Ortak Gösterimler | 89 |
| EK-1b Mekânsal Strateji Planı | 30 |
| EK-1c Çevre Düzeni Planı | 37 |
| EK-1ç Nazım İmar Planı | 115 |
| EK-1d Uygulama İmar Planı | 205 |
| **Toplam** | **476** |

Bu 476 satırın:

- **297'sinde** alan renk kodu (RGB) okundu ve `dolgu.renk` olarak yazıldı;
- **48'inde** kaynak `ŞEFFAF` diyor — alan boyanmaz, `dolgu.seffaf` işaretli;
- **8'inde** çizgi rengi, **17'sinde** simge rengi çözüldü;
- **5'inde** `%50 Saydam` gibi bir saydamlık notu var, yüzde değeri korundu;
- **439'unda** en az bir gömülü görsel (çizgi tipi, sembol veya tarama) var.

EK-1e'den **379 detay kartı** çıkarıldı; 338'inde renk, 311'inde plan türü başına
çizgi kalınlığı var. EK-2'den **33 altyapı kalemi**, 4 nüfus grubu ve 13 maddelik
açıklama bloğu çıkarıldı.

## Bir gösterim satırı neye benzer

```json
{
  "id": "ortak-organize-sanayi-bolgesi",
  "ad": "ORGANİZE SANAYİ BÖLGESİ",
  "ek": "EK-1a",
  "bolum": ["SINIRLAR", "ÖZEL KANUNLARLA BELİRLENEN ALAN VE SINIRLARI"],
  "dolgu": { "renk": "#AA66CD" },
  "gorsel": {
    "cizgi_tipi": ["gorsel-5eb0cd52a1d0fa90"],
    "sembol": ["gorsel-113ff1bf43f9b15c"],
    "tarama": ["gorsel-1de155a3f7ed45b3"]
  }
}
```

`kaynak` alanı satırın hangi ekten ve hangi bölüm başlığından geldiğini yazar.
`sutunlar` alanı ise kaynak tablonun o satırdaki **bütün hücrelerini olduğu gibi**
taşır: onay veren mühendis çevrilmiş her değeri yönetmelik metnine karşı
denetleyebilsin diye.

Renkler `#RRGGBB` biçiminde onaltılık metindir. Yönetmelik `170/102/205` yazar,
paket `#AA66CD` yazar; aynı sayıdır. Onaltılık tercih edilir çünkü lejantı onaylayan
mühendis `4281236786` gibi bir sayıyı denetleyemez.

## Semboller

Eklerdeki her çizgi tipi, sembol ve tarama çizimi `data/catalogs/mpyy/semboller/`
altına ayrı dosya olarak çıkarıldı. Dosya adı, dosyanın **kendi içeriğinin
SHA-256 özetinin** ilk 16 basamağıdır. Aynı sembol kaç satırda geçerse geçsin tek
dosyadır: 897 gönderme, 608 ayrık dosya, toplam 6,6 MB.

Görseller yönetmeliğin kendi bitmap ve metafile çizimleridir (PNG, JPEG, EMF, TIFF).
Bu paket onları olduğu gibi taşır; çizilebilir hâlleri bir sonraki bölümdeki **vektör
paketindedir**.

## Vektör paketi

`data/catalogs/mpyy-vektor/plan-gosterim.json`, aynı 476 gösterimin **çizilebilir**
hâlidir. Resimli paketin ÜSTÜNE yüklenir ve aynı kimliği yeniden bildiren satır
öncekinin yerine geçer; program açılışta ikisini bu sırayla yükler.

**Neden ayrı bir paket.** Bir JPEG kırpması yeniden renklendirilemez, keskin
ölçeklenemez, DWG'ye çizgi tipi olarak yazılamaz ve köşe dönemez. Yönetmelik
gösterimi resim olarak yayımlar; imzalanan bir plan ise bunların hepsini ister.

| | |
|---|---|
| Vektörleşen satır | **467** / 476 |
| Gerekçeli atlanan | **9** — `ATLANANLAR.md` |
| Sembol katmanı | 1 574 (satır başına ortalama 3,4) |
| Katman tipiyle anlatılan | 1 436 katman |
| SVG çizim | 98 dosya, 107 KB |
| Paket boyutu | 431 KB |

Katman tipi dağılımı: 325 çizgi desen dolgusu, 297 düz dolgu, 197 işaretçi-çizgi,
193 çizgi, 177 işaretçi, 135 görsel işaretçi, 114 nokta desen dolgusu, 107 yazı
işaretçisi, 23 görsel çizgi, 3 tarak-çizgi, 3 görsel dolgu.

### Sayılar nereden geldi

Her satır ekin **bastığı şekle bakılarak** yazıldı; taranmış görüntünün otomatik
izlenmesiyle değil. Ama gözle tahmin de edilmedi: her kırpma önce ölçüldü.

Kırpmaların içinde **gömülü DPI** vardır (çoğunda 220 dpi). Bir piksel böylece
mikrometreye çevrilebilir, ve mürekkep dört yönde izdüşürülüp çizgi kalınlığı,
tekrar adımı, işaretçi aralığı ve kesik/boşluk yapısı **sayı olarak** okunur.
Renkler de ölçülür: JPEG pusunu geçmek için mürekkepli piksellerin en koyu ve en
doygun yüzde onu alınır.

Eğik taramalarda bir tuzak vardır ve bu pakette ona düşülmemiştir: `y+x` ekseninde
bir adım, iki çizgi arasındaki **dik** mesafenin √2 katıdır. Ölçüm bunu böler;
bölmeyen bir okuma bütün eğik taramaları %41 seyrek yazardı.

Buna rağmen her satır `belirsiz: true` ve `cizim-yorumu` gerekçelidir: ölçülen şey
bir **resimdir**, yönetmeliğin verdiği bir sayı değil. Onay veren mühendis için bu
ayrım esastır.

### Kaynağın kendi kusurları

Ekin bastığı şey her zaman bir gösterim değildir. **17 satırda** kaynak kusuru
bulundu ve satıra `kaynak_kusuru` alanı olarak, gerekçesiyle yazıldı:

- ÇİZGİ TİPİ / SEMBOL / TARAMA sütununa gösterim yerine bir **program ekran
  görüntüsü** konmuş satırlar (ArcMap ve siyah zeminli bir çizim penceresi).
  Deseni ekranın harita bölmesinden ölçülebilenler bu paketin ev ölçeğiyle çizildi
  ve ölçeğin okunamadığı satıra yazıldı; okunamayan çizilmedi.
- AYRIK, BİTİŞİK ve BLOK DÜZEN ile KAT ADEDİ satırları ekte **aynı düz siyah
  lekeyle** basılmış; dördü birbirinden ayırt edilemiyor.

Dokuz satır hiç çizilmedi: yönetmelik o satırların bütün gösterim sütunlarını boş
basmıştır. Gerekçeleri satır satır `ATLANANLAR.md` dosyasındadır. Uydurmak,
olmayan bir kuralı gösterim diye yayımlamak olurdu.

### SVG ne zaman kullanılır

Bir daire, bir üçgen, bir çapraz tarama **katmanlarla** yazılır — döner,
renklendirilir, çizgi tipi olarak dışa aktarılır. SVG yalnız katman tiplerinin
ifade edemediği şekiller içindir: bir kaplumbağa, bir çapa, bir hilal, bir çark.
Bir SVG bir katmanın içinde `gorsel` alanıyla çağrılır, yani bir sınır çizgisi
sayılarla yazılıp üzerine çark basılabilir.

Ekin **EMF olarak** bastığı on iki çizim zaten vektördür; bunların altısı
dönüştürülerek pakete alınmıştır. Kalan altısı düz desendi ve katmanlarla yazıldı:
döşenen bir resim kendi dikişini gösterir.

## Bilerek eksik bırakılanlar

Eksik satır, yanlış satırdan sonsuz kez iyidir. Uydurulmuş bir gösterim, imzalanan bir
imar planına yanlış renk yazar. Bu yüzden çıkarım hiçbir değeri tahmin etmez.

**1. Eşleme kuralları yok.** Paketin `kurallar` dizisi boştur. Hangi nesnenin hangi
gösterim satırını alacağı yönetmelik ekinden okunamaz; plan türü ve öznitelik
şemasıyla birlikte uzman kararıdır.

**2. Çizgi desenleri motor indeksine çevrilmedi.** Kesik/noktalı desen tablosu
(`cizgi_desenleri`, `tarama_desenleri`) paketin kendi sembol uzayıdır ve gösterim
satırlarına henüz bağlanmamıştır.

**3. Çizgi kalınlıkları yalnız EK-1e'de var.** EK-1a…EK-1d ekleri kalınlık vermez.
EK-1e'de kalınlık, hücre `ÇİZGİ KALINLIĞI: <sayı> mm` ile **başlıyorsa** kâğıt
mikrometresine çevrildi (1000 = 1 mm). Birden çok kalınlık sayan açıklamalar
(`Cephe Çizgisi 2.5 mm, Kaldırım ve Refüj çizgileri 0.3 mm`) çevrilmedi; ham metin
`plan_notlari` içinde durur.

**4. `belirsiz` işaretli satırlar.** Kaynaktan tek anlamlı bir görünüm çıkmayan
satırlar silinmedi, işaretlendi. Toplam 14 gösterim satırı, 15 detay kartı, 6 standart
kalemi. Gerekçe kodları:

| Kod | Anlamı | Adet |
|---|---|---|
| `gosterim-sutunlari-bos` | Satırın çizgi tipi, sembol, tarama ve renk sütunlarının hepsi kaynakta boş | 9 |
| `alan-renk-coklu` | Alan renk hücresinde birden çok renk var (ör. `Siyah çizgi = 0/0/0` + `Sarı çizgi = 255/255/115`); tek dolgu rengi seçilemez | 4 |
| `simge-renk-coklu` | Sembol hücresinde birden çok renk sayılmış; hangisinin hangi alt türe ait olduğu ekten okunamıyor | 1 |
| `ad-paragrafi-yok` | EK-1e kartının adını veren paragraf kaynakta bulunamadı | 5 |
| `ad-paragraf-sayisi-belirsiz` | EK-1e kartından önce birden çok paragraf var; hangisinin ad olduğu seçilemedi, hepsi `ad_adaylari` altında | 9 |
| `cozulmemis-satir` | Kart içinde bilinen hiçbir satır türüne uymayan bir satır var; ham hâliyle saklandı | 1 |
| `asgari-alan-sayiya-cevrilemedi` | EK-2 hücresi birim bağımlı (`Yatak başına (130) m²`), düz m² değeri değil | 6 |

Bu satırların hepsi katalogda durur ve okunabilir; yalnız türetilmiş görünüm alanları
boştur.

## EK-2 asgari standartlar

EK-2 bir gösterim tablosu değil, **asgari alan standardı** tablosudur: TAKS/KAKS
komşusu mevzuat değerleri. Ayrı bir katalogdadır.

m²/kişi değerleri **binde tam sayı** olarak saklanır: `0.5` → `500`, `10,00` →
`10000`, `1.25` → `1250`. Kayan nokta saklanmaz — PiriCAD'de kayan nokta bir ara
değerdir, saklanan biçim değildir. Her değerin yanında kaynak hücrenin metni de
durur (`m2_kisi_metin`), böylece çeviri denetlenebilir.

Asgari birim alan sütununda nokta **binler ayırıcısıdır**: `1.500-3.000` = 1500–3000 m².

İki noktaya dikkat edin:

- **Birleşik hücreler.** Kaynak tabloda bir değer birden çok satırı kapsıyorsa
  (ör. açık ve yeşil alanlar için 10 m²/kişi, yedi kalemi birden kapsar) o değer
  kapsadığı her kaleme yazılır ama `m2_kisi_birlesik_hucre` alanıyla işaretlenir.
  Bu bir **grup** değeridir; kalem başına ayrı bir standart değildir. Tablonun
  9. ve 10. açıklama maddeleri bunu anlatır.
- **Nüfus grubu boşluğu.** Kaynak tabloda `150.001 - 500.000` ile `501.000 +`
  grupları arasında 500.001–500.999 aralığı tanımsızdır. Bu kaynağın kendi
  yazımıdır ve düzeltilmemiştir.

Tablonun 13 maddelik AÇIKLAMALAR bloğu `aciklamalar` dizisinde birebir durur.

## Paketi yeniden üretmek

Katalog elle yazılmaz; resmî ek dosyalarından üretilir. Elle düzenlenmiş bir katalog
kabul edilmez ve CI kapısı bunu yakalar.

1. Resmî Gazete'den yedi ek dosyasını indirin ve bir dizine koyun. Dosya adları
   `scripts/mpyy-cikar.py` içindeki `EKLER` tablosunda yazılıdır.
2. Çıkarımı çalıştırın:

```bash
python3 scripts/mpyy-cikar.py --kaynak /yol/.mpyy-kaynak
```

3. Kapıyı çalıştırın; kaynak dizin duruyorsa çıkarımı yeniden koşup bayt birebir
   karşılaştırır:

```bash
PIRICAD_MPYY_KAYNAK=/yol/.mpyy-kaynak bash scripts/ci-gate-mpyy.sh
```

Betik yalnız Python 3 standart kütüphanesini kullanır; kurulacak bir şey yoktur.
Kaynak dosyalar depoya girmez (`.gitignore`), üretilen katalog girer.

Katalog içeriği değiştiyse önce farkı okuyun, sonra golden özetini yenileyin ve
`CHANGELOG.md`'ye değişikliğe sebep olan yönetmelik veya genelgeyi yazın:

```bash
PIRICAD_GOLDEN_UPDATE=1 bash scripts/ci-gate-mpyy.sh
```

## Paketi kullanmak

Bugün gösterim satırlarına [`STİL`](../komutlar/style.md) komutunun `paket=` ve
`kod=` parametreleriyle erişilir:

```text
STİL katman=PARSEL paket=<depo kökü>/data/catalogs/mpyy-vektor/plan-gosterim.json kod=uip-ticaret-alani
```

Vektör paketi yerine resimli paketin yolu da verilebilir; o zaman satır yönetmeliğin
bastığı resimle çizilir.

Yol çalışma dizinine göre çözülür; `<depo kökü>` yerine kendi yolunuzu yazın. Satır
kimlikleri `<ek kısaltması>-<ad>` biçimindedir: `ortak-` (EK-1a), `msp-` (EK-1b),
`cdp-` (EK-1c), `nip-` (EK-1ç), `uip-` (EK-1d). Ek harfi yerine planın kısaltması
kullanılır, çünkü `EK-1c` ile `EK-1ç` ASCII'ye indirildiğinde aynı yazılır ve
kimlikler çakışırdı.

Eşleme kuralları eklendiğinde katmanı tek tek kodlamak yerine kural kümesi
uygulanabilecektir; bu Faz 3'ün işidir.

## Neyin denetlendiği

İki kapı vardır. `scripts/ci-gate-mpyy-vektor.py` vektör paketini denetler:

1. Yazılan her katman şemanın sözlüğüne uyuyor mu — tip, şekil, yerleşim, birim
   tanınıyor mu, renkler `#AARRGGBB` mi, ölçüler tam sayı mı;
2. Her satır `belirsiz: true` ve `cizim-yorumu` taşıyor mu;
3. Her `gorsel` göndermesi pakette gerçekten duran bir SVG'yi gösteriyor mu;
4. `YAPILACAKLAR.md` **doğru mu söylüyor** — işaretli ama pakette olmayan bir satır
   kusurdur, çünkü kaçamağın alacağı biçim tam olarak budur.

`scripts/ci-gate-mpyy.sh` resimli paket için üç şeyi denetler:

1. Katalog özeti — ek başına satır sayısı, referans satırların renkleri, `belirsiz`
   listesi — `tests/golden/mpyy/beklenen.txt` ile birebir eşleşiyor mu;
2. Üç katalog dosyasının SHA-256'sı kayıtlı değerle aynı mı (elle düzenlemeyi
   yakalayan tek şey budur);
3. Kaynak ekler duruyorsa, yeniden çıkarım aynı baytları üretiyor mu.

## İlgili sayfalar

- [Nesne stili ve gösterim kataloğu](../komutlar/style.md) — `STİL` komutu
- [Koordinat sistemleri](koordinat-sistemleri.md) — milimetre depolama ve TM 3° dilimleri
- [Sözlük](../sozluk.md) — gösterim, detay kataloğu, TAKS/KAKS ve diğer terimler
