# PAH — Köşe Pahı Kırma

Kavşakta köşesi kesilmiş bir yapı adası, bir bordür dönüşü ya da kırılmış bir
parsel köşesi çizen herkes için; bu sayfayı bitirdiğinizde pah kırmayı arayüzden,
komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

`PAH`, bir köşeyi **düz bir kenarla** keser. Köşe noktası, her iki kenar boyunca
verdiğiniz **mesafe** kadar içeride duran iki noktayla değiştirilir.

Nesne aynı nesne olarak kalır; kimliği, katmanı, stili ve öznitelikleri değişmez.
Yeni nesne oluşmaz — kesilen köşenin yerine bir kenar gelir, o kadar.

**Kapalı bir alanın her köşesi köşedir**, kapanış kenarının vardığı köşe dahil.
Açık bir çizginin **ilk ve son noktası ise köşe değildir**: orada tek kenar
buluşur ve karşıdan kesilecek bir şey yoktur. Komut bunu söyler.

Kesim mesafesi komşu kenarlardan **kısa olmalıdır**; uzun olsaydı kenar ortadan
kalkardı ve komut bunu yapmak yerine söyler.

**Hangi köşe?** Nesne verilmemişse ve tek bir nesne seçili değilse komut önce
köşeyi sorar: köşeye **bir kez tıklamak** hem nesneyi hem köşeyi seçer. Ardından
mesafe sorulur; mesafeyi **yazabilir** ya da tuvalde **gösterebilirsiniz** —
köşeden imlece olan uzaklık mesafedir. İmleç hareket ettikçe kesilmiş köşe
tuvalde vurgulu çizilir; gördüğünüz, tıkladığınızda olacak olandır.

Hesap `atan2` ya da başka bir trigonometri çağrısı kullanmaz — yalnız birim
vektörler ve `sqrt`. Sebebi taşınabilirliktir: libm'in trigonometri işlevleri
platformlar arası bit bit aynı sonucu vermez, KentOSCad ise verir (§7.3).

## Adlar

| Ad | Tür |
|---|---|
| `PAH` | Türkçe, birincil |
| `CHAMFER` | İngilizce karşılık |
| `PH` | Kısaltma |
| `core.chamfer` | Komut kimliği |

## Sözdizimi

```text
PAH nesne=<k> nokta=<n> mesafe=<metre>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi kesilecek nesnenin kimliği |
| `nokta` | İşlem yapılacak köşe. En yakın köşe seçilir |
| `mesafe` | Köşeden her iki kenar boyunca kesilecek mesafe, metre |

## Örnekler

### Komut satırı

```text
PAH nesne=1 nokta=485300,4310200 mesafe=5
```

```text
Köşeye pah kırıldı.
```

### Arayüz

Sol araç sütununda **köşe ailesinin** düğmesine basın (Pah, Yuvarla, Köşe Taşı,
Köşe Ekle ve Çizgi Düzenle aynı düğmededir; basılı tutunca ya da sağ tıklayınca
kart açılır). Aynı araç **Değiştir → Pah** menüsündedir.

1. Kesilecek köşeye tıklayın. Nesne de bu tıklamayla seçilir.
2. İmleci köşeden uzaklaştırın: kesilmiş köşe tuvalde vurgulu çizilir, yanında
   uzaklık yazar.
3. İstediğiniz yerde tıklayın **ya da** mesafeyi komut satırına yazıp Enter'a
   basın (`5`).

Araç açık kalır; bir sonraki köşeye tıklayarak devam edebilirsiniz, Esc bırakır.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.polyline",
      "args": { "noktalar": [[485300000, 4310260000], [485300000, 4310200000],
                             [485360000, 4310200000]] } },
    { "cmd": "core.chamfer",
      "args": { "nesne": [1], "nokta": [485300000, 4310200000], "mesafe": 5 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`PAH` tek bir geri alma adımıdır; [`GERİAL`](undo.md) köşeyi geri getirir.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `nokta` ve `mesafe` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `PAH için nesne belirtilmedi. Örnek: PAH nesne=1 nokta=10,10 mesafe=3` | Betik ne nesneyi ne köşeyi verdi | `nesne=` ve `nokta=` verin |
| `Orada köşesi kesilecek bir çizgi ya da alan yok. ...` | Tıklanan yerde nesne yok | Bir çizginin iki kenarının buluştuğu köşeye tıklayın |
| `Nesne N bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.` | Daire, yay, yazı ya da nokta verildi | Çizgi ya da alan seçin |
| `Burada iki kenarın buluştuğu bir köşe yok. ...` | Açık bir çizginin ucu gösterildi | İki kenarın buluştuğu bir köşe gösterin |
| `Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.` | Kenarlar doğrusal | Gerçek bir köşe gösterin |
| `Mesafe sıfırdan büyük olmalı.` | Sıfır ya da eksi mesafe | Artı bir mesafe verin |
| `Kesim komşu kenardan uzun: kenarlar 12,000 m ve 20,000 m, gereken 21,000 m. ...` | Değer kenarlardan büyük | Daha küçük bir değer verin ya da daha yakına tıklayın |

## İlgili

- [`YUVARLA`](fillet.md) — köşeyi düz kenar yerine yayla keser
- [`KÖŞETAŞI`](vertex_move.md) · [`KÖŞEEKLE`](vertex_insert.md)
