# ÖLÇ — Mesafe Ölçme

İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı okumak isteyen herkes
için; bu sayfayı bitirdiğinizde ölçümü arayüzden, komut satırından ve betikten
yapmayı bileceksiniz.

## Ne yapar

`ÖLÇ`, verdiğiniz iki nokta arasındaki **mesafeyi**, **koordinat farkını**
(ΔY, ΔX) ve **açıyı** transkripte yazar.

Açı, **kuzeyden saat yönünde** verilir — Türkiye'deki her ölçü krokisinin ve her
aletin kullandığı yön budur, altındaki matematiğin doğudan saat yönünün tersine
sayan yönü değil.

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
ÖLÇ baslangic=<n> bitis=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `baslangic` | Ölçümün ilk noktası |
| `bitis` | Ölçümün ikinci noktası |

## Örnekler

### Komut satırı

```text
ÖLÇ baslangic=485300,4310200 bitis=485330,4310240
```

```text
Mesafe: 50,000 m   ΔY: 30,000 m   ΔX: 40,000 m   Açı: 36,869° (kuzeyden saat yönünde)
```

### Arayüz

`ÖLÇ` yazın ve iki noktayı tıklayın. İki tıklama arasında kesikli bir kılavuz
uzanır. Yakalama açıkken uçlar mevcut köşelere oturur, yani iki parsel köşesi
arasındaki gerçek mesafeyi okursunuz.

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

Betikten çağrıldığında `baslangic` ve `bitis` verilmelidir. Sonuç transkripte
yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'baslangic' parametresi nokta bekliyor` | Nokta olmayan bir değer verildi | Koordinat yazın: `485300,4310200` |

## İlgili

- [`ALANÖLÇ`](measure_area.md) — alan ve çevre ölçer
- [`MOD`](mode.md) — yakalama modları
