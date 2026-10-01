# KentOS sistem stil kitaplığı

Kaynak: kardeş `kentos-cad` projesinin `apps/web/src/style/system/` dizini ve
`crates/native/style/assets/system-library.kstil` dosyası, 2026-10-01 kopyası.
Kullanıcı isteğiyle özgün kaynak dosyalar ve kategori yapısı korunarak aktarıldı.

- `system/`: KentOS'un aynı dizin yapısındaki TypeScript sembol tanımları.
- `assets/system-library.kstil`: özgün 776 öğe ve 138 kategori; değiştirilmeden korunur.
- `assets/system-library.json`: PiriCAD'in okuduğu katalog; 695 sembol ve 81 SVG öğesi.
- `assets/mpyy/svg/`: kaynak SVG öğelerinin değişmeden kopyaları.
- `assets/generated/`: bileşik işaretçi ve desenlerin kendine yeterli SVG vektör motifleri.

Kaynak SHA-256:
`5864c9b9f3ab726b78281796269c2829a8bc394c89dd25300eefecb507c77d1e`.

`python3 scripts/adapt-kentos-styles.py` kataloğu üretir; `--check` üretilen
katalog ve bütün SVG dosyalarının kaynakla güncel olduğunu doğrular. Aynı kontrol
`ci-gate-catalogs.sh` içinde de çalışır; yerel katalog şeması
`data/catalogs/schema/system-library.schema.json` dosyasındadır. Kaynak
kimlikleri, isimleri, kategori yolları, etiketleri ve referansları taşınır.
Yerel dosya adları sembolün noktayla ayrılmış kimliğinden alt dizinlere çevrilir.

Çizgi, dolgu ve taramalar yerel sembol katmanlarıdır; bileşik işaretçi, dalga,
resim ve desenler SVG'dir. Çizgi işaretçilerinin aralığı, fazı, dönüş seçimi,
iç köşe ve segment merkezi yerleşimleri SVG bilgisiyle korunur. Bir sembol
uygulandığında görüntü baytları belgenin içine alınır; kaynak dosyanın yolu
çizimin sonradan açılabilmesi için gerekli değildir.

Uyarlamanın sınırları: kaynak ifadeler özgün tanımlarda korunur, mevcut yerel
katalogda sabit yedek değerleri kullanılır; bileşik SVG içindeki metinler nesne
öznitelikleriyle henüz yeniden üretilmez. Rastgele serpilmiş desenler, kaynak
hücre karmasıyla üretilen 16×16 tekrar eden bir vektör döşemeye çevrilir.
Bileşik işaretçilerin iç parçaları şimdilik ayrı form alanları olarak düzenlenmez.
Bunlar KentOS ifade/desen motoruyla tam davranış eşitliği iddiası değildir.

MPYY'nin önceki resmî eklerden çıkarılan paketleri `data/catalogs/` altında
ayrıca korunur. Bu kaynak projenin kataloğu, resmî ekin yeni bir yayımı değildir.
