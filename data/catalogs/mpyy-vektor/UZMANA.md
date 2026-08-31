# Uzman imzası bekleyen eşlemeler

Bu paket 476 gösterimi taşır ve `STİL sinifla=<sütun>` bir nesnenin özniteliğini
gösterimin **kimliğine, adına ya da takma adına** göre çözer. Yönetmeliğin EK-1e
detay kataloğundaki 379 kartın 339'u zaten harfi harfine ya da katlanınca
eşleşir; 13'ü bu pakette `takma_adlar` olarak yazılmıştır ve her biri
yönetmeliğin **kendi öteki yazımıdır**, yorum değil.

Aşağıdakiler yazılmadı. Hepsi bir alanın hangi gösterime ait olduğuna dair
**karar** ister, ve o karar harita mühendisi / şehir plancısı imzasıdır
(CLAUDE.md 6.11). Uydurulmuş bir eşleme, imzalanan bir imar planına yanlış
gösterim yazar.

## 1. Yoğunluk kademeleri — on kart

EK-1e beş kademe adlandırır ve her birinin kişi/ha aralığını yazar:

| Kart | Aralık |
|---|---|
| MEVCUT KONUT ALANI … ÇOK YÜKSEK | 601 kişi/ha üstünde |
| MEVCUT KONUT ALANI … YÜKSEK | 301–600 |
| MEVCUT KONUT ALANI … ORTA | 151–300 |
| MEVCUT KONUT ALANI … DÜŞÜK | 51–150 |
| MEVCUT KONUT ALANI … SEYREK | 50 kişi/ha altında |
| GELİŞME KONUT ALANI … ÇOK YÜKSEK | 401 kişi/ha üstünde |
| GELİŞME KONUT ALANI … YÜKSEK | 251–400 |
| GELİŞME KONUT ALANI … ORTA | 121–250 |
| GELİŞME KONUT ALANI … DÜŞÜK | 51–120 |
| GELİŞME KONUT ALANI … SEYREK | 50 kişi/ha altında |

Gösterim tarafında her aile için **beş satır** vardır ve beşi de aynı adı taşır
(`nip-mevcut-konut-alani-brut-yogunluguna-gore`, `…-2` … `…-5`). Ada göre eşleşme
bu yüzden hep ilkini seçer ve dördü sessizce yanlış çizilir.

Karar gereken şey: **hangi kimlik hangi kademedir.** Ölçüm, satırların sırayla
seyrekleşen dikey taramalar olduğunu söylüyor (3,4 → 4,5 → 6,1 → 7,1 mm), ki bu
yoğunluğun azalması demektir; ama beşinci satır ölçülemedi ve sıranın yönetmelik
sırası olduğu bir varsayımdır. İmza bunu onaylarsa eşleme `takma_adlar` ile ya da
`kurallar` içinde bir aralık koşuluyla yazılabilir.

## 2. EK-1e'de iki kartın birleştiği satırlar

Çıkarım bu kartlarda iki adı tek hücrede birleştirmiş; hangisinin hangi gösterime
gittiği kartın kendisinden okunamıyor:

- `TEKNOLOJİ GELİŞTİRME BÖLGESİ SERBEST BÖLGE`
- `ORGANİZE SANAYİ BÖLGESİ HİZMET VE DESTEK ALANI (HDA) ENDÜSTRİ BÖLGESİ`
- `BAĞLIK-BAHÇELİK ALAN MEVCUT KONUT ALANI (BRÜT YOĞUNLUĞUNA GÖRE) ÇOK YÜKSEK …`
- `HAVARAY İSTASYONU TOPLUTAŞIM TÜRLERİ ARASI DEĞİŞİM VE AKTARMA ALANI`
- `KENTSEL RİSK ALANI TSUNAMİ RİSKLİ ALAN`

Bunlar kaynak/çıkarım kusurudur; düzeltilecek yer eşleme tablosu değil, EK-1e
çıkarımıdır.

## 3. Gösterim karşılığı olmayan detay sınıfları

`İL MERKEZİ`, `İLÇE MERKEZİ`, `BELDE MERKEZİ`, `KÖY MERKEZİ` — EK-1e bunları
detay sınıfı olarak sayar, EK-1a…EK-1d bunlar için bir gösterim basmaz.

## 4. Birden çok gösterime yakın duran kartlar

- `GOLF TURİZMİ` → `cdp-golf-turizm-bolgesi` mi, `nip-golf-turizm-alani` mı?
  Kartın ait olduğu plan türü belirler; kart bunu yazmıyor.
- `DİĞER ÖZEL KANUNLARLA BELİRLENEN ALAN SINIRLARI` → EK-1a'daki `… SAYILI KANUN`
  satırı mı, EK-1ç'deki `STATÜSÜ ÖZEL KANUNLARLA BELİRLENEN ALAN SINIRI` mı?
