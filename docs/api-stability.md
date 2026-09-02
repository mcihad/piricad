# Sürüm ve Uyumluluk Politikası

KentOSCad üzerine iş kuran, eklenti yazan veya çıktı biçimlerine bağımlı sistem geliştiren
herkes için; bu sayfayı bitirdiğinizde neyin sabit kalacağını, neyin ne zaman
değişebileceğini bileceksiniz.

KentOSCad [Semantik Sürümleme](https://semver.org) kullanır: `BÜYÜK.KÜÇÜK.YAMA`.

## Neyin garantisi var

| Yüzey | Garanti | Ne zaman kırılabilir |
|---|---|---|
| Eklenti C arayüzü | Bir büyük sürüm içinde yalnız ekleme yapılır; yüklemede sürüm el sıkışması olur | Yalnız büyük sürümde |
| Komut kimlikleri (`core.line`) | Yayımlandıktan sonra sonsuza kadar sabit; adı değişen komut takma ad olarak korunur | Asla |
| Komut adları (`ÇİZGİ`, `LINE`, `Ç`) | Yalnız ekleme yapılır; yayımlanmış bir ad başka bir komuta devredilmez | Asla |
| Komut parametreleri | Yalnız ekleme yapılır; yayımlanmış parametre adını, tipini ve anlamını korur | Yalnız büyük sürümde |
| Komut günlüğü biçimi | İleriye uyumlu; bilinmeyen alanlar oynatmada yok sayılır | Yalnız büyük sürümde |
| Proje dosyası biçimi | Sürümlenir; eski sürüm yeni dosyayı açarken açıklayıcı mesaj verir, çökmez | Yalnız büyük sürümde |
| Betik API'si | Bir büyük sürüm içinde yalnız ekleme yapılır | Yalnız büyük sürümde |
| C++ başlıkları (`piricad/`) | İç kullanım. Küçük sürümler arasında garanti yoktur | Her sürümde |

## Komut kimlikleri neden kalıcı

Komut günlüğü aynı anda geri almanın, makro kaydının, regresyon testinin, oturum
kurtarmanın ve uzak API'nin kaynağıdır. Üç yıl önce kaydedilmiş bir oturumun bugün
oynatılabilmesi gerekir.

Bu, bir komut kimliğini uygulama ayrıntısı değil, **veri biçimi** yapar. Yayımlanmış bir
kimlik hiçbir zaman kaldırılmaz; komut yeniden adlandırılırsa eski kimlik yenisinin takma
adı olur.

## Kullanımdan kaldırma

Bir yüzey kullanımdan kaldırılırken şu sıra izlenir:

1. Yerine geçecek yüzey hangi sürümde geliyorsa, eskisi aynı sürümde "kullanımdan
   kaldırılacak" olarak işaretlenir.
2. En az **iki küçük sürüm** boyunca çalışmaya devam eder; kullanıldığında uyarı verir.
3. Kaldırılışı `CHANGELOG.md` dosyasında `### Kaldırıldı` başlığı altında kaydedilir.
4. Komut kimliği kaldırılmaz — halefinin takma adı olur.

## Sürüm numarası ne anlatır

| Değişiklik | Örnek |
|---|---|
| **YAMA** (`0.1.0` → `0.1.1`) | Hata düzeltmesi; davranış değişmez |
| **KÜÇÜK** (`0.1.0` → `0.2.0`) | Yeni komut, yeni parametre, yeni özellik; eskisi çalışmaya devam eder |
| **BÜYÜK** (`0.1.0` → `1.0.0`) | Yukarıdaki tablodaki garantilerden birinin kırılması |

`0.x` sürümleri henüz kararlılık taahhüdü altında değildir; ilk kararlı taahhüt `1.0.0`
ile başlar.

## Sırada ne var

- [Komut sistemi](komutlar/README.md) — komut kimliklerinin nerede göründüğü
- [Komut günlüğü](mimari/gunluk.md) — kalıcılığın neden önemli olduğu
