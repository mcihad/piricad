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

`PAH` yazın, nesneyi verin, köşeyi tıklayın, mesafeyi yazın.

### Betik

```json
{
  "komutlar": [
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
| `Nesne N bir eğri ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.` | Daire, yay ya da nokta verildi | Çizgi ya da alan seçin |
| `Burada iki kenarın buluştuğu bir köşe yok. ...` | Açık bir çizginin ucu gösterildi | İki kenarın buluştuğu bir köşe gösterin |
| `Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.` | Kenarlar doğrusal | Gerçek bir köşe gösterin |
| `Kesim komşu kenardan uzun: kenarlar A m ve B m, gereken C m.` | Değer kenarlardan büyük | Daha küçük bir değer verin |

## İlgili

- [`YUVARLA`](fillet.md) — köşeyi düz kenar yerine yayla keser
- [`KÖŞETAŞI`](vertex_move.md) · [`KÖŞEEKLE`](vertex_insert.md)
