# STİLKOPYALA — Stili Başka Nesneye Uygulama

Bir nesneyi yanındakine benzetmesi gereken herkes için; bu sayfayı bitirdiğinizde
stil kopyalamayı komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

`STİLKOPYALA`, bir **kaynak** nesnenin görünümünü seçili nesnelere uygular.

Kopyalanan şey **etkin görünümdür**, stil sütununun ham değeri değil. Bu ayrım
önemlidir: kaynak nesne katmanından miras alıyorsa stil sütununda "katmanından
al" işareti durur, ve bu işareti **başka bir katmandaki** nesneye kopyalamak onu
kaynağa değil kendi katmanına benzetirdi. Komut bunun yerine mirası çözer —
katmanın sembol yığını varsa onu, yoksa katmanın renk ve kalınlıklarını bir stile
dönüştürüp uygular.

Kaynak nesne hedefler arasındaysa atlanır; bir nesnenin stilini kendine
kopyalamak hiçbir şey değiştirmez.

Geometri, katman, öznitelik ve yazı **değişmez**; yalnız görünüm değişir.

## Adlar

| Ad | Tür |
|---|---|
| `STİLKOPYALA` | Türkçe, birincil |
| `STILKOPYALA` | ASCII karşılık |
| `MATCHPROP` | İngilizce karşılık |
| `SK` | Kısaltma |
| `core.match_style` | Komut kimliği |

## Sözdizimi

```text
STİLKOPYALA kaynak=<k>
STİLKOPYALA kaynak=<k> nesneler=<k1> nesneler=<k2>
```

Hedef verilmezse **etkin seçim** kullanılır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `kaynak` | Stili kopyalanacak nesnenin kimliği |
| `nesneler` | Stili alacak nesnelerin kimlikleri. Verilmezse etkin seçim |

## Örnekler

### Komut satırı

```text
STİLKOPYALA kaynak=1 nesneler=2 nesneler=3
```

```text
2 nesne kaynağın stilini aldı.
```

Seçimi kullanarak:

```text
SEÇ
STİLKOPYALA kaynak=1
```

### Arayüz

Kaynağı ve hedefleri **birlikte** seçin, sol araç kutusundaki **Stil Kopyala**
düğmesine basın, sonra stili **kopyalanacak** nesneye tıklayın. Tıkladığınız nesne
kaynaktır; seçimdeki diğerleri onun stilini alır.

Kaynağı komut satırından da verebilirsiniz: `STİLKOPYALA kaynak=<kimlik>`. O zaman
tıklama sorulmaz.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.match_style", "args": { "kaynak": [1], "nesneler": [2, 3] } }
  ]
}
```

## Geri alma

`STİLKOPYALA` tek bir geri alma adımıdır.

## Betikten kullanım

Betikten çağrıldığında `kaynak` ve `nesneler` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Stili kopyalanacak kaynak nesne belirtilmedi. ...` | `kaynak` verilmedi | Kaynak nesnenin kimliğini yazın |
| `Kaynak nesne bulunamadı veya silinmiş: N` | Kaynak kimliği yok | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`STİL`](style.md) — stili doğrudan tanımlar
- [`KATMANAT`](set_layer.md) — nesneyi başka katmana taşır
