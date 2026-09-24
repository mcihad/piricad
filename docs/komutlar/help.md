# YARDIM — Yardım

Hangi komutun ne yaptığını hatırlamak isteyen herkes için; bu sayfayı bitirdiğinizde
program içinden komut listesine ve tek bir komutun parametrelerine ulaşabileceksiniz.

## Ne yapar

Parametresiz çağrıldığında komutları kategorilerine göre özetler ve — arayüzde —
**Komut Listesi sayfasını** açar: solda kategori başlıkları altında bütün komutlar,
sağda imlecin üzerinde olduğu komutun aldığı parametreler. `komut` parametresi
verildiğinde tek bir komutun parametrelerini, tiplerini ve adetlerini yazar; sayfa da
o komutun üzerinde açılır.

Sayfa arayüzün işidir, metin ise her istemcinin: betikten, komut satırından ya da bir
ajandan çağrıldığında `YARDIM` metni aynı biçimde yazar. Komut, karşısında bir pencere
olup olmadığına bakmaz.

Listenin kaynağı komut kaydıdır. Yeni bir komut eklendiğinde `YARDIM` onu **kendiliğinden**
bilir; elle güncellenen ikinci bir liste yoktur.

## Adlar

| Ad | Tür |
|---|---|
| `YARDIM` | Türkçe, birincil |
| `HELP` | İngilizce karşılık |
| `?` | Kısaltma |
| `core.help` | Komut kimliği |

## Sözdizimi

```
YARDIM
YARDIM komut=<komut-adı>
```

## Parametreler

Tek parametresi vardır: **`komut`** — ayrıntısı istenen komutun adı. İsteğe bağlıdır.
Türkçe adı, İngilizce karşılığını, kısaltmasını veya komut kimliğini yazabilirsiniz.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bütün komutları listele:

```
YARDIM
```

Çıktı — kategori başına bir satır, tamamı için sayfa:

```text
Komutlar (97). Ayrıntı: YARDIM komut=<ad>; aramak için Ctrl+K.
Çizim (20): ÇİZGİ, ÇOKLUÇİZGİ, NOKTA, METİN, ALAN, DİKDÖRTGEN, DAİRE, YAY, DİLİM, HALKA, ELİPS, SPLINE, TARAMA, BLOK, BLOKEKLE, ÖLÇÜ, LİDER, KILAVUZ, ETİKET, EŞYÜKSELTİ
Düzenleme (31): YAZIDÜZENLE, KÖŞETAŞI, KÖŞEEKLE, ALANAÇEVİR, TAŞI, KOPYALA, DİZİ, BİRLEŞTİR, BÖL, BUDA, UZAT, PAH, YUVARLA, KATMANAT, STİLKOPYALA, DÖNDÜR, ÖLÇEKLE, AYNALA, OFSET, ÖZNİTELİK, SÜTUN, SİL, SEÇ, OTURT, DÖNÜŞTÜR, TEVHİT, İFRAZ, ALANİFRAZ, ALANDÜZENLE, BAĞÇÖZ, BAĞLA
Görünüm (2): KAYDIR, YAKINLAŞ
Katman (4): KATMAN, KATMANGÖRÜNÜM, STİL, SEMBOL
Dosya (14): STİLAKTAR, NOKTALAR, ÇIKTIYERLEŞİMİ, ÇIKTIÖĞE, ÇIKTIŞABLON, YENİ, AÇ, KAYDET, FARKLIKAYDET, İÇEAKTAR, DIŞAAKTAR, VERİTABANI, YAZDIR, YAZDIRMAPROFİLİ
Sorgu (14): ÖLÇ, ALANÖLÇ, KOORDİNAT, APLİKASYON, TOPOLOJİ, HACİM, KATMANLAR, ÖZNİTELİKŞEMASI, SORGULA, SEÇİMBİLGİSİ, GÖRÜNÜMBİLGİSİ, BAĞLAM, ARAÇARA, İŞŞABLONU
İşlem (2): UZUNLUKYAZ, KÖŞENUMARALA
Betik (1): BETİK
Sistem (9): GERİAL, YİNELE, AYAR, TERCİH, MOD, YARDIM, ÖNERİ, MCPSUNUCU, YAPAYZEKAMODELİ
```

Komut sayısı ve adlar sizin yapınızdaki kayda göre değişir; yukarıdaki çıktı 97 komutlu
bir yapıdan alınmıştır.

Tek bir komutun ayrıntısı:

```
YARDIM komut=ÇİZGİ
```

Çıktı:

```text
core.line  (ÇİZGİ, CIZGI, LINE, Ç, L)  — İki veya daha fazla nokta arasında doğru parçaları çizer.
    noktalar : point_list [en az 2]  Ardışık doğru parçalarının köşe noktaları
```

Kısaltma da çalışır:

```
? komut=KAT
```

### Arayüz

**KentOS CAD ▸ Komut Listesi** (`F1`) ve `Ctrl+K` aynı sayfayı açar; komut satırına `YARDIM`
yazmak da aynı sayfayı açar. Üçü de aynı komutu çalıştırır, ayrı bir yol yoktur.

Sayfanın düzeni:

| Bölüm | Ne gösterir |
|---|---|
| Üst alan | Süzgeç. Yazdığınız şey ad, kısaltma, komut kimliği ve açıklamada aranır; `cizgi` yazmak `ÇİZGİ`yi bulur |
| Sol liste | Kategori başlıkları (Çizim, Düzenleme, Görünüm, Katman, Dosya, Sorgu, İşlem, Betik, Sistem) altında komutlar: adı, tek satır açıklaması ve sağ kenarda kısaltmaları |
| Sağ bölme | İmlecin üzerinde olduğu komut: kategorisi, komut kimliği, kabul ettiği bütün yazımlar, açıklaması ve parametre listesi — her parametrenin tipi, gerekli mi, aralığı, birimi ve varsa sözcük listesi |
| Alt satır | Komut sayısı ve tuşlar |

Tuşlar: `↑` `↓` gezinir, `Enter` seçili komutu komut satırına yazar (parametrelerini
orada tamamlarsınız), `Esc` kapatır. Süzgeç alanındaki imleç hiç oradan ayrılmaz.

**KentOS CAD ▸ Hakkında** sürümü, Qt'yi, çizim motorunu, platformu ve komut sayısını
gösterir; **Bileşenler** sayfası programın üzerine kurulduğu açık kaynak bileşenleri,
**Lisans** sayfası lisans metnini taşır. **Bilgileri Kopyala** bu bilgileri bir hata
bildirimine yapıştırmak için panoya alır.

### Betik

```json
{
  "ad": "Komutları listele",
  "komutlar": [
    { "cmd": "core.help", "args": {} }
  ]
}
```

Çıktı transkripte yazılır. Betikte nadiren gerekir; genellikle komut satırından
kullanılır.

## Geri alma

`YARDIM` çizime dokunmaz, geri alma yığınına girmez. Salt okunur bir komuttur.

## Betikten kullanım

`YARDIM` betiklenebilir ve salt okunur işaretlidir.

Komut kataloğunun tamamını makine biçiminde istiyorsanız `YARDIM` yerine üretilmiş
[komut referansına](referans.md) bakın; orada her komutun tam parametre tablosu ve AI
araç şemasının JSON hâli de vardır. Referansı yeniden üretmek için:

```bash
make reference
```

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen komut: 'XYZ'` | `komut=` ile verilen ad bulunamadı | Parametresiz `YARDIM` ile listeye bakın |
| `'core.help': bilinmeyen parametre 'ad'. Tanımlı parametreler: komut` | Parametre adı yanlış | `komut=` yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
