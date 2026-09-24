# ÖLÇ — Mesafe Ölçme

İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı — ya da bir sınırın,
bir güzergâhın kenar kenar uzunluğunu — okumak isteyen herkes için; bu sayfayı
bitirdiğinizde ölçümü arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`ÖLÇ`, verdiğiniz iki nokta arasındaki **mesafeyi**, **koordinat farkını**
(ΔY, ΔX) ve **açıyı** yazar. İkinci noktadan sonra **durmaz**: verdiğiniz her yeni
nokta bir kenar daha ekler, o kenarın uzunluğunu, açısını ve **toplam** uzunluğu
yazar. **Enter** ölçümü bitirir; birden çok kenar ölçtüyseniz son satır toplamı ve
kenar sayısını verir.

**Sonuç tuvalde kalır.** Ölçülen hat, her kenarın üstünde uzunluğu ve son noktanın
yanında toplamı ile vurgulu çizilir. Çizim değiştiğinde ya da hiçbir komut
çalışmıyorken **Esc**'e bastığınızda silinir; çizimin bir parçası değildir, kaydedilmez.

Açı, varsayılan olarak **semt açısıdır**: kuzeyden saat yönünde, **grad** cinsinden —
Türkiye'deki her ölçü krokisinin ve her aletin kullandığı yön ve birim budur. Birimi
`açı_birimi` proje ayarı, yönü `açı_kuralı` oturum modu belirler; `MOD kural matematik`
yazılmışsa açı doğudan saat yönünün tersine yazılır. Satır hangi kuralla yazıldığını
parantez içinde söyler. Komut satırına yazdığınız `@mesafe<açı` ile aynı iki ayardır
(bkz. [Komut satırı](komut-satiri.md)).

`ÖLÇ` çizimi **değiştirmez**. Hiçbir şey yazmaz, geri alma adımı üretmez ve komut
günlüğüne düzenleme olarak düşmez: soru soran bir komutun Ctrl+Z ile geri alınacak
bir şeyi olmamalıdır.

Sayılar **belgeden** gelir, ekrandan değil. Uzunluk saklanan milimetreler üzerinden
hesaplanır, dolayısıyla okuduğunuz değer dışa aktarmanın yazacağı değerdir — piksel
konumlarından alınan bir ölçüm o anki yakınlaştırma kadar yanılırdı.

## Adlar

| Ad | Tür |
|---|---|
| `ÖLÇ` | Türkçe, birincil |
| `OLC` | ASCII karşılık |
| `MEASURE` | İngilizce karşılık |
| `MS` | Kısaltma |
| `core.measure` | Komut kimliği |

## Sözdizimi

```text
ÖLÇ
ÖLÇ <n1> <n2>
ÖLÇ <n1> <n2> <n3> …
ÖLÇ baslangic=<n> bitis=<n> [devam=<n> …]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `baslangic` | Ölçümün ilk noktası |
| `bitis` | Ölçümün ikinci noktası |
| `devam` | Sonraki noktalar; her biri bir kenar daha ekler. Verilmezse ölçüm iki noktada biter (arayüzde Enter'a kadar sorulur) |

## Örnekler

### Komut satırı

```text
ÖLÇ baslangic=485300,4310200 bitis=485330,4310240
```

```text
Mesafe: 50,000 m   ΔY: 30,000 m   ΔX: 40,000 m   Açı: 40,9666 grad (kuzeyden saat yönünde)
```

Bir sınırı kenar kenar ölçmek için noktaları sürdürün:

```text
ÖLÇ 485300,4310200 485330,4310240 devam=485330,4310260
```

```text
Mesafe: 50,000 m   ΔY: 30,000 m   ΔX: 40,000 m   Açı: 40,9666 grad (kuzeyden saat yönünde)
Kenar 2: 20,000 m   Açı: 0,0000 grad   Toplam: 70,000 m
Toplam uzunluk: 70,000 m   (2 kenar)
```

### Arayüz

Şeritte **Harita ▸ Ölçüm ▸ Ölç**'e basın (ya da `ÖLÇ` yazın) ve noktaları sırayla
tıklayın. İmleç hareket ettikçe ölçülen hat ve imlece giden kenar çizilir; biten her
kenarın üstünde uzunluğu, imlecin yanında o kenarın uzunluğu, açısı ve **toplam**
yazar. Yakalama açıkken noktalar mevcut köşelere oturur, yani parsel köşeleri
arasındaki gerçek mesafeyi okursunuz.

Bitirmek için **Enter**'a basın ya da **sağ tıklayın**. Sonuç tuvalde kalır ve araç
bir sonraki ölçüm için hazır bekler; birkaç ölçüm yan yana durabilir. Esc aracı
bırakır, hiçbir komut çalışmıyorken ikinci bir Esc ölçüm işaretlerini siler.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.measure",
      "args": { "baslangic": [485300000, 4310200000],
                "bitis": [485330000, 4310240000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`ÖLÇ` geri alınmaz, çünkü hiçbir şeyi değiştirmez. [`GERİAL`](undo.md) ondan
önceki düzenlemeye gider.

## Betikten kullanım

Betikten çağrıldığında `baslangic` ve `bitis` verilmelidir; `devam` isteğe
bağlıdır. Sonuç transkripte yazılır. Yapılandırılmış sonuç (`kenarlar_mm`,
`toplam_mm`) bir ajana ve Python'a da döner.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'baslangic' parametresi nokta bekliyor` | Nokta olmayan bir değer verildi | Koordinat yazın: `485300,4310200` |

## İlgili

- [`ALANÖLÇ`](measure_area.md) — alan ve çevre ölçer
- [`MOD`](mode.md) — yakalama modları
