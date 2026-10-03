# KALEMDENETİM — Sınıf Denetimi

Bir sayısallaştırma paftasının teslim edilmeden önce sorulan sorusu için: "bu katmandaki her şey
hâlâ sınıfının dediği gibi mi?" Bu sayfayı bitirdiğinizde bir sınıfın neyi denetlediğini, bulguların
nasıl okunduğunu ve sorunlu nesneleri nasıl seçeceğinizi bileceksiniz.

## Ne yapar

`KALEMDENETİM`, bir sınıfı izleyen katmanların nesnelerine sınıfın kurallarını sorar ve bulguları
sayar. **Hiçbir şey değiştirmez.**

| Kural | Ne zaman bulgu verir |
|---|---|
| **geometri** | Nesne sınıfın şekli değil (bina katmanında açık çizgi). Çizerken bu zaten reddedilir; bulgu, sınıfı **sonradan devralmış** bir katmandaki eski nesneler içindir |
| **çok küçük** | Alan sınıfında nesnenin alanı sınıfın `en_az_alan_m2` sınırının altında |
| **çok kısa** | Çizgi sınıfında nesnenin uzunluğu sınıfın `en_az_uzunluk_m` sınırının altında |
| **eksik değer** | `zorunlu` bir alan boş |
| **listede yok** | Alanın `secenekler` listesi var ve değer onların dışında |
| **eksik sütun** | Sınıfın bir alanının sütunu belgeden silinmiş (nesne başına değil, sınıf için bir bulgu; `KALEM <sınıf>` yeniden tanımlar) |

Bu kurallar **çizimde sorulmaz, burada sorulur.** Bilinmeyen bir ada numarası yüzünden çizimi
engellemek yanlış sırada soru sormaktır: parsel önce çizilir, numarası sonra gelir.

**Kapsam:** argümansız komut sınıf izleyen **bütün** katmanları denetler. `ad=` yalnız o sınıfın
katmanını, `katman=` yalnız o katmanı denetler.

**Paket bulunamazsa** katmandaki nesneler yerinde kalır ve özet denetlenemediklerini söyler.
Katman paketin eski bir sürümüyle bağlanmışsa özet bunu da söyler (`katman 0.1.0 sürümüyle
bağlanmış, paket şimdi 0.2.0`).

**`sec=evet`** sorunlu nesneleri çizimde seçer; bir sonraki komut tam bunlara uygulanır
(`SEÇ mod=ÖNCEKİ` önceki seçimi geri getirir).

## Adlar

| Türkçe | Tür |
|---|---|
| `KALEMDENETİM` | Türkçe, birincil |
| `KALEMDENETIM` | ASCII karşılık |
| `SINIFDENETİM` | Türkçe eşanlamlı |
| `KLD` | Kısaltma |
| `CHECKCLASS` | İngilizce karşılık |
| `core.feature_class_check` | Komut kimliği |

## Sözdizimi

```text
KALEMDENETİM [ad=<sınıf>] [katman=<ad>] [sec=evet]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Yalnız bu sınıfın katmanı; verilmezse sınıf izleyen bütün katmanlar |
| `katman` | Yalnız bu katman |
| `sec` | `evet` ise sorunlu nesneleri seç. Varsayılan `hayır` |

## Örnekler

### Komut satırı

İki bina çizin; birinin yapı türünü sınıfın listesinde olmayan bir değerle değiştirin:

```
KALEM bina
ALAN noktalar=0,0 12,0 12,9 0,9
ALAN noktalar=20,0 32,0 32,9 20,9
ÖZNİTELİK yapi_turu 2 Cam
```

Denetleyin:

```
KALEMDENETİM
```

```text
Sınıf denetimi:
BINA — Bina: 2 nesne, 1 bulgu
    listede yok: 1
    [2] 'yapi_turu' = 'Cam', izin verilenler arasında değil
```

Sorunlu nesneyi seçin ve düzeltin:

```
KALEMDENETİM ad=bina sec=evet
ÖZNİTELİK yapi_turu 2 Yığma
KALEMDENETİM ad=bina
```

Eski bir katmanı devraldığınızda önceki nesneler denetlenmez, çünkü onlar sınıftan önce oradaydı;
`KALEM` bunu söyler ve bu komut onları bulur. Yeni bir çizimde, içinde açık bir çizgi olan bir
`BINA` katmanı kurup bina kalemini alın:

<!-- örnek: yeni çizim -->

```
KATMAN ad=BINA
ÇİZGİ 0,60 10,60
KALEM bina
KALEMDENETİM ad=bina
```

```text
Kalem: Bina — kapalı alan, katman BINA, 4 alan (4 yeni sütun)
  Başlangıç değerleri: sinif_kodu=BNA · kat_sayisi=1 · yapi_turu=Betonarme
  Şimdi çizdiğiniz her kapalı alan BINA katmanına gider ve bu değerlerle başlar; sınıfa uymayan şekil reddedilir.
  Uyarı: BINA katmanında zaten 1 nesne var; denetlenmediler. KALEMDENETİM bakar.
Sınıf denetimi:
BINA — Bina: 1 nesne, 1 bulgu
    geometri: 1
    [1] sınıf kapalı alan ister, nesne açık çizgi
```

### Arayüz

**Harita ▸ Kalem ▸ Sınıfı denetle** bütün sınıf katmanlarını denetler ve özeti transkripte yazar.
Özelliği (`sec`, `ad`, `katman`) komut satırından kullanın.

### Betik

```json
{
  "ad": "Sınıf denetimi",
  "komutlar": [
    { "cmd": "core.feature_class", "args": { "ad": "bina" } },
    { "cmd": "core.area", "args": { "noktalar": [[0,0],[12000,0],[12000,9000],[0,9000]] } },
    { "cmd": "core.feature_class_check", "args": { "ad": "bina" } }
  ]
}
```

## Geri alma

Gerek yok: komut hiçbir şeyi değiştirmez. `sec=evet` yalnız seçimi değiştirir ve belgeye,
günlüğe ve geri alma yığınına dokunmaz.

## Betikten kullanım

Komut kimliği `core.feature_class_check`; Python'dan
`cad.feature_class_check(name="bina", select=True)`. Yapılandırılmış sonuç:

| Alan | Ne |
|---|---|
| `toplam` | Bulgu sayısı |
| `katmanlar` | Her katman için `katman`, `sinif`, `nesne`, `denetlendi` ve `bulgu` sayısı |
| `bulgular` | En çok 200 bulgu: `katman`, `nesne` (sınıf bulgusunda 0), `kural`, `alan`, `ayrinti` |

Yapay zekâ da çağırabilir; yalnız okuduğu için onay istemez.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen sınıf: '<ad>'. Tanımlı sınıflar: …` | Sınıf paketde yok | [KALEM](feature_class.md) ile listeyi görün |
| `'<sınıf>' sınıfını izleyen bir katman yok; önce KALEM <sınıf> ya da KALEMBAĞLA.` | Sınıf hiç kurulmamış | Önce `KALEM` |
| `Bilinmeyen katman: '<ad>'.` | `katman` çizimde yok | Adı doğrulayın |
| `'<ad>' katmanı bir sınıfı izlemiyor; KALEMBAĞLA ile bağlayın.` | Katman sınıf izlemiyor | [KALEMBAĞLA](feature_class_bind.md) |
| `<katman>: <sınıf> sınıfı bu kataloğda yok (paket '<paket>'); N nesne denetlenemedi.` | Katman başka bir paketin sınıfını izliyor ya da sınıf paketten çıkarılmış | `core.kalem.katalog` ayarını o pakete çevirin |
| `Bir sınıfı izleyen katman yok; denetlenecek bir şey yok. Başlamak için: KALEM` | Çizimde sınıf izleyen katman yok | `KALEM` |

## İlgili

- [KALEM](feature_class.md), [KALEMBAĞLA](feature_class_bind.md)
- [Kalem kataloğu](../veri/kalem-katalogu.md) — denetimlerin tanımı (`dogrulama`, `zorunlu`, `secenekler`)
- [SEÇ](select.md)
