# UÇUCA — Uç Uca Ekle

Parça parça sayısallaştırılmış bir yolu, bir dereyi ya da bir sınırı tek bir
nesne hâline getirecek herkes için.

## Ne yapar

Uçları birbirine **değen** çizgileri tek bir çizgiye ekler. Sıra önemsizdir ve
ters çizilmiş bir parça gerekirse çevrilir — sayısallaştırılmış bir harita tam
olarak böyle görünür.

**`BİRLEŞTİR` ile karıştırmayın.** İkisi farklı sorudur:

| Komut | Ne yapar |
|---|---|
`UÇUCA` | Uçları değen **çizgileri** tek çizgi yapar (topolojik ekleme) |
`BİRLEŞTİR` | Örtüşen **alanları** tek alan yapar (poligon boolean) |

Adları karıştırmak, ikisini de kullanılmaz kılar; bu yüzden her iki sayfa
öbürünü adıyla anar.

## Tolerans

Uçların "değmiş" sayılması için en büyük açıklık `tolerans=` ile verilir,
**metre** cinsinden, varsayılan **1 mm**. Bu bir ölçünün özelliğidir, farenin
değil: kadastral bir çizim 1 mm ister, elle sayısallaştırılmış bir harita bir
metre isteyebilir.

Zincire değmeyen bir çizgi **olduğu gibi bırakılır** — yerine çekilmez.

## Adlar

| Ad | Tür |
|---|---|
| `UÇUCA` | Türkçe, birincil |
| `UCUCA` | ASCII katlanmış Türkçe |
| `JOIN` | İngilizce karşılık |
| `UÇE` | Kısaltma |
| `core.join` | Komut kimliği |

## Sözdizimi

```text
UÇUCA nesne=<kimlik> nesne=<kimlik> [nesne=… …] [tolerans=<m>]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 0..n | Uç uca eklenecek çizgiler |
| `tolerans` | sayı | 0..1 | Uçların değmiş sayılması için en büyük açıklık (m); varsayılan 0,001 |

## Örnekler

### Komut satırı

Üç parçayı — biri ters çizilmiş — tek çizgiye eklemek:

```text
UÇUCA nesne=1 nesne=2 nesne=3
```

```text
3 çizgi tek bir çizgiye eklendi (4 köşe).
```

5 metrelik bir açıklığı kapatmak:

```text
UÇUCA nesne=1 nesne=2 tolerans=5
```

### Arayüz

**Değiştir > Uç Uca Ekle**. Çizgileri seçip Enter'a basın.

### Betik

```json
{ "cmd": "core.join", "args": { "nesne": [1, 2, 3], "tolerans": 0.005 } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `UÇUCA en az iki çizgi ister; N geldi.` | Tek nesne seçili | En az iki çizgi seçin |
| `Seçilen çizgilerin uçları birbirine değmiyor (tolerans N mm).` | Açıklık toleransı aşıyor | `tolerans=` ile büyütün ya da uçları yakalama açıkken yeniden çizin |
| `Nesne N açık bir çizgi değil.` | Kapalı alan | `ÇİZGİDÜZENLE islem=ac` ile açın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BİRLEŞTİR](combine.md) — alanları tek alan yapar, bu ise çizgileri
- [ÇİZGİDÜZENLE](pedit.md) — kapat / aç / ters / sadeleştir
- [KIR](break.md) — parça çıkarır
