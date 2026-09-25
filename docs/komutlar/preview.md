# ÖNİZLE — Çizime Dokunmadan Ne Yapacağını Görmek

Bir yapay zekâ önerisini uygulamadan önce, uzun bir betiği çalıştırmadan önce ya da bir
komutun neye dokunacağından emin olmak istediğinizde. Bu sayfayı bitirdiğinizde bir komut
satırının, bir betiğin ya da bir önerinin çizimde neyi ekleyip sileceğini, neyin yerini
değiştireceğini ve nerede duracağını, çizim değişmeden göreceksiniz.

## Ne yapar

`ÖNİZLE` verdiğiniz komut satırlarını **gerçekten çalıştırır** — aynı komut, aynı
doğrulama, aynı bağlı nesne güncellemeleri — ve sonra **hepsini bütünüyle geri alır**.
Önizlemeden sonra çizim, sürümü, nesne kimlikleri, geri alma ve yineleme yığınları, komut
günlüğü, seçim ve etkin katman önizlemeden önceki hâlindedir. Bir yapay zekâ önerisi
önizlendikten sonra da uygulanabilir: çizimin sürümü değişmediği için öneri "çizim
değişti" diye reddedilmez.

Ne olacağını sayar: kaç nesnenin ekleneceğini ve silineceğini, kaçının yerinin ya da
biçiminin, kaç yazının metninin, kaç nesnenin öznitelik değerinin değişeceğini. Kaynağını
izleyen nesneler — bir parselin etiketi, bir ölçü — de sayılır. Bir satır başarısız
olacaksa **hangisinin** ve **neden** olacağını söyler; uygulansaydı önerinin ya da betiğin
tamamı geri alınacaktı.

**Çizimin dışına dokunan satırları çalıştırmaz** ve sebebini yazar; sonraki satırlar yine
önizlenir:

| Satır | Sebep |
|---|---|
| Dosya yazan: `DIŞAAKTAR`, `KAYDET`, `FARKLIKAYDET`, `STİLAKTAR`, `NOKTALAR … yon=yaz` | dosya yazar |
| Makinenin dışına yazan: yazdırma, veritabanına yazma | makinenin dışına yazar |
| Uygulama ya da oturum ayarı: `TERCİH`, `MOD` | uygulama ya da oturum ayarını değiştirir |
| Görünüm: `YAKINLAŞ`, `KAYDIR` | görünümü değiştirir |
| Başka bir çizim: `AÇ`, `YENİ` | çizimin yerine başkasını koyar |
| Geri alma yığını: `GERİAL`, `YİNELE` | çizimi işlemin dışında değiştirir |

Proje ayarı (`AYAR`) önizlenir: önizlemeden sonra eski değerine döner.

## Adlar

| Ad | Tür |
|---|---|
| `ÖNİZLE` | Türkçe ana ad |
| `ONIZLE` | ASCII |
| `PREVIEW` | İngilizce |
| `ÖNZ` | Kısaltma |

## Sözdizimi

```text
ÖNİZLE komut="<komut satırı>" [komut="<komut satırı>" …] [taslaklar=evet]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `komut` | Önizlenecek komut satırı. Birden çok satır için yineleyin; sırayla, tek iş olarak önizlenir. Verilmezse sorulur |
| `taslaklar` | `evet` ise yapılandırılmış cevap, oluşacak ve değişecek nesnelerin taslaklarını (noktalarını) da taşır. Varsayılan `hayır` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bir parsel ve bir çizgi; parselin 5 m tamponu ile çizginin silinmesi önizleniyor:

```
ALAN 0,0 20,0 20,10 0,10
ÇİZGİ 0,30 20,30
ÖNİZLE komut="TAMPON nesneler=1 mesafe=5 katman=BANT" komut="SİL nesneler=2"
```

```text
Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek; 1 nesne silinecek. Çizim değişmedi.
```

Çizimde hâlâ yalnız parsel ve çizgi vardır; `BANT` katmanı da açılmamıştır.

Duracak bir iş:

```
ÖNİZLE komut="ÇİZGİ 0,5 10,5" komut="SİL nesneler=99"
```

```text
Önizleme: 2. adımda duracak — Nesne bulunamadı veya zaten silinmiş: 99. Uygulanırsa bütünüyle geri alınacak; çizim değişmedi.
```

Dosya yazan bir satır çalıştırılmaz, ötekiler önizlenir:

```
ÖNİZLE komut="DIŞAAKTAR teslim/ada.dxf" komut="ÇİZGİ 0,5 10,5"
```

```text
Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek. Çizim değişmedi.
  1. adım (core.export) önizlemede çalıştırılmadı: dosya yazar.
```

### Arayüz

**Bir yapay zekâ önerisi** kartına gelir gelmez önizlenir: kartta komut satırlarının
altında vurgu renginde **"Uygulanırsa: …"** satırı durur, tuvalde de sonuç **kesikli**
çizilir — oluşacak ve değişecek nesneler mavi, silinecekler kırmızı. Öneri duracaksa
satır kırmızıdır ve hangi adımda neden duracağını söyler. Önerinin hiçbir çizgisi çizimde
değildir; **Uygula** ya da **Reddet**'e basınca taslaklar kalkar.

**KentOS CAD ▸ Betiği Önizle…** bir JSON betiği seçtirir ve çalıştırmadan ne
değiştireceğini komut satırına yazar (`BETİK … onizle=evet`).

Herhangi bir komutu önizlemek için komut satırına `ÖNİZLE` yazın.

### Betik

Bir betiğin tamamını önizlemek için [`BETİK`](script.md)'i `onizle=evet` ile çalıştırın:

```text
BETİK teslim/kaydirma.json onizle=evet
```

Bir betiğin **içinde** `ÖNİZLE` de çalışır: betiğin o ana kadarki işi durur, önizlenen
satırlar geri alınır, betik kaldığı yerden sürer. Betik yine tek geri alma adımıdır.

Python'dan: `cad.preview(commands=["TAMPON nesneler=1 mesafe=5"])`.

### Üçü de aynı

Komut satırı, betik ve öneri kartı aynı önizlemeyi kullanır: aynı satırlar aynı sayıları
verir ve hiçbiri çizimde, günlükte ya da geri alma yığınında iz bırakmaz.

## Geri alma

`ÖNİZLE` hiçbir şey değiştirmez: geri alma adımı bırakmaz ve günlüğe yazılmaz. Önizlenen
satırların eklediği katmanlar, sütunlar ve nesne kimlikleri de geri alınır; önizlemeden
sonra çizilen ilk nesne, önizleme hiç yapılmamış gibi kimlik alır.

## Betikten kullanım

`ÖNİZLE` betiklenebilir ve **yapay zekâya açıktır**: çizime dokunmadığı için bir okuma
aracı gibi hemen çalışır. Bir ajan için yalnız ajana açık komutlar önizlenir; ötekiler
"yapay zekâya kapalı bir komut" diye çalıştırılmaz.

Yapılandırılmış cevap: `adim` ve `calisan` (kaç satır istendi, kaçı çalıştı),
`degisiklik` (sayılar: `eklenen`, `silinen`, `yeri_bicimi_degisen`, `metni_degisen`,
`degeri_degisen`, `katmani_degisen`, `gorunusu_degisen`, `bagi_degisen`,
`ayari_degisen_katman`, `bloklar`, `yerlesimler`, `kilavuzlar`, `koordinat_sistemi`),
`degisiklik_ozeti` (Türkçe cümle), duracaksa `duracagi_adim` ve `hata`,
`calistirilmayan` (`adim`, `komut`, `neden`), `silinecek` (silinecek nesnelerin
kimlikleri) ve `taslaklar=evet` ile `taslaklar` (her biri için `yeni`, değişecekse
`nesne`, `katman`, varsa `yazi` ve `halkalar`: `kapali` ve `noktalar`). Bir önizleme en çok
2000 nesnenin taslağını verir; ötesi sayılara katılır ve `cizilmeyen_taslak` ile söylenir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Önizlenecek satır okunamadı (…): …` | Satır komut satırı gibi okunamadı | Satırı komut satırında deneyin; mesaj neyin yanlış olduğunu söyler |
| `Önizleme başka bir önizlemenin içinde yapılmaz.` | Önizlenen satırlardan biri yine `ÖNİZLE` | İç içe önizleme yok; satırları tek `ÖNİZLE`'de verin |
| `Önizleme: N. adımda duracak — …` | Satırlardan biri uygulansaydı başarısız olacaktı | Hata değildir; mesajın sonundaki sebebi giderin |

## İlgili

- [BETİK](script.md) — `onizle=evet` ile bir betiğin önizlemesi
- [Yapay zekâ önerileri ve onay](../yapay-zeka/onay.md) — kartta önizleme
- [Betik ne değiştirdiğini söyler](../betik/README.md#betik-ne-değiştirdiğini-söyler)
