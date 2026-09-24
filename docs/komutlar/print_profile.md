# YAZDIRMAPROFİLİ — Yazdırma Profilleri

Ofisin her zaman aynı kâğıda, aynı çözünürlükte çıktı almasını isteyen herkes için;
bu sayfayı bitirdiğinizde yazdırma profillerini listelemeyi, eklemeyi, silmeyi ve
birini varsayılan yapmayı bileceksiniz.

## Ne yapar

Bir **yazdırma profili** adlandırılmış bir kâğıttır: kâğıt boyu, **yön**,
**çözünürlük** ve **kenar boşluğu**. Ofis birkaç tane tutar — pafta için "A3 Yatay
300 dpi", röper krokisi için "A4 Dikey" — ve tam **biri varsayılandır**: araç
çubuğundaki Yazdır düğmesi onu kullanır.

`YAZDIRMAPROFİLİ` bu listeyi yönetir. Profiller **kullanıcı profilinizde** bir JSON
dosyasında tutulur; çizim dosyasına yazılmaz, komut günlüğüne girmez ve
[`GERİAL`](undo.md) ile geri alınmaz — bir yazıcı tercihi gibi, size ve makinenize
aittir.

Yeni bir kurulumda altı profil vardır: **A4 Dikey** (varsayılan), A4 Yatay, A3
Yatay, A2 Yatay, A1 Yatay, A0 Yatay; hepsi 300 dpi ve 10 mm kenar boşluğu.

## Adlar

| Ad | Tür |
|---|---|
| `YAZDIRMAPROFİLİ` | Türkçe, birincil |
| `YAZDIRMAPROFILI` | ASCII karşılık |
| `PRINTPROFILE` | İngilizce karşılık |
| `YZP` | Kısaltma |
| `core.print_profile` | Komut kimliği |

## Sözdizimi

```text
YAZDIRMAPROFİLİ islem=listele
YAZDIRMAPROFİLİ islem=ekle ad=<ad> kagit=A3 yon=yatay dpi=300 kenar=10
YAZDIRMAPROFİLİ islem=varsayilan ad=<ad>
YAZDIRMAPROFİLİ islem=sil ad=<ad>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | `listele`, `ekle`, `sil` ya da `varsayilan` |
| `ad` | Profilin adı (`ekle`, `sil`, `varsayilan` için zorunlu) |
| `kagit` | `A5`, `A4`, `A3`, `A2`, `A1`, `A0` ya da `ozel`; varsayılan `A4` |
| `genislik` | `ozel` kâğıdın eni, milimetre (dikey duruşta); `ozel` ile zorunlu |
| `yukseklik` | `ozel` kâğıdın boyu, milimetre (dikey duruşta); `ozel` ile zorunlu |
| `yon` | `dikey` ya da `yatay`; varsayılan `dikey` |
| `dpi` | Çözünürlük, 72–4800; varsayılan 300 |
| `kenar` | Kenar boşluğu, milimetre; varsayılan 10 |

`ekle`, var olan bir ada **yazar**: aynı adla ikinci kez eklemek o profili
değiştirir. Ölçüler her zaman kâğıdın **dikey** duruşundaki en ve boyudur; `yon`
onu çevirir.

## Örnekler

### Komut satırı

Profilleri listelemek — varsayılan `*` ile işaretlidir:

```text
YAZDIRMAPROFİLİ islem=listele
```

Ofisin pafta kâğıdını eklemek ve varsayılan yapmak:

```text
YAZDIRMAPROFİLİ islem=ekle ad="Pafta A3" kagit=A3 yon=yatay dpi=400 kenar=8
YAZDIRMAPROFİLİ islem=varsayilan ad="Pafta A3"
```

Rulo kâğıt — `ozel` ölçüsünü söyler:

```text
YAZDIRMAPROFİLİ islem=ekle ad=Rulo kagit=ozel genislik=900 yukseklik=1200 yon=dikey
YAZDIRMAPROFİLİ islem=sil ad=Rulo
```

### Arayüz

`Seçenekler ▸ Plot ve Çıktı` sayfasının başında **Yazdırma Profilleri** tablosu
vardır: her satır bir kâğıt, `●` olan varsayılandır. Bir satır seçince **Varsayılan
yap** ve **Sil** düğmeleri açılır (Sil her zaman onay ister). Altındaki **Yeni
profil** satırında ad, kâğıt, yön, çözünürlük ve kenar boşluğunu verip **Ekle**'ye
basın. Aynı pencereye şeritteki **Çıktı ▸ Yazdır ▸ Yazdır** düğmesinin okundaki
**Profilleri Yönet…** de götürür.

Tablo komut satırını izler: `YAZDIRMAPROFİLİ` ile yaptığınız değişiklik pencere
açıkken de görünür, çünkü ikisi aynı listeye bakar.

### Betik

```json
{
  "ad": "Ofis kâğıdı",
  "komutlar": [
    { "cmd": "core.print_profile",
      "args": { "islem": "ekle", "ad": "Pafta A3", "kagit": "A3",
                "yon": "yatay", "dpi": 400, "kenar": 8 } },
    { "cmd": "core.print_profile", "args": { "islem": "varsayilan", "ad": "Pafta A3" } }
  ]
}
```

## Geri alma

Profiller uygulama ayarıdır: [`GERİAL`](undo.md) onlara dokunmaz. Yanlış bir
değişikliği geri almanın yolu profili yeniden `ekle` ile yazmak ya da silmektir.

## Betikten kullanım

Betikte `islem` her zaman, `ad` ise `listele` dışındaki her işlemde verilmelidir.
Komut günlüğüne yazılmaz (bir uygulama tercihidir), bu yüzden yeniden oynatma
profilleri değiştirmez.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Tanınmayan işlem: 'X'. İşlemler: listele / ekle / sil / varsayilan` | `islem` sözcüğü yanlış | Dördünden birini yazın |
| `'sil' için profil adı gerekir: ad=<ad>` | `ad` verilmedi | Profil adını verin |
| `'yon' dikey ya da yatay olmalı. Girilen: 'X'` | Yön sözcüğü yanlış | `dikey` ya da `yatay` |
| `kagit=ozel için genislik ve yukseklik milimetre olarak verilmeli: …` | Özel kâğıdın ölçüsü yok | İkisini de verin |
| `Tanınmayan kâğıt: 'X'. Kâğıtlar: A5, A4, A3, A2, A1, A0, ozel.` | Kâğıt tablonun dışında | Listeden birini yazın |
| `dpi 72 ile 4800 arasında olmalı; verilen N.` | Çözünürlük aralık dışı | Aralıkta bir sayı verin |
| `Kenar boşluğu kâğıtta yazdırılacak yer bırakmıyor: …` | Kenar boşluğu kâğıdı tüketiyor | Küçültün |
| `Böyle bir yazdırma profili yok: 'X'.` | `sil`/`varsayilan` bilinmeyen ad | `listele` ile bakın |
| `Son profil silinemez; önce başka bir profil ekleyin.` | Tek profil kalmış | Önce yeni profil ekleyin |
| `Profil adı boş olamaz.` | `ad=""` verildi | Bir ad yazın |
| `Yazdırma motoru bağlı değil; …` | Arayüz olmadan çalıştırıldı | Uygulama içinden çalıştırın |

## İlgili

- [Yazdırma ve PDF](../baslangic/yazdirma.md)
- [`YAZDIR`](print.md) — çizimi kâğıda ya da PDF'e verme
- [`TERCİH`](preference.md) — uygulama tercihleri
