# PATLAT — Parçalarına Ayır

Bir parselin bir köşesini oynatması gereken ama parselin bir *alan* olduğunu
gören, ya da bir çitin bir ayağını atması gereken ama çitin tek bir *çizgi*
olduğunu gören herkes için.

## Ne yapar

Bir nesneyi çizildiği parçalara ayırır ve kendisini siler:

| Nesne | Ne çıkar |
|---|---|
| Çok köşeli çizgi | Her kenar ayrı bir çizgi |
| Alan | Sınırı ayrı kenarlar hâlinde — **kapanış kenarı dâhil** |
| Blok referansı | Bileşenleri, durdukları yere yerleştirilmiş; dizinin her kopyası için |

## Ne reddeder ve neden

Tanımında **daire, yay ya da yazı** olan bir blok, yarım yerleştirilmek yerine
**adıyla reddedilir**. Sebebi şudur: bu türler bir yük taşır ve aynalı veya eşit
olmayan bir ölçek altında bir daire artık daire, bir yay artık yay değildir.
Çizilmiş dış çizgilerini koymak bir daireyi çevresi 2πr, alanı πr² olmayan bir
128-gen'e çevirir — ve bunlar tapuya giden sayılardır.

Bileşenleri tek tek düzenlemek için Faz 2'nin `BLOKDÜZENLE` komutu gelecek.

## Adlar

| Ad | Tür |
|---|---|
| `PATLAT` | Türkçe, birincil |
| `EXPLODE` | İngilizce karşılık |
| `PTL` | Kısaltma |
| `core.explode` | Komut kimliği |

## Sözdizimi

```text
PATLAT nesne=<kimlik> [nesne=<kimlik> …]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 0..n | Patlatılacak nesneler |

## Örnekler

### Komut satırı

Üç kenarlı bir çizgi üç çizgi olur:

```text
PATLAT nesne=1
```

```text
1 nesne patlatıldı, 3 parça çıktı.
```

Dört köşeli bir alan **dört** kenar verir — kapanış kenarı da bir kenardır:

```text
PATLAT nesne=2
```

```text
1 nesne patlatıldı, 4 parça çıktı.
```

### Arayüz

**Değiştir ▸ Birleştir ▸ Patlat** (aynı düğme **Giriş ▸ Değiştir**'de, **Çizim ▸ Blok**'ta ve
bir blok seçiliyken beliren **Blok** sekmesinde de vardır). Nesneleri seçip Enter'a basın.

### Betik

```json
{ "cmd": "core.explode", "args": { "nesne": [1, 2] } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: geri alma nesneyi bütün hâline döndürür, parçalarından
birini değil.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Blok '…' içinde bir daire var ve bu sürüm onu yerine koyamıyor` | Blok tanımında eğri ya da yazı var | Faz 2'nin `BLOKDÜZENLE`'sini bekleyin |
| `Nesne N bir daire; PATLAT bu sürümde çizgileri, alanları ve blok referanslarını patlatır.` | Doğrudan bir eğri seçildi | Bir eğri zaten parçalarına ayrılmış değildir |
| `Katman kilitli: …` | Aktif katman kilitli | `KATMAN` ile kilidi kaldırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BLOK](block.md) — bileşenlerden blok tanımlar
- [BLOKEKLE](insert.md) — tanımlı bloğu yerleştirir
- [ALANAÇEVİR](to_area.md) — kapalı çizgiyi alana çevirir
