# KESİŞİM — Kesişim

Kapalı alanları düzenleyen kullanıcılar için; bu sayfayla kesişim işlemini şeritten, komut satırından ve betikten yapabilirsiniz.

## Ne yapar

İki kapalı alanın ortak bölgesini oluşturur. İşlem [OpenCASCADE](../veri/geometri-cekirdegi.md) üzerinden yürür; doğru ve yay sınırları korunur, sonuç ilk kaynağın katmanında oluşturulur.

Çok parçalı bir alanın her dış halkası kendi delikleriyle işlenir; diğer parçalar delik sayılmaz.

Varsayılan olarak sonuç kaynakların yerine geçer. `kaynaklari_koru=evet` kaynakları da saklar. Sonuç boşsa kaynaklar daima korunur.

Ortak stil ve aynı öznitelik değerleri korunur. Ayrışan stil katman stiline döner; ayrışan sütunlar boş bırakılır ve cevapta belirtilir.

## Adlar

`KESİŞİM` · KESISIM · INTERSECTION · AKS · `core.area_intersection`.

## Sözdizimi

```text
KESİŞİM [nesneler=<kimlikler>] [kaynaklari_koru=<evet|hayır>]
```

## Parametreler

Tam iki kapalı alan gerekir. Parametrelerin tam tanımı [oluşturulmuş komut referansında](referans.md) bulunur. Kaynaklar düz alan, kapalı yaylı çoklu çizgi veya daire olabilir. Tek seçili alanla başlanırsa diğer girdi sorulur.

## Örnekler

```
ALAN 0,0 10,0 10,10 0,10
ALAN 5,5 15,5 15,15 5,15
KESİŞİM nesneler=1 nesneler=2
```

Sonuç 25 m² alandır. Şeritte **Değiştir ▸ Alan İşlemleri ▸ Kesişim** düğmesini kullanın. Alan seçildiğinde açılan **Alan** sekmesinde, daire seçildiğinde **Eğri** sekmesinde de aynı grup bulunur. Klavyede `KESİŞİM` yazıp Enter'a basın; nesneleri seçip Enter ile tamamlayın.

## Geri alma

`GERİAL` bütün sonuçları kaldırır ve kaynakları tek adımda geri getirir. `YİNELE` aynı sonucu yeniden oluşturur. İşlem çalışırken **Durdur**, seçim sırasında Esc hiçbir sonuç bırakmaz.

## Betikten kullanım

JSON koordinatları milimetredir; aşağıdaki tam betik aynı örneği çizer.

```json
{
  "ad": "Kesişim örneği",
  "komutlar": [
    {"cmd": "core.area", "args": {"noktalar": [[0, 0], [10000, 0], [10000, 10000], [0, 10000]]}},
    {"cmd": "core.area", "args": {"noktalar": [[5000, 5000], [15000, 5000], [15000, 15000], [5000, 15000]]}},
    {"cmd": "core.area_intersection", "args": {"nesneler": [1, 2]}}
  ]
}
```

## Hatalar

- “Kesişim en az iki kapalı alan ister.”: iki farklı alan sağlayın.
- “Nesne kimlikleri pozitif ve birbirinden farklı olmalı.”: aynı kaynağı iki kez vermeyin.
- “Nesne bulunamadı: …”: canlı nesne kimliklerini kullanın.
- “Nesne … kapalı bir alan değil. Alan, kapalı yaylı çizgi veya daire seçin.”: açık çizgileri önce kapatın.
- “Nesne … geçersiz bir alan sınırı içeriyor; kaynaklar değiştirilmedi.”: kendi kendini kesen sınırı düzeltin.
- “Sonuç eğrili bir sınır ve delik içeriyor; bu alan biçimi henüz kaydedilemiyor. Kaynaklar değiştirilmedi.”: mevcut belge modeli eğrili ve delikli sonuçları birlikte saklamaz; işlem tamamen geri alınır. Elips/spline sınırlarını değiştiren bu komutlar da henüz desteklenmez; sessizce düz çizgiye çevrilmez.
- “OpenCASCADE alan işlemini tamamlayamadı; kaynaklar değiştirilmedi.”: kaynak geometrisini denetleyin; kısmi sonuç kaydedilmez.
