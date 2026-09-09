# NOKTALAR — Ölçü Nokta Listesi

## Ne yapar

Ölçülmüş nokta listesini okur ve yazar. Sahadan dönen ekibin total station,
GNSS alıcısı ya da bir meslektaşın Netcad çıktısı olarak getirdiği numaralı
liste — işin geri kalanı bundan başlar.

Okunan her satır çizime bir **nokta** koyar ve üç özniteliği doldurur:

| Sütun | Öznitelik |
|---|---|
| nokta numarası | `nokta_no` |
| kot (Z) | `kot` |
| kod | `kod` |

## Y sağadır, X yukarıdır

Türkiye'de bir nokta listesi `nokta no, Y, X, Z` yazılır: **Y doğu-batı (sağa),
X kuzey-güney (yukarı)**. Matematik kitabının tersidir ve bu program bunu böyle
okur.

Dosyanız gerçekten ters sıradaysa `eksen=XY` deyin. Bu **söylenmelidir**,
tahmin edilmez: 485 320'lik bir sağa değerini 485 320'lik bir yukarı
değerinden ayırt edebilecek hiçbir sezgi yoktur, ve yanlış okunan bir liste
her noktayı makul görünen yanlış bir yere koyar.

## Biçim

```text
nokta_no ; Y ; X ; [Z] ; [kod]
```

- **Ayraç** dosyadan anlaşılır: noktalı virgül, sekme, virgül ya da boşluk.
- **Ondalık ayracı** hem `.` hem `,` olabilir — virgülle ayrılmış bir dosyada
  ondalık zorunlu olarak `.`'tır, diğerlerinde ikisi de okunur. Türkçe yerel
  ayarlı bir dışa aktarma `485320,543` yazar ve bunu reddetmek çoğu gerçek
  dosyayı reddetmek olurdu.
- `#`, `//` veya `*` ile başlayan satırlar atlanır; başlık satırı da atlanır.
- Dördüncü sütun sayı değilse **kod** sayılır: `no;Y;X;AGAC` gibi Z'siz listeler
  yaygındır.

Koordinatlar **basamak basamak** okunur, `strtod` ile değil: dokuz haneli bir
sağa değerinin üç ondalığı çift duyarlıkta tam temsil edilemez ve son
milimetresi bir parsel köşesinde önemlidir.

## Adlar

| Türkçe | İngilizce | Kısaltma |
|---|---|---|
| `NOKTALAR` | `POINTS` | `NKL` |

## Sözdizimi

```text
NOKTALAR dosya=<yol> [yon=oku|yaz] [eksen=YX|XY] [nesneler=<kimlikler>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `dosya` | Nokta listesinin yolu |
| `yon` | `oku` (varsayılan) ya da `yaz` |
| `eksen` | Sütun sırası: `YX` (varsayılan) ya da `XY` |
| `nesneler` | Yalnız `yon=yaz` ile: köşeleri yazılacak nesnelerin kimlikleri, virgülle. Verilmezse çizimdeki nokta nesneleri yazılır |

## Örnekler

### Komut satırı

```text
KATMAN ad=NIRENGI
NOKTALAR dosya="olcu.txt"
```

Okunan dosya:

```text
# Ada 1284 poligon ölçüsü
1;485320,543;4310220,250;845,120;NIRENGI
2;485360.000;4310220.000;845.300;PARSEL
3 485360.000 4310265.000 845.900 PARSEL
```

Geri yazmak:

```text
NOKTALAR dosya="cikti.txt" yon=yaz
```

Yazılan dosya noktalı virgülle ayrılır ve ondalık olarak `.` kullanır: Türkçe
yerel ayarlı bir hesap tablosu `485320.543`'ü ikiye bölmeden açar ve dosyayı
sonradan kim okursa okusun belirsizlik kalmaz.

### Bir nesnenin köşelerini yazmak

Aplikasyona çıkacak ekip parselin köşelerini ister, noktalarını değil. `nesneler`
verildiğinde her köşe bir satır olur; satır numarası `nesne.köşe` biçimindedir ve
kod sütununa katmanın adı yazılır:

```text
NOKTALAR dosya="koseler.txt" yon=yaz nesneler=4128
```

```text
4128.1;485300.000;4310200.000;Kadastro Parselleri
4128.2;485360.000;4310200.000;Kadastro Parselleri
4128.3;485360.000;4310245.000;Kadastro Parselleri
4128.4;485300.000;4310245.000;Kadastro Parselleri
```

Birden çok nesne virgülle verilir: `nesneler=4128,4129`. Delikli bir parselin iç
halkası dış halkanın ardından, numara sırasıyla devam eder.

### Arayüz

Okumak için `NOKTALAR dosya="..."` komut satırından. Okunan noktalar aktif katmana
düşer, yani önce `KATMAN` ile hedefi seçin.

Bir nesnenin köşelerini yazmak için sağdaki **Öznitelikler** panelinde nesne
seçiliyken panele **sağ tıklayın** (ya da klavyede **Menü** tuşuna / **Shift+F10**'a
basın) ve **Koordinatları dışa aktar…** deyin. [Dışa Aktar](../baslangic/disa-aktarma.md)
penceresi açılır; dosyayı seçtiğinizde pencere kuracağı `NOKTALAR … yon=yaz nesneler=…`
satırını altta gösterir ve **Dışa aktar** ile çalıştırır. Aynı menüdeki
**Koordinatları kopyala**, aynı satırları panoya koyar.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",  "args": { "ad": "NIRENGI" } },
    { "cmd": "core.points", "args": { "dosya": "olcu.txt" } }
  ]
}
```

## Geri alma

**Tek adımdır**: yirmi nokta okuyan bir çağrı tek `GERİAL` ile bütünüyle kalkar.

Sütun tanımları kalkmaz, ve bu tasarımdır: bir sütun tanımlamak **şema**
değişikliğidir ve `SÜTUN` komutu da geri alınamaz — satırlar şemaya göre
adreslenir, tanımı geri almak o şemaya yazılmış her satırı geçersiz kılardı.
Aynı listeyi yeniden okumak ikinci bir `nokta_no` sütunu açmaz.

## Betikten kullanım

Dosya yolu argüman olduğu için bir betik ölçü klasörünü sırayla okuyabilir.
Herhangi bir satır bozuksa **hiçbiri** okunmaz — işlem bütünüyle geri alınır.

## Hatalar

> `<dosya>:<satır> — koordinat okunamadı ('...', '...').`

O satırdaki iki koordinat alanından biri sayı değil. Bir nokta listesinin sessizce
bir satır eksik olması, bir sınırın sessizce bir köşe eksik olması demektir; bu
yüzden atlanmaz.

> `<dosya>:<satır> — bir noktanın en az numarası ve iki koordinatı olmalı; N alan var.`

Satır kısa. Baştaki başlık satırı bir kez atlanır, ortadaki kısa satır atlanmaz.

> `Dosyada okunabilir nokta yok: <dosya>. Beklenen biçim: nokta_no; Y; X; [Z]; [kod]`

Dosyada hiç veri satırı bulunamadı.

> `Nokta listesi çok büyük: 5000000 noktadan fazlası okunmuyor.`

Sınır, hasımca bir dosyanın belleği tüketmesini engeller.

> `Çizimde nokta yok. NOKTA komutuyla çizin ya da bir liste okuyun.`

`yon=yaz` verildi ama çizimde yazılacak nokta yok.

> `nesneler yalnız yon=yaz ile verilir: köşeleri yazılacak nesneler.`

`nesneler` bir okuma çağrısında verildi. Okunan bir listenin seçilecek nesnesi yoktur;
`yon=yaz` ekleyin.

> `Nesne bulunamadı veya silinmiş: 99`

`nesneler` içindeki bir kimlik çizimde yok. Kimliği panelden ya da öznitelik
tablosunun `fid` sütunundan okuyun.

> `Verilen nesnelerin yazılacak köşesi yok.`

Verilen nesnelerin hiçbirinin geometrisi köşe taşımıyor.

## İlgili

- [NOKTA](point_draw.md) — tek nokta çizme
- [KOORDİNAT](coordinate.md) — bir noktanın değerini okuma
- [İÇEAKTAR](import.md) — DXF ve GeoPackage
