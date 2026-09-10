<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->
<!-- Kaynak: kentos::core::builtin_kinds().  Yeniden üret: make reference -->
<!-- Bir nesne türünün burada görünmesi için tek yapılması gereken onu kaydetmektir; -->
<!-- projede elle tutulan ikinci bir tür listesi yoktur (model.md R25). -->

# Nesne Türleri Referansı

Bu tablo çekirdeğin tür kaydından üretilir. Her türün ayrıntılı sayfası
`docs/nesneler/` altındadır ve tablodan bağlanır. Kimlik dosyaya yazılan sayıdır ve
bir kez verildikten sonra asla başka anlama gelmez (model.md R26).

| Tür | Kimlik | Adlar | Açıklama |
|---|---|---|---|
| [`core.polyline`](coklucizgi.md) | 1 | `ÇOKLUÇİZGİ`, `COKLUCIZGI`, `POLYLINE`, `PL` | Açık ya da kapalı halkalardan oluşan temel çizgi nesnesi. |
| [`core.circle`](daire.md) | 2 | `DAİRE`, `DAIRE`, `CIRCLE`, `DR` | Merkez ve yarıçapla tanımlı daire. |
| [`core.arc`](yay.md) | 3 | `YAY`, `YAY`, `ARC`, `YY` | Merkez, yarıçap ve iki uçla tanımlı yay. |
| [`core.point`](nokta.md) | 4 | `NOKTA`, `NOKTA`, `POINT`, `NK` | Ölçülmüş tek nokta: nirengi, poligon noktası, röper. |
| [`core.ellipse`](elips.md) | 5 | `ELİPS`, `ELIPS`, `ELLIPSE`, `EL` | Merkez ve iki eksen ucuyla tanımlı elips. |
| [`core.arc_polyline`](yaylicizgi.md) | 6 | `YAYLIÇİZGİ`, `YAYLICIZGI`, `ARCPOLYLINE`, `YPL` | Kenarları yay olabilen çoklu çizgi ya da alan; DXF şişkinliğinin türü. |
| [`core.spline`](spline.md) | 7 | `SPLINE`, `SPLINE`, `SPLINE`, `SPL` | Kontrol noktaları, derece ve düğümlerle tanımlı NURBS eğrisi. |
| [`core.hatch`](tarama.md) | 8 | `TARAMA`, `TARAMA`, `HATCH`, `TRM` | Sınır halkaları ve deseniyle tanımlı tarama; dolu ya da çizgi desenli. |
| [`core.block_reference`](blokreferansi.md) | 9 | `BLOKREFERANSI`, `BLOKREFERANSI`, `INSERT`, `BR` | Bir blok tanımını noktaya, ölçekle, açıyla ve dizi olarak yerleştiren nesne. |
| [`core.dimension`](olcu.md) | 10 | `ÖLÇÜ`, `OLCU`, `DIMENSION`, `DIM` | Uzunluğu ya da açıyı yazısı, çizgisi ve oklarıyla gösteren ölçü. |
| [`core.leader`](lider.md) | 11 | `LİDER`, `LIDER`, `LEADER`, `LD` | Bir noktayı gösteren oklu çizgi; yazısı ayrı bir metin nesnesidir. |
