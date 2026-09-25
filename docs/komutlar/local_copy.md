# YERELKOPYA — Dış Referanstan Yerel Kopya

Bağladığınız bir dosyanın — komşu paftanın, kadastro altlığının, bir GeoPackage
katmanının — bir parselini düzeltmek, bir yol eksenini kendi projenizde sürdürmek
istediğinizde. Bu sayfayı bitirdiğinizde bağlı bir dosyanın nesnelerini, bağlantıyı
bozmadan, çiziminizin kendi nesneleri olarak alabileceksiniz.

## Ne yapar

Bir [dış referansın](xref.md) nesneleri kendi dosyasındandır: her açılışta ve
yenilemede oradan okunur, bu yüzden burada düzenlenmez — değiştirseniz bir sonraki
yenilemede kaybolurdu. YERELKOPYA o nesnelerin **düzenlenebilir birer kopyasını** bu
çizime alır:

- Kopya, dış referansın çizdiği **yerde** durur (referansın konumu, dönüklüğü ve
  ölçeğiyle), **değerleriyle** ve görünüşüyle gelir.
- Kopya, kaynağın kendi katman adıyla **bu çizimin katmanına** konur: `altlik|PARSEL`
  katmanındaki bir parselin kopyası `PARSEL` katmanına gider; çizimde böyle bir katman
  yoksa açılır. `katman=` başka bir katman verir.
- **Bağlantı olduğu gibi kalır.** Dış referans yerinde durur, yenilenmeye devam eder;
  kopyalar artık bu çizimindir ve onu izlemez.
- Her kopya **kökenini** bilir: hangi dış referanstan alındığı, öznitelik panelinde ve
  [NESNEBİLGİ](entity_info.md)'de görünür ([nesne kimliği ve kökeni](../veri/kimlik-ve-koken.md)).

Bağlı bir dosyanın nesnesini düzenlemeye kalktığınızda — [PATLAT](explode.md),
[BLOKDÜZENLE](block_edit.md), bir değer yazmak — program reddeder ve bu komutu önerir:
arayüzde tuvalin üstünde **Yerel Kopya** düğmesiyle, komut satırında
`Öneri: YERELKOPYA nesneler=…` satırıyla.

## Adlar

| Ad | Tür |
|---|---|
| `YERELKOPYA` | Türkçe ana ad |
| `LOCALCOPY` | İngilizce |
| `YK` | Kısaltma |

## Sözdizimi

```text
YERELKOPYA nesneler=<kimlik…> [katman=<ad>] [pencere=<köşe> <köşe>]
YERELKOPYA ad=<dış referans> [katman=<ad>] [pencere=<köşe> <köşe>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Dış referans (çizimdeki yerleşimi) ya da onun nesneleri. Referans verilirse bütün nesneleri kopyalanır |
| `ad` | `nesneler` yerine: dış referansın bağlandığı ad; çizimdeki yerleşimi kullanılır |
| `katman` | Kopyaların konacağı katman. Verilmezse her kopya kaynağındaki katmanın adını alır |
| `pencere` | İki köşe: yalnız bu dikdörtgene değen nesneler kopyalanır |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

<!-- örnek: yeni çizim -->

Bir dış referansın bütün nesneleri, kaynak katman adlarıyla — seçerek ya da adıyla:

```text
YERELKOPYA nesneler=3
YERELKOPYA ad=altlik
```

Yalnız bir bölgesi, taslak katmanına:

```text
YERELKOPYA nesneler=3 katman=TASLAK pencere=485300,4310200 485400,4310300
```

### Arayüz

Dış referansı seçin ve **Harita ▸ Veri ▸ Dış Referans** grubundan **Yerel Kopya**'ya
basın; ya da bağlı dosyanın bir nesnesini düzenlemeye kalktığınızda tuvalin üstünde
çıkan şeritteki **Yerel Kopya** düğmesine basın. Şerit reddin sebebini de yazar.

### Betik

Bir altlığı bağlayıp bütün nesnelerinin kopyasını almak — betik dış referansı
bağladığı adıyla bilir:

```json
{"ad": "altliktan al", "komutlar": [
  {"cmd": "core.xref", "args": {"dosya": "altlik.pcad"}},
  {"cmd": "core.local_copy", "args": {"ad": "altlik", "katman": "TASLAK"}}
]}
```

Python'dan: `cad.local_copy(name="altlik", layer="TASLAK")`.

### Üçü de aynı

Arayüz, komut satırı ve betik aynı kopyaları aynı katmana, aynı değerlerle bırakır;
günlüğe aynı satır yazılır.

## Geri alma

Bütün kopyalar tek adımda geri alınır (Ctrl+Z ya da `GERİAL`); dış referansa
dokunulmaz.

## Betikten kullanım

Yapılandırılmış cevap `adet` (kopya sayısı) ve `referanslar` taşır: her dış referans
için `referans` (kimliği), `ad` ve `kopyalar` (yeni nesnelerin kimlikleri). Dosyanın
kendi bloğuna iç referanslar kopyalanmazsa `alinmayan_ic_referans` sayısı gelir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Nesne N bir dış referans değil; bu çizimin kendi nesnesi zaten düzenlenebilir. …` | Çizimin kendi nesnesi verildi | Bir kopyası için [KOPYALA](copy.md) kullanın |
| `Nesne N bir dış referansın parçası değil; …` | Verilen nesne bir blok ya da dış referans üyesi değil | Aynı |
| `'X' dış referansı çizimde hiçbir yere yerleşmemiş; kopyanın konacağı yer yok.` | Dış referansın çizimde yerleşimi yok | Önce [DIŞREFERANS](xref.md) ile yerleştirin |
| `Pencereye değen bir nesne yok; hiçbir şey kopyalanmadı.` | `pencere=` hiçbir nesneye değmiyor | Pencereyi genişletin |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok | Kimliği [NESNEBİLGİ](entity_info.md) ya da seçimle doğrulayın |
| `'X' adında bir dış referans yok.` | `ad=` bilinmeyen bir ad | Bağlı dış referansları `DIŞREFERANS islem=listele` gösterir |

Dosyanın kendi bloklarına (`altlik|KAPAK` gibi) konmuş iç referanslar kopyalanmaz ve
mesaj sayısını söyler: onlar dosyanın tanımını çizer. Onlar için referansı
`DIŞREFERANS islem=bagla` ile çizime bağlayın.

## İlgili

- [DIŞREFERANS](xref.md) — bir dosyayı bağlamak, yenilemek, çizime bağlamak
- [İÇEAKTAR](import.md) — bir dosyanın bütün nesnelerini bir kez kopyalamak
- [Nesne kimliği ve kökeni](../veri/kimlik-ve-koken.md)
