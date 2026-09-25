# PYTHON — Python Parçacığı Çalıştırma

Bir satırlık ya da birkaç satırlık Python kodunu çizimin üzerinde çalıştırmak isteyen
kullanıcı için; bu sayfayı bitirdiğinizde bir parçacığı komut satırından, panelden ve
betikten çalıştırabilecek ve tamamının tek adımda geri alındığını bileceksiniz.

## Ne yapar

Verilen Python kaynağını gömülü yorumlayıcıda çalıştırır. Kaynak, `BETİK` bir `.py`
dosyası için ne yapıyorsa aynısından geçer: aynı `cad` modülü, aynı kum havuzu, aynı
günlük kaydı.

Parçacığın **tamamı tek bir geri alma adımıdır** ve **tek bir doğrulama geçişinden**
geçer. Yakalanmamış bir hata olursa o ana kadar yapılan **her şey geri alınır**.

Python yazmanın tamamı: [Python betikleri](../betik/python.md).

Bu yapıda Python yoksa komut bunu söyler ve çizim değişmez:

```text
Bu yapıda Python yok. KENTOS_WITH_PYTHON=ON ile derleyin.
```

## Adlar

| Ad | Tür |
|---|---|
| `PYTHON` | Birincil |
| `PİTON` | Türkçe okunuşu |
| `PITON` | Türkçe karaktersiz klavye için |
| `PY` | Kısaltma |
| `core.python` | Komut kimliği |

## Sözdizimi

```
PYTHON
PYTHON kod="<python kaynağı>"
```

Kod verilmezse komut sorar. Kaynak tırnak içine alınır; içinde tırnak varsa ters bölü ile
kaçırılır. Çok satırlı kod için komut satırı yerine **Python konsolunu** kullanın:
**Görünüm ▸ Pencereler ▸ Python Konsolu**.

## Parametreler

Tek parametresi vardır: **`kod`** — çalıştırılacak Python kaynağı.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
PYTHON kod="cad.run('ÇİZGİ 0,0 10,10')"
```

```
PYTHON kod="print(cad.doc.entity_count())"
```

### Arayüz

**Görünüm ▸ Pencereler ▸ Python Konsolu** panelini açar. İstemde kod yazıp **Enter**'a basarsınız;
**Shift+Enter** satır ekler, **Yukarı** ve **Aşağı** gönderilenleri geri getirir. Panel
yazdığınızı `>>>` ile, programın söylediklerini altına yazar.

İstemin solunda satır numarası değil, Python'un kendi istemi durur: bir deyimin ilk
satırı `>>>`, devam eden satırlar `...` ile başlar. `for` satırından sonra gövdesi
beklenirken ilk satır da `...` olur.

Yazarken iki yardımcı açılır ve ikisi de **yazdığınız satırın üstünü örtmez**:

- **İmza ipucu** — bir `cad.` çağrısının parantezi içindeyken komutun parametreleri,
  üzerinde bulunduğunuz parametre vurgulu ve Türkçe açıklamasıyla. Satırın hemen
  yanında durur ve yazdıkça yer değiştirmez.
- **Tamamlama listesi** — `cad.`'den sonra komutlar, bir çağrının içinde o komutun
  anahtar sözcükleri (`points=`), başka yerde betiğin kendi adları ve Python'un
  sözcükleri. İpucunun ötesinde açılır; **Yukarı/Aşağı** ile seçip **Enter** ya da
  **Tab** ile yerleştirirsiniz, **Esc** kapatır. **Ctrl+Boşluk** istediğiniz yerde
  açar.

İkisi, istem pencerenin alt kenarındaysa satırın üstüne, yer varsa altına istiflenir:
ipucu satıra en yakın, liste onun ötesinde.

Panel bu komutun bir istemcisidir: her gönderim bir `core.python` çağrısıdır, dolayısıyla
panelden yapabildiğiniz her şeyi komut satırından da yapabilirsiniz.

### Betik

```json
{ "cmd": "core.python", "args": { "kod": "for i in range(5): cad.run(f'NOKTA {i},0')" } }
```

### Üçü de aynı

Aynı kaynak, aynı belge, aynı günlük satırları. Panel, komut satırı ve JSON betik bu
komutun eşit istemcileridir.

## Geri alma

Parçacığın tamamı **tek bir `GERİAL`** ile geri alınır — kaç komut çalıştırdığından
bağımsız olarak.

Kaynak bir hata verirse ve siz onu `try`/`except` ile yakalamadıysanız, parçacığın o ana
kadar yaptığı her şey geri alınır: yarım uygulanmış bir parçacık bırakılmaz. Hatayı
yakalarsanız kalanının uygulanmasına siz karar vermiş olursunuz; başarısız komut yine de
hiçbir şey bırakmaz.

## Betikten kullanım

`core.python` betiklenebilir: bir JSON betiği ya da bir `.py` dosyası onu çağırabilir.

**Yapay zekâya kapalıdır ve kapalı kalacaktır** (`CLAUDE.md` 5.24). Bir ajan komut
önerir; komut önizlenebilir, doğrulanır ve tek tek günlüğe yazılır. Bir parçacık
çalışana kadar bunların hiçbiri değildir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bu yapıda Python yok. KENTOS_WITH_PYTHON=ON ile derleyin.` | Yorumlayıcı derlenmemiş | Seçeneği açıp yeniden derleyin ([Kurulum](../baslangic/kurulum.md)) |
| `Python hatası: Python betiği: …` | Kaynağın kendi hatası; `PYTHON` başarısız olur | İletideki satır numarasına bakın; çizim değişmedi, `YİNELE` yarım kalanı geri getirmez |
| `Python hatası: Betik komutu (…): …` | Çalıştırılan bir komut doğrulamadan geçemedi ve yakalanmadı | Komutun kendi sayfasına bakın; parçacığın tamamı geri alındı |
| `'tam' kum havuzu bu betik için onaylanmamış.` | `tam` seviyede onaysız kaynak | Onayı verin; onay kaynağın kendisine verilir |

## İlgili

- [Python betikleri](../betik/python.md) — dilin ve `cad` modülünün tamamı
- [Python API referansı](../python/referans.md) — her komutun Python imzası
- [BETİK](script.md) — bir dosyayı çalıştırmak
