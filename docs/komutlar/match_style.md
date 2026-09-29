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
| `BİÇİMBOYA` | Türkçe eş ad: Netcad'deki adı (Biçim Boya) |
| `BICIMBOYA` | ASCII karşılık |
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

Şeritteki **Giriş ▸ Değiştir ▸ Stil Kopyala** simgesi (bir nesne seçiliyken beliren
**Yazı**, **Alan** ve **Çizgi** sekmelerinde de vardır) Netcad'in Biçim Boya'sı gibi
çalışır — önce kaynak, sonra hedefler:

1. **Hiçbir şey seçmeden** basın. Komut satırı `Stili kopyalanacak KAYNAK nesneye
   tıklayın` der; kaynağa **bir kez** tıklayın — Enter gerekmez.
2. `Stili alacak nesneleri seçin, sonra Enter` sorusunda hedefleri tıklayın ya da
   kutuyla seçin, **Enter**'a ya da sağ tuşa basın.

**Bir nesne seçiliyken** basarsanız seçili nesne kaynaktır ve yalnız hedefler sorulur.

Kaynağı ve hedefleri **birlikte** seçip basarsanız eski yol geçerlidir: komut stili
**kopyalanacak** nesneye tıklatır; tıkladığınız kaynaktır, seçimdeki diğerleri onun
stilini alır.

Kaynağı komut satırından da verebilirsiniz: `STİLKOPYALA kaynak=<kimlik>`. O zaman
kaynak sorulmaz. Vazgeçmek için Esc.

### Betik

Betik önce üç çizgi çizer, sonra birincinin stilini öbür ikisine kopyalar:

```json
{
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0,0],[20000,0]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0,5000],[20000,5000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0,10000],[20000,10000]] } },
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
| `'<katman>' katmanı kilitli; üzerindeki nesne düzenlenemez. Kilidi KATMAN ad=<katman> kilitli=hayır ile açın.` | Nesne kilitli bir katmanda: değeri, katmanı, stili ve yazısı da kilitlidir | Kilidi [`KATMAN`](layer.md) ile açın |
| `Bu nesne '<ad>' dış referansının parçası ('<dosya>'); kendi dosyasında düzenlenir ve yenilenince oradan yeniden okunur. …` | Nesne bir [dış referansın](xref.md) içinde; buradaki bir değişiklik bir sonraki yenilemede kaybolurdu | Kaynak dosyada düzenleyin, ya da referansı `DIŞREFERANS islem=bagla ad=<ad>` ile çizime bağlayın |

## İlgili

- [`STİL`](style.md) — stili doğrudan tanımlar
- [`KATMANAT`](set_layer.md) — nesneyi başka katmana taşır
