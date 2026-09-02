# KATMANAT — Nesneyi Başka Katmana Taşıma

Yanlış katmana çizilmiş bir nesneyi yerine koyması gereken herkes için; bu sayfayı
bitirdiğinizde katman değiştirmeyi arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`KATMANAT`, seçili nesneleri **başka bir katmana** taşır.

Nesne **aynı nesne olarak kalır**: kimliği, geometrisi, öznitelikleri ve yazısı
değişmez. Bir parseli `PARSEL_TASLAK`'tan `PARSEL`'e taşımak ada/parsel satırını
düşürmez — kimlik nesnenin kendisidir, katmanı değil.

Hedef katman yoksa **oluşturulur**. Yeni bir katmana çizim yapmak onu nasıl
oluşturuyorsa bu da öyle: önce [`KATMAN`](layer.md) çalıştırmayı zorunlu kılmak,
çizimin umursamadığı bir adım eklemek olurdu.

Nesne **gizli** bir katmana taşınırsa görünmez olur, **kilitli** bir katmana ise
taşınamaz — komut bunu söyler ve hiçbir şey değiştirmez.

## Adlar

| Ad | Tür |
|---|---|
| `KATMANAT` | Türkçe, birincil |
| `KATMANATA` | Türkçe eşanlamlı |
| `SETLAYER` | İngilizce karşılık |
| `KA` | Kısaltma |
| `core.set_layer` | Komut kimliği |

## Sözdizimi

```text
KATMANAT katman=<ad>
KATMANAT nesneler=<k1> nesneler=<k2> katman=<ad>
```

Kimlik verilmezse **etkin seçim** taşınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Taşınacak nesnelerin kimlikleri. Verilmezse etkin seçim |
| `katman` | Hedef katmanın adı. Yoksa oluşturulur |

## Örnekler

### Komut satırı

```text
SEÇ
KATMANAT katman=PARSEL
```

```text
3 nesne 'PARSEL' katmanına taşındı.
```

### Arayüz

Nesneleri seçin ve komut satırına `KATMANAT katman=PARSEL` yazın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.set_layer", "args": { "nesneler": [1, 2], "katman": "PARSEL" } }
  ]
}
```

## Geri alma

`KATMANAT` tek bir geri alma adımıdır; [`GERİAL`](undo.md) nesneleri eski
katmanlarına döndürür.

## Betikten kullanım

Betikten çağrıldığında `nesneler` ve `katman` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `'<katman>' katmanı kilitli.` | Hedef katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |
| `Katman adı boş olamaz.` | Boş ad verildi | Bir katman adı yazın |

## İlgili

- [`KATMAN`](layer.md) — katman oluşturma, gizleme, kilitleme
- [`STİLKOPYALA`](match_style.md) — stili bir nesneden diğerine taşır
- [`SEÇ`](select.md)
