# BAĞIMLILIK — Sonuçlar Güncel mi

Bir kuyunun koruma alanını çizip sonra kuyunun yerini düzelttiğinizde, bir çizgi ağından
parselleri ürettikten sonra bir çizgiyi oynattığınızda ya da kotlu noktalardan eş yükselti
eğrilerini çizip bir kotu yeniden okuduğunuzda. Bu sayfayı bitirdiğinizde hangi sonucun
kaynağıyla artık uyuşmadığını görecek, onu şimdiki hâliyle kabul edecek ya da kaynağından
çözebileceksiniz.

## Ne yapar

Bazı nesneler başka nesneler **hakkında** bir cümledir: [TAMPON](tampon.md)'un çizdiği
koruma alanı bir kuyu hakkında, [ALANÜRET](alan_uret.md)'in ürettiği alan onu kapatan
çizgiler hakkında, [SINIR](boundary.md)'ın bulduğu alan çizgileri hakkında,
[EŞYÜKSELTİ](contour.md)'nin eğrileri kotlu noktalar hakkında. Bunlara **sonuç** denir.
Her sonuç, hesaplandığı anda kaynaklarının ne olduğunu kaydeder.

Bir kaynak sonradan değişince — taşınınca, köşesi oynayınca, bir değeri değişince —
sonuç kendiliğinden yeniden hesaplanmaz; ama **güncel olmadığını söyler**:

- Kaynağı değiştiren komut bittiği anda komut satırına yazılır:
  `Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON).`
- Tuvalde sonucun üstünde uyarı renginde **güncel değil** işareti durur.
- Öznitelik panelinde `koken` satırı **GÜNCEL DEĞİL** rozetini taşır.

BAĞIMLILIK bu durumu çizimin bütünü ya da verdiğiniz nesneler için sayar ve hangi
sonucun hangi kaynağı değiştiği için güncel olmadığını yazar. Güncel olmayan bir sonuç
için iki karar verebilirsiniz:

- **Kabul** (`islem=kabul`): sonuç olduğu gibi doğrudur; kaynaklarının şimdiki hâli
  kaydedilir ve sonuç yeniden güncel sayılır.
- **Çöz** (`islem=coz`): sonuç artık kendi başına bir nesnedir; kökeni geçmiş olarak
  kalır, güncel olup olmadığı bir daha sorulmaz.

Bir sonucun durumu dörtten biridir:

| Durum | Anlamı |
|---|---|
| Güncel | Bütün kaynakları yerinde ve hesaplandığı hâlde |
| Güncel değil | Yerinde duran bir kaynağı o zamandan beri değişti |
| Kaynaksız | Hiçbir şey değişmedi ama bir kaynağı silindi; sonuç kendi başına duruyor |
| Geçmiş | Sonuç değil: bir kopya, bir parça, bir ifraz parseli — ya da çözülmüş bir sonuç |

**Durum saklanmaz, çizimden okunur.** Kuyuyu taşıyıp geri aldığınızda koruma alanı
yeniden güncel olur; kuyuyu ayrı bir komutla eski yerine koyduğunuzda da. Ayrıntı:
[Bağımlılıklar ve sonuçlar](../veri/bagimliliklar.md).

## Adlar

| Ad | Tür |
|---|---|
| `BAĞIMLILIK` | Türkçe ana ad |
| `BAGIMLILIK` | ASCII |
| `DEPENDENCY` | İngilizce |
| `BĞM` | Kısaltma |

## Sözdizimi

```text
BAĞIMLILIK [islem=durum|kabul|coz] [nesneler=<kimlik…>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | `durum` (varsayılan): sonuçların güncel olup olmadığını sayar ve güncel olmayanları yazar. `kabul`: kaynakların şimdiki hâlini kaydeder, sonuç güncel olur. `coz`: sonucu kaynağından çözer, kökeni geçmiş olarak kalır |
| `nesneler` | Sorulacak sonuçlar. Verilmezse `durum` çizimdeki bütün sonuçlara, `kabul` ve `coz` güncel olmayan bütün sonuçlara bakar |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bir kuyu ve onun 5 m koruma alanı; sonra kuyunun yeri 2 m düzeltiliyor:

```
NOKTA 485320,4310220
TAMPON nesneler=1 mesafe=5 katman=KORUMA
TAŞI nesneler=1 baslangic=485320,4310220 bitis=485322,4310220
```

TAŞI bittiğinde komut satırı şunu yazar:

```text
Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON). Hangileri olduğunu görmek için: BAĞIMLILIK
```

Hangi sonuç, neden:

```
BAĞIMLILIK
```

```text
1 sonuç: 0 güncel, 1 güncel değil, 0 kaynaksız.
  güncel değil: nesne 2 (TAMPON) — değişen kaynak: nesne 1
Kaynakların şimdiki hâlini kabul etmek için: BAĞIMLILIK islem=kabul — sonucu kaynağından çözmek için: BAĞIMLILIK islem=coz
```

Koruma alanı olduğu gibi doğruysa kabul edin; artık kuyuya bağlı saymak istemiyorsanız
çözün:

```
BAĞIMLILIK islem=kabul nesneler=2
BAĞIMLILIK islem=coz nesneler=2
```

### Arayüz

Şeritte **Analiz ▸ Denetim ▸ Bağımlılıklar**'a (ya da **Kadastro ▸ Denetim ▸
Bağımlılıklar**'a) basın: komut satırı çizimdeki sonuçları sayar ve güncel olmayanları
yazar. Güncel olmayan bir sonuç tuvalde uyarı renginde
**güncel değil** diye işaretlidir; seçtiğinizde öznitelik panelindeki `koken` satırı
**GÜNCEL DEĞİL** rozetini ve değişen kaynağı gösterir.

### Betik

Bir kuyunun koruma alanını çizmek, kuyuyu taşımak ve koruma alanını kabul etmek:

```json
{"ad": "koruma alanını kabul et", "komutlar": [
  {"cmd": "core.point_draw", "args": {"noktalar": [[485320000, 4310220000]]}},
  {"cmd": "islem.tampon", "args": {"nesneler": [1], "mesafe": 5, "katman": "KORUMA"}},
  {"cmd": "core.move", "args": {"nesneler": [1],
                                "baslangic": [485320000, 4310220000],
                                "bitis": [485322000, 4310220000]}},
  {"cmd": "core.dependency", "args": {"islem": "kabul"}}
]}
```

Python'dan: `cad.dependency()` durumu, `cad.dependency(action="kabul")` kabulü yapar.

### Üçü de aynı

Arayüz, komut satırı ve betik aynı sonuçları kabul eder ya da çözer ve aynı belgeyi
bırakır. Günlüğe kabul edilen ya da çözülen sonuçların kimlikleri yazılır; günlüğü
oynatmak aynı sonuçları kabul eder.

## Geri alma

`durum` çizimi değiştirmez ve geri alma adımı bırakmaz. `kabul` ve `coz` tek adımda
geri alınır (Ctrl+Z ya da `GERİAL`): kabul geri alınınca sonuç yeniden güncel değildir,
çözme geri alınınca yeniden kaynağına bağlıdır. Kabul edilecek bir şey yoksa geri alma
adımı da bırakılmaz.

## Betikten kullanım

`durum`'un yapılandırılmış cevabı `sonuclar` taşır — her sonuç için `nesne` (kimliği),
`islem` (onu yapan işin kimliği, örneğin `islem.tampon`), `ad` (`TAMPON`), `durum`
(`guncel`, `guncel_degil`, `kaynaksiz`), `degisen` ve `silinen` (kaynakların
kimlikleri) — ve üç sayı: `guncel`, `guncel_degil`, `kaynaksiz`. `kabul` ve `coz`'un
cevabı `islem` ve `nesneler` (işlenen sonuçların kimlikleri) taşır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | Kimliği [NESNEBİLGİ](entity_info.md) ya da seçimle doğrulayın |
| `Seçilen nesnelerin hiçbiri kaynağına bağlı bir sonuç değil.` | Verilen nesneler bir sonuç değil: çizilmiş, kopyalanmış ya da çözülmüş | Hata değildir; sonuçlar TAMPON, ALANÜRET, SINIR ve EŞYÜKSELTİ'nin çıktılarıdır |
| `Çizimde kaynağına bağlı bir sonuç yok.` | Çizimde hiç sonuç yok | Hata değildir |
| `Güncel olmayan bir sonuç yok; değişen bir şey olmadı.` | `kabul` ya da `coz` için güncel olmayan sonuç yok | Hata değildir |
| `N nesne kaynağına bağlı bir sonuç değil; atlandı.` | Verilenlerin bir kısmı sonuç değil | Hata değildir; yalnız sonuçlar işlendi |

## İlgili

- [Bağımlılıklar ve sonuçlar](../veri/bagimliliklar.md) — hangi nesne neyi izler, neyi bilir
- [Nesne kimliği ve kökeni](../veri/kimlik-ve-koken.md)
- [Bağlı nesneler](../islem/bagli-nesneler.md) — kaynağını izleyen yazılar
- [NESNEBİLGİ](entity_info.md)
