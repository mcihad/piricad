# FARK — Fark

Kapalı alanları düzenleyen kullanıcılar için; bu sayfayla fark işlemini şeritten, komut satırından ve betikten yapabilirsiniz.

## Ne yapar

İlk verilen kapalı alandan diğer alanların bütününü çıkarır. Seçim sırası sonucu değiştirir. İşlem [OpenCASCADE](../veri/geometri-cekirdegi.md) üzerinden yürür; doğru ve yay sınırları korunur, sonuç ilk kaynağın katmanında oluşturulur.

Çok parçalı bir alanın her dış halkası kendi delikleriyle işlenir; diğer parçalar delik sayılmaz.

Varsayılan olarak sonuç kaynakların yerine geçer. `kaynaklari_koru=evet` kaynakları da saklar. Sonuç boşsa kaynaklar daima korunur.

Farkta tek alan önceden seçiliyse bu alan tutulur ve çıkarılacak alanlar sorulur. Çoklu etkin seçim tıklama sırasını saklamadığından, tutulacak alan ayrıca sorulur. Komut satırında `nesneler=` listesinin ilk alanı tutulur; listedeki başka bir alan `tutulan=` ile belirtilebilir. Sonuç tutulacak alanın stilini ve özniteliklerini alır.

## Adlar

`FARK` · DIFFERENCE · SUBTRACT · AFR · `core.area_difference`.

## Sözdizimi

```text
FARK [nesneler=<kimlikler>] [kaynaklari_koru=<evet|hayır>] [tutulan=<kimlik>]
```

## Parametreler

En az iki kapalı alan gerekir. Parametrelerin tam tanımı [oluşturulmuş komut referansında](referans.md) bulunur. Kaynaklar düz alan, kapalı yaylı çoklu çizgi veya daire olabilir. Tek seçili alanla başlanırsa diğer girdi sorulur.

## Örnekler

```
ALAN 0,0 10,0 10,10 0,10
ALAN 5,5 15,5 15,15 5,15
FARK nesneler=1 nesneler=2
```

Sonuç 75 m² alandır. Şeritte **Değiştir ▸ Alan İşlemleri ▸ Fark** düğmesini kullanın. Alan seçildiğinde açılan **Alan** sekmesinde, daire seçildiğinde **Eğri** sekmesinde de aynı grup bulunur. Klavyede `FARK` yazıp Enter'a basın; nesneleri seçip Enter ile tamamlayın.

## Geri alma

`GERİAL` bütün sonuçları kaldırır ve kaynakları tek adımda geri getirir. `YİNELE` aynı sonucu yeniden oluşturur. İşlem çalışırken **Durdur**, seçim sırasında Esc hiçbir sonuç bırakmaz.

## Betikten kullanım

JSON koordinatları milimetredir; aşağıdaki tam betik aynı örneği çizer.

```json
{
  "ad": "Fark örneği",
  "komutlar": [
    {"cmd": "core.area", "args": {"noktalar": [[0, 0], [10000, 0], [10000, 10000], [0, 10000]]}},
    {"cmd": "core.area", "args": {"noktalar": [[5000, 5000], [15000, 5000], [15000, 15000], [5000, 15000]]}},
    {"cmd": "core.area_difference", "args": {"nesneler": [1, 2]}}
  ]
}
```

## Hatalar

- “Fark en az iki kapalı alan ister.”: iki farklı alan sağlayın.
- “Nesne kimlikleri pozitif ve birbirinden farklı olmalı.”: aynı kaynağı iki kez vermeyin.
- “Nesne bulunamadı: …”: canlı nesne kimliklerini kullanın.
- “Nesne … kapalı bir alan değil. Alan, kapalı yaylı çizgi veya daire seçin.”: açık çizgileri önce kapatın.
- “Nesne … geçersiz bir alan sınırı içeriyor; kaynaklar değiştirilmedi.”: kendi kendini kesen sınırı düzeltin.
- “Sonuç eğrili bir sınır ve delik içeriyor; bu alan biçimi henüz kaydedilemiyor. Kaynaklar değiştirilmedi.”: mevcut belge modeli eğrili ve delikli sonuçları birlikte saklamaz; işlem tamamen geri alınır. Elips/spline sınırlarını değiştiren bu komutlar da henüz desteklenmez; sessizce düz çizgiye çevrilmez.
- “OpenCASCADE alan işlemini tamamlayamadı; kaynaklar değiştirilmedi.”: kaynak geometrisini denetleyin; kısmi sonuç kaydedilmez.
- “Tutulacak alan işlemdeki alanlardan biri olmalı.”: listeden bir alan seçin.
