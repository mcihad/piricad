# ARAÇARA — Araç Kataloğunda Arama

Bir ajanın ya da bir kullanıcının "bu işi yapan komut hangisi" sorusunu sorduğu an için;
bu sayfayı bitirdiğinizde katalogda ada ve özete göre aramayı, sonucun neden her zaman
üç sayı taşıdığını ve tam listeyi nereden alacağınızı bileceksiniz.

## Ne yapar

Yapay zeka **araç kataloğunda** arar. Katalog, `Flags::AiAccessible` taşıyan her komuttan
üretilir ve program büyüdükçe büyür; bir bakışta okunacak boyu çoktan geçti.

Arama, aracın **adında** ve **özetinde** çalışır. Eşleşme Türkçe katlamayla yapılır:
`olcek` yazan `ÖLÇEKLE`'yi bulur, `ı` ile `i` birbirine karışmaz.

> **Arama hiçbir aracı gizlemez.** Bu, bir araç yüzeyinde aramanın tek gerçek tehlikesidir:
> tam görünen ama süzülmüş bir liste, ajanı görmediği araçların var olmadığı sonucuna
> götürür. Bu yüzden her cevap üç sayı taşır — kaç araç **eşleşti**, kaçı **gösterildi**,
> katalogda kaç araç **var** — ve tam listenin `tools/list` ile alındığını söyler.
> Hiçbir eşleşme yoksa bunu sözle söyler; boş bir dizi, cevap gibi okunur.

Arama **çağrı şemasını vermez**. Bir aracı çağırmak için gereken parametre şeması
`tools/list`'tedir; burada ad, başlık, komut kimliği, aracın çizimi değiştirip
değiştirmediği ve özetin ilk satırı vardır.

## Adlar

| Ad | Tür |
|---|---|
| `ARAÇARA` | Türkçe, birincil |
| `ARACARA` | ASCII karşılığı |
| `TOOLSEARCH` | İngilizce karşılık |
| `ARA` | Kısaltma |
| `core.tool_search` | Komut kimliği |

## Sözdizimi

```text
ARAÇARA sorgu=<sözcük>
ARAÇARA sorgu=<sözcük> alan=ad
ARAÇARA sorgu=<sözcük> sinir=<1..200>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `sorgu` | Aranan sözcük. Zorunlu. Ad ve özet içinde Türkçe katlamayla eşleşir |
| `alan` | Nerede aranacağı: `hepsi` (öntanımlı), `ad` ya da `ozet` |
| `sinir` | En çok kaç sonuç **gösterilsin**: 1–200, öntanımlı 20. Eşleşme sayısı her hâlde bildirilir |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
ARAÇARA sorgu=katman
```

```text
4 araç eşleşti (katalog: 63):
  core_layer — KATMAN
  core_layer_visibility — KATMANGÖRÜNÜM
  core_set_layer — KATMANAT
  katmanlari_listele — KATMANLAR
```

Sınır, gösterileni keser ama **sayımı kesmez** — ve kesileni söyler:

```text
ARAÇARA sorgu=çizgi sinir=1
```

```text
8 araç eşleşti, ilk 1 tanesi (katalog: 63):
  core_line — ÇİZGİ
  … 7 tane daha. Sınırı büyütün (sinir=…) ya da tam listeyi `tools/list` ile alın.
```

Eşleşme yoksa cevap sözle verilir:

```text
ARAÇARA sorgu=zzqq
```

```text
'zzqq' için eşleşen araç yok. Kataloğun tamamı 63 araç taşıyor; tam listeyi
`tools/list` ile alın.
```

Yalnız adlarda aramak:

```text
ARAÇARA sorgu=yerleşim alan=ad
```

### Arayüz

Arayüzde bunun karşılığı **Ctrl+K** komut aramasıdır: komut adlarında ve açıklamalarında
aynı Türkçe katlamayla arar, seçtiğinizi komut satırına yerleştirir. `ARAÇARA` aynı işi
bir **ajan** için yapar ve sonucu yapılandırılmış veri olarak döndürür.

### Betik

```json
{ "cmd": "core.tool_search", "args": { "sorgu": "katman", "sinir": 5 } }
```

Yapılandırılmış sonuç şu alanları taşır: `sorgu`, `alan`, `eslesen`, `gosterilen`,
`katalog`, `araclar` ve `aciklama`. Her araç satırı `arac`, `ad`, `komut`, `degistirir`
ve `ozet` taşır.

## Geri alma

`ARAÇARA` hiçbir şeyi değiştirmez: geri alınacak bir şey yoktur ve [`GERİAL`](undo.md)
listesine girmez. Komut `NoEffect` taşır, yani bir ajan onu **onay beklemeden**
çalıştırabilir.

## Betikten kullanım

Komut betiklerde `core.tool_search` kimliğiyle çağrılır. `sinir` betikte de **tam
sayıdır**, metin değil.

Komut **etkileşimlidir**: `sorgu` eksik bırakılırsa sorulur. Bir betikte soracak kimse
olmadığı için `sorgu`'yu her zaman yazın.

Bir ajan için sıra şudur: `ARAÇARA` ile adayı bulun, `tools/list` ile o aracın çağrı
şemasını alın, sonra çağırın. Arama, `tools/list`'in yerine geçmez.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Aranacak bir sözcük gerekir: sorgu=<sözcük>.` | `sorgu` boş verildi | Bir sözcük yazın |
| `'core.tool_search': 'alan' için tanınmayan değer 'ne'. Kabul edilenler: hepsi / ad / ozet` | `alan` üç sözcükten biri değil | Üç sözcükten birini yazın |
| `'core.tool_search': 'sinir' 1 ile 200 arasında olmalı, 0 geldi.` | Sınır aralığın dışında | 1–200 arasında bir sayı yazın |
| `'core.tool_search': 'sinir' parametresi tam sayı bekliyor. Girilen: 'abc'` | Sayı olmayan bir sınır | Tam sayı yazın |

## İlgili

- [`İŞŞABLONU`](job_template.md) — bir işin komut satırları, sırasıyla
- [`YARDIM`](help.md) — komutların insana yönelik listesi
- [komut referansı](referans.md) — üretilmiş tam liste
- [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) — `tools/list` ve araç yüzeyi
