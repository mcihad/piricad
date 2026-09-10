# ALANÖLÇ — Alan ve Çevre Ölçme

Bir parselin yüzölçümünü ya da çevresini okumak isteyen herkes için; bu sayfayı
bitirdiğinizde alan ölçümünü arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`ALANÖLÇ`, seçili nesnelerin **alanını** ve **çevresini** transkripte yazar. Birden
çok nesne verildiğinde her birini ayrı ayrı yazar ve sonunda **toplam alanı**
bildirir.

Alan hesabı **nesnenin türüne sorulur**. Bu, dairede fark eder: daire gerçek
alanını, yani **π·r²**'yi bildirir — ekranda çizildiği 128 kenarlı çokgenin alanını
değil. Yay ve açık çizgi hiçbir şey çevrelemediği için alanları sıfırdır.

Alan **halkalar üzerinden, role göre işaretli** hesaplanır: dış sınır artı,
delikler eksi. Delikli bir parselin alanı deliksiz gösterilmez.

`ALANÖLÇ` çizimi **değiştirmez** ve geri alma adımı üretmez.

## Adlar

| Ad | Tür |
|---|---|
| `ALANÖLÇ` | Türkçe, birincil |
| `ALANOLC` | ASCII karşılık |
| `AREAOF` | İngilizce karşılık |
| `AÖ` | Kısaltma |
| `core.measure_area` | Komut kimliği |

## Sözdizimi

```text
ALANÖLÇ
ALANÖLÇ nesneler=<k1> nesneler=<k2> …
```

Kimlik verilmezse **etkin seçim** ölçülür; o da boşsa arayüz nesneleri sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ölçülecek nesnelerin kimlikleri. Verilmezse etkin seçim; o da boşsa tuvalden seçtirir |

## Örnekler

### Komut satırı

```text
SEÇ
ALANÖLÇ
```

```text
Nesne 1 — alan: 2700,00 m²   çevre: 210,000 m
```

Birden çok parselin toplamı:

```text
ALANÖLÇ nesneler=1 nesneler=2 nesneler=3
```

### Arayüz

Araç kutusunda **Uzunluk Ölç** ailesinden **Alan Ölç**'e basın. Seçili nesne varsa
hemen ölçülür; yoksa komut "Ölçülecek nesneleri seçin" der, tuvalden tıkladığınız
her nesne seçime eklenir ve **sağ tık** (ya da Enter) ölçtürür. Sonuç transkripte
düşer ve araç elinizde kalır: seçim temizlenir, sıradaki parsel için yeniden sorar.
Bırakmak için Esc.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.measure_area", "args": { "nesneler": [1, 2] } }
  ]
}
```

## Geri alma

`ALANÖLÇ` geri alınmaz, çünkü hiçbir şeyi değiştirmez.

## Betikten kullanım

Betikten çağrıldığında `nesneler` verilmelidir; betik çalışırken "etkin seçim"
diye bir şey olmayabilir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.` | Ne kimlik verildi ne seçim var, tuvalde de seçilmedi | Nesneleri seçin ya da kimliklerini yazın; örnek satır mesajın altındadır |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`ÖLÇ`](measure.md) — iki nokta arası mesafe
- [`ÖZNİTELİK`](attribute.md) — alanı özniteliğe yazmak için
- [`SEÇ`](select.md)
