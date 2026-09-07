# YAZIDÜZENLE — Var Olan Yazıyı Değiştirme

İçe aktardığı paftadaki ada ve parsel numaralarını düzelten, plan notunun
yüksekliğini paftaya uyduran ya da yanlış yazılmış bir lejant açıklamasını
değiştiren herkes için; bu sayfayı bitirdiğinizde çizimdeki bir yazının metnini,
yüksekliğini ve hizalamasını yerinde değiştirmeyi bileceksiniz.

## Ne yapar

`YAZIDÜZENLE`, çizimde **zaten duran** bir yazının metnini, yüksekliğini ya da
hizalamasını değiştirir. Yeni yazı **çizmez** — onu [`METİN`](text.md) yapar.

İkisinin ayrı komut olması bir tercihtir: `METİN` bir yazıyı **çizer** ve nereye
çizeceğini bilmek için bir nokta ister; `YAZIDÜZENLE` var olan yazıların
**sözünü** değiştirir ve hiçbir noktaya ihtiyaç duymaz. Tek komuta katlansalardı,
bazen çizen bazen çizmeyen — hangisi olduğuna bir argümanın varlığı karar veren —
bir komut olurdu.

**Verilmeyen hiçbir şey değişmez.** Yalnız `yazi` verirseniz yükseklik ve hizalama
olduğu gibi kalır: bir parsel numarasını düzeltmek, plancının seçtiği yüksekliği
sessizce sıfırlamamalıdır.

Yazı taşımayan nesneler **atlanır**, yazıya çevrilmez. Seçimde hiç yazı yoksa komut
bunu söyler.

## Adlar

| Ad | Tür |
|---|---|
| `YAZIDÜZENLE` | Türkçe, birincil |
| `YAZIDUZENLE` | ASCII karşılık |
| `EDITTEXT` | İngilizce karşılık |
| `YZD` | Kısaltma |
| `core.edittext` | Komut kimliği |

## Sözdizimi

```text
YAZIDÜZENLE yazi=<yazı>
YAZIDÜZENLE yukseklik=<mm>
YAZIDÜZENLE nesneler=<kimlik> yazi=<yazı> yukseklik=<mm> hizalama=<hiza>
```

`nesneler` verilmezse **seçim** kullanılır. İçinde boşluk olan yazı tırnak içine
alınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Düzenlenecek yazıların kimlikleri. Verilmezse seçimdeki nesneler |
| `yazi` | Yeni metin. Verilmezse metin değişmez |
| `yukseklik` | Yeni yükseklik, **zeminde milimetre**. Verilmezse yükseklik değişmez |
| `hizalama` | `sol`, `orta`, `sag` ya da `merkez`. Verilmezse hizalama değişmez |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Yükseklik [`METİN`](text.md) ile aynı birimdedir ve aynı şeyi ölçer: **büyük harfin
boyu**. `4000` yazarsanız `A` harfi zeminde 4 m olur.

## Örnekler

### Komut satırı

Bir yazıyı seçip metnini düzeltin:

```
METİN noktalar=485320,4310220 yazi="PARSEL 12" yukseklik=3000
SEÇ mod=NOKTA noktalar=485328,4310221.5 tolerans=0.2
YAZIDÜZENLE yazi="ADA 128"
```

Yalnız yüksekliği büyütün — metin olduğu gibi kalır:

```
YAZIDÜZENLE yukseklik=4000
```

Kimliğiyle, seçim yapmadan:

```
YAZIDÜZENLE nesneler=1 yazi="ADA 128/12" hizalama=merkez
```

### Arayüz

Yazıya **tıklayın** — harflerin üstüne, taban çizgisine değil; yazı harflerinin
çevresinden tutulur. **Öznitelikler** panelindeki **METİN** bölümünde `icerik` ve
`yukseklik` satırları düzenlenebilir: hücreye yeni değeri yazıp **Enter**'a basın.

Panel hücresi belgeye dokunmaz; tam da yukarıdaki komut satırını kurup veri
yoluna verir. Yani panelden düzenlemekle komut satırına yazmak aynı yazmadır, aynı
günlüğe düşer ve tek adımda geri alınır.

### Betik

```json
{
  "ad": "Ada numarasını düzelt",
  "komutlar": [
    { "cmd": "core.select", "args": { "mod": "NESNE", "nesneler": [1] } },
    { "cmd": "core.edittext", "args": { "yazi": "ADA 128", "yukseklik": 4000 } }
  ]
}
```

## Geri alma

Tek adım. Bir çağrıda kaç yazı değiştiyse **hepsi birlikte** geri alınır: metin de,
yükseklik de, hizalama da eski hâline döner.

Bir yazı reddedilirse **hiçbiri** yazılmaz — işlem bütün olarak geri sarılır ve
çizim komut çalışmadan önceki hâlinde kalır.

## Betikten kullanım

Komut kimliği `core.edittext`. Parametreler yukarıdaki tabloyla aynıdır; `nesneler`
bir kimlik listesidir.

Yapay zekâ da bu komutu çağırabilir (`AiAccessible`), ama her öneri gibi
**önizlenir ve onaylanır**; onaysız hiçbir yazı değişmez.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Düzenlenecek yazı yok. Bir yazı seçin ya da nesneler= ile verin.` | Ne seçim var ne `nesneler` | Yazıya tıklayın ya da `nesneler=` yazın |
| `Değiştirilecek bir şey verilmedi: yazi=, yukseklik= ya da hizalama=.` | Komut argümansız çağrıldı | Değiştirmek istediğiniz alanı yazın |
| `Seçimde yazı taşıyan nesne yok.` | Seçimdekilerin hiçbiri yazı değil | Bir yazı seçin |
| `Boş bir yazı bir yazı değildir; silmek için SİL kullanın.` | `yazi=""` verildi | Silmek istiyorsanız [SİL](erase.md) kullanın |
| `Yazı yüksekliği sıfırdan büyük olmalı.` | `yukseklik` sıfır ya da negatif | Milimetre cinsinden pozitif bir değer yazın, örnek `3000` |
| `Nesne bulunamadı veya silinmiş: <kimlik>` | `nesneler` içinde olmayan bir kimlik | Kimliği öznitelik tablosundan doğrulayın |

## İlgili

- [METİN](text.md) — yeni yazı çizer
- [SEÇ](select.md) — hangi yazının düzenleneceğini seçer
- [SİL](erase.md) — yazıyı çizimden kaldırır
