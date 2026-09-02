# KOPYALA — Nesne Çoğaltma

Aynı yapıyı, aynı direği ya da aynı parsel şablonunu tekrar tekrar yerleştiren
herkes için; bu sayfayı bitirdiğinizde seçili nesneleri arayüzden, komut
satırından ve betikten kopyalamayı bileceksiniz.

## Ne yapar

`KOPYALA`, seçili nesnelerin **kopyasını** iki nokta arasındaki kadar öteye
koyar. Özgün nesneler yerinde kalır.

Kopyayla birlikte **stil, yazı ve bütün öznitelikler** de gider. Ada numarasını
kaybeden bir kopya, koruyan bir kopyadan daha kötüdür: tek bir numarayı düzeltmek,
hepsini yeniden yazmaktan küçük bir iştir.

Kopya **yeni bir kimlik** alır. Bu kasıtlıdır ve model kuralıdır: kimlik nesnenin
kendisidir, iki nesne aynı kimliği taşıyamaz. Pratikte şu demektir: kopyalanan bir
parselin ada/parsel numarası kopyaya da geçer ve **onu siz düzeltmelisiniz** —
program hangi numaranın doğru olduğunu bilemez.

## Adlar

| Ad | Tür |
|---|---|
| `KOPYALA` | Türkçe, birincil |
| `COPY` | İngilizce karşılık |
| `KP` | Kısaltma |
| `core.copy` | Komut kimliği |

## Sözdizimi

```text
KOPYALA
KOPYALA nesneler=<k1> nesneler=<k2> …
KOPYALA nesneler=<k> baslangic=<n> bitis=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Kopyalanacak nesnelerin kimlikleri. Verilmezse etkin seçim |
| `baslangic` | Kopyalamanın başlangıç noktası |
| `bitis` | Kopyanın geleceği nokta |

## Örnekler

### Komut satırı

```text
SEÇ
KOPYALA baslangic=0,0 bitis=50,0
```

Göreli koordinatla, tam 25 metre doğuya:

```text
KOPYALA nesneler=1 baslangic=0,0 bitis=@25,0
```

### Arayüz

Nesneleri seçin, `KOPYALA` yazın, iki noktayı tıklayın. Yakalama kopyanın
nereye oturacağını belirler.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.copy",
      "args": { "nesneler": [1],
                "baslangic": [0, 0], "bitis": [50000, 0] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`KOPYALA` tek bir geri alma adımıdır: [`GERİAL`](undo.md) bütün kopyaları birden
kaldırır.

## Betikten kullanım

Betikten çağrıldığında `nesneler`, `baslangic` ve `bitis` verilmelidir.
Kopyalanan özniteliklerin — özellikle ada/parsel numaralarının — düzeltilmesi
betiğin sorumluluğundadır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`TAŞI`](move.md) — çoğaltmadan taşır
- [`DÖNDÜR`](rotate.md) · [`ÖLÇEKLE`](scale.md) · [`AYNALA`](mirror.md)
- [`ÖZNİTELİK`](attribute.md) — kopyanın numarasını düzeltir
