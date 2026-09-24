# SPDX-License-Identifier: GPL-3.0-or-later
# ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN.  Yeniden üret: make reference
#
# `kentos.cad`'in tip taslağı. Program bu dosyayı ÇALIŞTIRMAZ: gerçek yüzey
# çalışma anında komut kaydından kurulur. Bu taslak, betiği programın
# dışında yazan bir düzenleyicinin tamamlama ve tip denetimi yapabilmesi
# içindir.
#
# Koordinatlar milimetre tam sayıdır ve [Sağa, Yukarı] sırasındadır.

from typing import Any

# Bir koordinat iki biçimde yazılabilir ve ikisi de her yerde geçerlidir.
Coord = 'Point | list[int]'
Coords = 'list[Coord]'

class _Document:
    """Çizimden okuma. Hiçbiri çizimi değiştirmez."""

    def layers(self) -> list[str]: ...
    def layer_count(self) -> int: ...
    def active_layer(self) -> str: ...
    def entity_count(self) -> int: ...
    def selection_count(self) -> int: ...
    def crs(self) -> str: ...
    def setting(self, id: str) -> Any: ...

doc: _Document

class Point:
    """Bir koordinat, milimetre tam sayı.

    east  — sağa değer (paftada Y).   north — yukarı değer (paftada X).
    Harfler bilerek sunulmuyor: kodda ve paftada ters şeyler demek.
    """

    def __init__(self, east: int, north: int) -> None: ...
    @property
    def east(self) -> int: ...
    @property
    def north(self) -> int: ...
    def distance_to(self, other: 'Point') -> float: ...
    def __len__(self) -> int: ...
    def __getitem__(self, i: int) -> int: ...
    def __iter__(self) -> Any: ...

class Box:
    """Eksenlere paralel bir dikdörtgen, milimetre tam sayı."""

    def __init__(self, min_east: int, min_north: int,
                 max_east: int, max_north: int) -> None: ...
    @property
    def min_east(self) -> int: ...
    @property
    def min_north(self) -> int: ...
    @property
    def max_east(self) -> int: ...
    @property
    def max_north(self) -> int: ...
    @property
    def width(self) -> int: ...
    @property
    def height(self) -> int: ...
    @property
    def center(self) -> Point: ...
    @property
    def corners(self) -> tuple[Point, Point, Point, Point]: ...
    def contains(self, point: Point) -> bool: ...
    def is_empty(self) -> bool: ...
    def __iter__(self) -> Any: ...

class _Viewport:
    """Pencerenin o an baktığı yer. Pencere yoksa exists() False döner."""

    def exists(self) -> bool: ...
    def bbox(self) -> Box: ...
    def center(self) -> Point: ...
    def scale(self) -> int: ...
    def mm_per_pixel(self) -> float: ...
    def size_px(self) -> tuple[int, int]: ...
    def crs(self) -> str: ...

viewport: _Viewport

def run(*parts: str) -> int:
    """Bir komut satırını veri yoluna gönderir; metre cinsinden."""

def sandbox() -> str: ...
def read_file(path: str) -> str: ...
def write_file(path: str, text: str) -> None: ...

def line(
    *,
    points: Coords = ...,
) -> int:
    """İki veya daha fazla nokta arasında doğru parçaları çizer.

    Komut: core.line (ÇİZGİ)
        points — Ardışık doğru parçalarının köşe noktaları [mm, Sağa (Y) önce]
    """

def polyline(
    *,
    points: Coords = ...,
) -> int:
    """Birden çok noktadan TEK bir çizgi nesnesi çizer.

    Komut: core.polyline (ÇOKLUÇİZGİ)
        points — Çoklu çizginin köşe noktaları; hepsi tek nesne olur [mm, Sağa (Y) önce]
    """

def point_draw(
    *,
    points: Coords = ...,
) -> int:
    """Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.

    Komut: core.point_draw (NOKTA)
        points — Yerleştirilecek noktalar [mm, Sağa (Y) önce]
    """

def perp_offset(
    *,
    start: Coord = ...,
    end: Coord = ...,
    chainage: list[float] = ...,
    offset: list[float] = ...,
    connect: bool = ...,
) -> int:
    """Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir.

    Komut: core.perp_offset (DİKAYAK)
        start — Taban çizgisinin ilk noktası (A) [mm, Sağa (Y) önce]
        end — Taban çizgisinin ikinci noktası (B) [mm, Sağa (Y) önce]
        chainage — A'dan taban boyunca uzaklık (m); boy ile sırayla eşleşir
        offset — Tabana dik uzaklık (m); A→B yönünde SOL pozitiftir
        connect — Yerleştirilen noktaları verildikleri sırayla çizgiyle birleştirir
    """

def survey_polar(
    *,
    station: Coord = ...,
    backsight: Coord = ...,
    angle: list[float] = ...,
    distance: list[float] = ...,
    connect: bool = ...,
) -> int:
    """İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir.

    Komut: core.survey_polar (ALIM)
        station — Aletin durduğu bilinen nokta [mm, Sağa (Y) önce]
        backsight — Bağlama noktası: verilirse açılar ondan itibaren okunmuş sayılır [mm, Sağa (Y) önce]
        angle — Okunan açı; kenar ile sırayla eşleşir [oturumun açı birimi]
        distance — Alete olan uzaklık (m) [m]
        connect — Hesaplanan noktaları okundukları sırayla çizgiyle birleştirir
    """

def intersect_point(
    *,
    method: str = ...,
    first: Coord = ...,
    second: Coord = ...,
    third: Coord = ...,
    fourth: Coord = ...,
    first_angle: float = ...,
    second_angle: float = ...,
    first_distance: float = ...,
    second_distance: float = ...,
    side: str = ...,
    side_point: Coord = ...,
    intersection: Coord = ...,
) -> int:
    """İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar.

    Komut: core.intersect_point (KESİŞİMNOKTA)
        method — dogrultu: iki doğrultu · mesafe: iki uzaklık · dogru: iki doğru
        first — Birinci bilinen nokta [mm, Sağa (Y) önce]
        second — İkinci bilinen nokta [mm, Sağa (Y) önce]
        third — İkinci doğrunun ilk noktası [mm, Sağa (Y) önce]
        fourth — İkinci doğrunun ikinci noktası [mm, Sağa (Y) önce]
        first_angle — Birinci noktadan okunan doğrultu
        second_angle — İkinci noktadan okunan doğrultu
        first_distance — Birinci noktadan ölçülen uzaklık (m)
        second_distance — İkinci noktadan ölçülen uzaklık (m)
        side — İki uzaklık kesişiminin hangi çözümü; birinci→ikinci yönüne göre
        side_point — mesafe: iki çözümden istenenin gösterildiği nokta; yon verilmişse sorulmaz [mm, Sağa (Y) önce]
        intersection — Bulunan nokta; günlüğe yazılır [mm, Sağa (Y) önce]
    """

def point_along(
    *,
    first: Coord = ...,
    second: Coord = ...,
    method: str = ...,
    value: list[float] = ...,
    count: int = ...,
) -> int:
    """İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta koyar.

    Komut: core.point_along (ARANOKTA)
        first — Doğrunun ilk noktası [mm, Sağa (Y) önce]
        second — Doğrunun ikinci noktası [mm, Sağa (Y) önce]
        method — oran: 0 ile 1 arası · mesafe: ilk noktadan metre
        value — Oran ya da uzaklık; birden çok verilebilir
        count — Doğruyu bu kadar eşit parçaya böler
    """

def polygon_regular(
    *,
    center: Coord = ...,
    sides: int = ...,
    method: str = ...,
    radius: float = ...,
    side_length: float = ...,
    angle: float = ...,
    corner: Coord = ...,
) -> int:
    """Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar uzunluğundan.

    Komut: core.polygon_regular (ÇOKGEN)
        center — Çokgenin merkezi [mm, Sağa (Y) önce]
        sides — Kenar sayısı
        method — ic: köşeler çemberin üzerinde · dis: kenarlar çembere teğet · kenar: kenar uzunluğundan
        radius — ic/dis yönteminin yarıçapı (m) [m]
        side_length — kenar yönteminin uzunluğu (m) [m]
        angle — İlk köşenin merkeze göre doğrultusu; varsayılan 0
        corner — Yerine işaret edilen nokta: yarıçapı ve yönü verir; yaricap verilmişse sorulmaz [mm, Sağa (Y) önce]
    """

def break(
    *,
    object: list[int] = ...,
    first: Coord = ...,
    second: Coord = ...,
) -> int:
    """Çizgiden, yaydan, daireden ya da yaylı çoklu çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta açık bir nesneyi boşluk bırakmadan böler.

    Komut: core.break (KIR)
        object — Kırılacak nesne: çizgi, yay, daire ya da yaylı çoklu çizgi [kalıcı nesne anahtarı]
        first — Kırılacak parçanın ilk noktası [mm, Sağa (Y) önce]
        second — Kırılacak parçanın ikinci noktası; verilmezse boşluk bırakmadan böler [mm, Sağa (Y) önce]
    """

def join(
    *,
    object: list[int] = ...,
    tolerance: float = ...,
    on_conflict: str = ...,
) -> int:
    """Uçları birbirine değen çizgileri, yayları ve yaylı çoklu çizgileri tek bir nesneye ekler; yaylar yay kalır, boşluklar söylenir.

    Komut: core.join (UÇUCA)
        object — Uç uca eklenecek çizgiler, yaylar ve yaylı çoklu çizgiler [kalıcı nesne anahtarı]
        tolerance — Uçların değmiş sayılması için en büyük açıklık (m); varsayılan 0,001. Aradaki boşluk doğru parçasıyla kapatılır ve söylenir [m]
        on_conflict — ilk: katman, stil ve öznitelikler ilk nesneden, farklar söylenir · reddet: katman ya da öznitelik farklıysa birleştirmez
    """

def lengthen(
    *,
    object: list[int] = ...,
    delta: float = ...,
    percent: float = ...,
    total: float = ...,
    which_end: str = ...,
) -> int:
    """Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir.

    Komut: core.lengthen (UZUNLUK)
        object — Uzunluğu değişecek çizgi [kalıcı nesne anahtarı]
        delta — Eklenecek uzunluk (m); eksi kısaltır [m]
        percent — İstenen uzunluk, şimdikinin yüzdesi
        total — İstenen toplam uzunluk (m) [m]
        which_end — Hangi uç hareket eder; varsayılan son
    """

def explode(
    *,
    object: list[int] = ...,
) -> int:
    """Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine ayırır.

    Komut: core.explode (PATLAT)
        object — Patlatılacak nesneler [kalıcı nesne anahtarı]
    """

def align(
    *,
    object: list[int] = ...,
    source: Coord = ...,
    target: Coord = ...,
    source2: Coord = ...,
    target2: Coord = ...,
    scale: bool = ...,
    source3: Coord = ...,
    target3: Coord = ...,
) -> int:
    """Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler.

    Komut: core.align (HİZALA)
        object — Hizalanacak nesneler [kalıcı nesne anahtarı]
        source — Birinci kaynak nokta [mm, Sağa (Y) önce]
        target — Birinci kaynağın gideceği yer [mm, Sağa (Y) önce]
        source2 — İkinci kaynak nokta; verilirse döndürme de yapılır [mm, Sağa (Y) önce]
        target2 — İkinci kaynağın gideceği yer [mm, Sağa (Y) önce]
        scale — İki çiftin uzunluk oranıyla ölçekler de
        source3 — Üçüncü kaynak nokta: hedefi ilk iki hedefin öbür yanındaysa nesneler ters çevrilir [mm, Sağa (Y) önce]
        target3 — Üçüncü kaynağın gideceği yan [mm, Sağa (Y) önce]
    """

def divide(
    *,
    object: list[int] = ...,
    count: int = ...,
    spacing: float = ...,
    block: str = ...,
    align: bool = ...,
) -> int:
    """Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya blok yerleştirir.

    Komut: core.divide (BÖLÜMLE)
        object — Bölünecek nesne [kalıcı nesne anahtarı]
        count — Kaç eşit parçaya bölünecek
        spacing — Sabit aralık (m); başlangıçtan itibaren yürür [m]
        block — Nokta yerine bu bloğu koyar; blok önceden tanımlı olmalı
        align — Bloğu üzerinde durduğu kenarın doğrultusuna çevirir
    """

def pedit(
    *,
    object: list[int] = ...,
    action: str = ...,
    tolerance: float = ...,
) -> int:
    """Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir.

    Komut: core.pedit (ÇİZGİDÜZENLE)
        object — Düzenlenecek çizgiler [kalıcı nesne anahtarı]
        action — kapat: kapalı alana çevir · ac: aç · ters: yönünü çevir · sadelestir: yakın köşeleri at
        tolerance — sadelestir: bu uzaklıktan yakın köşeler atılır (m) [m]
    """

def copy_clip(
    *,
    objects: list[int] = ...,
    file: str = ...,
    base_point: Coord = ...,
    with_base: bool = ...,
) -> int:
    """Seçili nesneleri çizimin kendi biçiminde panoya yazar.

    Komut: core.copy_clip (PANOYAKOPYALA)
        objects — Panoya alınacak nesneler; verilmezse seçim kullanılır [kalıcı nesne anahtarı]
        file — Panonun yazılacağı dosya; verilmezse ortak pano dosyası
        base_point — Yapıştırırken gösterilen yere gelecek taban noktası [mm, Sağa (Y) önce]
        with_base — evet: taban noktası nesneler seçildikten sonra sorulur
    """

def cut(
    *,
    objects: list[int] = ...,
    file: str = ...,
    base_point: Coord = ...,
    with_base: bool = ...,
) -> int:
    """Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı.

    Komut: core.cut (KES)
        objects — Kesilecek nesneler; verilmezse seçim kullanılır [kalıcı nesne anahtarı]
        file — Panonun yazılacağı dosya; verilmezse ortak pano dosyası
        base_point — Yapıştırırken gösterilen yere gelecek taban noktası [mm, Sağa (Y) önce]
        with_base — evet: taban noktası nesneler seçildikten sonra sorulur
    """

def paste(
    *,
    point: Coord = ...,
    in_place: bool = ...,
    file: str = ...,
) -> int:
    """Panodaki nesneleri çizime koyar; tek geri alma adımı.

    Komut: core.paste (YAPIŞTIR)
        point — Yapıştırılacak yerin sol alt köşesi; yerinde=evet ile gereksiz [mm, Sağa (Y) önce]
        in_place — Kopyalandığı koordinatlara yapıştırır
        file — Okunacak pano dosyası; verilmezse ortak pano dosyası
    """

def entity_info(
    *,
    objects: list[int] = ...,
) -> int:
    """Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini bildirir.

    Komut: core.entity_info (NESNEBİLGİ)
        objects — Bilgisi istenen nesneler [kalıcı nesne anahtarı]
    """

def measure_angle(
    *,
    apex: Coord = ...,
    first: Coord = ...,
    second: Coord = ...,
) -> int:
    """Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla yazar.

    Komut: core.measure_angle (AÇIÖLÇ)
        apex — Açının tepe noktası [mm, Sağa (Y) önce]
        first — Birinci kolun üzerinde bir nokta [mm, Sağa (Y) önce]
        second — İkinci kolun üzerinde bir nokta [mm, Sağa (Y) önce]
    """

def stretch(
    *,
    window: Coords = ...,
    start: Coord = ...,
    end: Coord = ...,
    objects: list[int] = ...,
) -> int:
    """Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır.

    Komut: core.stretch (ESNET)
        window — Esnetme penceresinin iki köşesi; içindeki köşeler taşınır [mm, Sağa (Y) önce]
        start — Esnetmenin başlangıç noktası [mm, Sağa (Y) önce]
        end — Esnetmenin bitiş noktası [mm, Sağa (Y) önce]
        objects — Yalnız bu nesneler esnetilir; verilmezse pencerenin dokunduğu her nesne [kalıcı nesne anahtarı]
    """

def tracking(
    *,
    point: Coord = ...,
    delete: bool = ...,
) -> int:
    """Geçici izleme için nokta işaretler; iki işaretin izleri kesişir.

    Komut: core.tracking (İZ)
        point — İşaretlenecek nokta; yoksa işaretler listelenir [mm, Sağa (Y) önce]
        delete — Bütün işaretleri siler
    """

def text(
    *,
    points: Coord = ...,
    text: str = ...,
    height: int = ...,
    end: Coord = ...,
    alignment: str = ...,
) -> int:
    """Çizime metin yazar; yükseklik ve hizalama verilebilir.

    Komut: core.text (METİN)
        points — Yazının başlangıç noktası [mm, Sağa (Y) önce]
        text — Yazılacak metin
        height — Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı
        end — Taban çizgisinin bitişi; yoksa yatay [mm, Sağa (Y) önce]
        alignment — sol, orta, sag veya merkez
    """

def edittext(
    *,
    objects: list[int] = ...,
    text: str = ...,
    height: int = ...,
    alignment: str = ...,
) -> int:
    """Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir.

    Komut: core.edittext (YAZIDÜZENLE)
        objects — Düzenlenecek yazılar; verilmezse seçim [kalıcı nesne anahtarı]
        text — Yeni metin; verilmezse değişmez
        height — Yeni yükseklik, zeminde milimetre; verilmezse değişmez
        alignment — sol, orta, sag veya merkez; verilmezse değişmez
    """

def exportstyle(
    *,
    layer: str = ...,
    file: str = ...,
) -> int:
    """Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar.

    Komut: core.exportstyle (STİLAKTAR)
        layer — Stili aktarılacak katmanın adı
        file — Yazılacak .qml dosyasının yolu
    """

def area(
    *,
    points: Coords = ...,
    rings: list[int] = ...,
) -> int:
    """Kapalı bir alan çizer; istenirse içine delik açar.

    Komut: core.area (ALAN)
        points — Alanın köşe noktaları; kapanış noktası tekrarlanmaz [mm, Sağa (Y) önce]
        rings — Halka uzunlukları: ilki dış sınır, sonrakiler delik
    """

def rectangle(
    *,
    points: Coords = ...,
    method: str = ...,
) -> int:
    """Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı bir alan çizer.

    Komut: core.rectangle (DİKDÖRTGEN)
        points — 2n: karşılıklı iki köşe · 3n: bir kenarın iki köşesi ve karşı kenarın geçtiği nokta [mm, Sağa (Y) önce]
        method — 2n: karşılıklı iki köşe, eksenlere paralel · 3n: bir kenar ve yükseklik, döndürülmüş
    """

def circle_draw(
    *,
    center: Coord = ...,
    rim: Coord = ...,
    method: str = ...,
    first: Coord = ...,
    second: Coord = ...,
    third: Coord = ...,
    fourth: Coord = ...,
    radius: float = ...,
    side: Coord = ...,
) -> int:
    """Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya teğet yarıçapla daire çizer.

    Komut: core.circle_draw (DAİRE)
        center — Dairenin merkezi [mm, Sağa (Y) önce]
        rim — Çember üzerinde bir nokta; yarıçapı bu belirler [mm, Sağa (Y) önce]
        method — merkez: merkez + çevre · 2n: çapın iki ucu · 3n: çember üzerinde üç nokta · ttr: iki doğruya teğet, verilen yarıçapla
        first — 2n: çapın bir ucu · 3n: birinci nokta · ttr: birinci doğrunun ilk noktası [mm, Sağa (Y) önce]
        second — İkinci nokta [mm, Sağa (Y) önce]
        third — 3n: üçüncü nokta · ttr: ikinci doğrunun ilk noktası [mm, Sağa (Y) önce]
        fourth — ttr: ikinci doğrunun ikinci noktası [mm, Sağa (Y) önce]
        radius — ttr: teğet dairenin yarıçapı (m) [m]
        side — ttr: dairenin geleceği köşe; dört çözümden en yakını alınır [mm, Sağa (Y) önce]
    """

def arc_draw(
    *,
    center: Coord = ...,
    start: Coord = ...,
    end: Coord = ...,
    method: str = ...,
    through: Coord = ...,
    sweep: float = ...,
    radius: float = ...,
    side_point: Coord = ...,
    side: str = ...,
) -> int:
    """Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da başlangıç+bitiş+yarıçapla yay çizer.

    Komut: core.arc_draw (YAY)
        center — Yayın merkezi [mm, Sağa (Y) önce]
        start — Yayın başlangıç noktası; merkez yönteminde yarıçapı bu belirler [mm, Sağa (Y) önce]
        end — Yayın bitiş noktası; süpürme saat yönünün tersinedir [mm, Sağa (Y) önce]
        method — merkez: merkez + iki uç · 3n: yay üzerinde üç nokta · bma: başlangıç, merkez ve süpürme açısı · bby: başlangıç, bitiş ve yarıçap
        through — 3n: yayın üzerinden geçtiği nokta [mm, Sağa (Y) önce]
        sweep — bma: süpürme açısı
        radius — bby: yarıçap (m) [m]
        side_point — bby: yayın hangi yandan geçeceği gösterilen nokta; yon verilmişse sorulmaz [mm, Sağa (Y) önce]
        side — bby: yayın hangi tarafa kavis yaptığı; başlangıç→bitiş yönüne göre
    """

def vertex_move(
    *,
    object: list[int] = ...,
    vertex: int = ...,
    at: Coord = ...,
    shared_point: Coord = ...,
    point: Coord = ...,
) -> int:
    """Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır.

    Komut: core.vertex_move (KÖŞETAŞI)
        object — Köşesi taşınacak nesne; birden çok nesne verilirse ortak köşeleri birlikte taşınır [kalıcı nesne anahtarı]
        vertex — Taşınacak köşenin sırası; ilk köşe 1'dir. Birden çok nesnede birincinin köşesi; verilmezse yer ya da kaynak
        at — Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce]
        shared_point — Ortak köşenin bugünkü yeri: verilen nesnelerin o noktadaki bütün köşe ve tutamakları birlikte taşınır [mm, Sağa (Y) önce]
        point — Köşenin yeni yeri [mm, Sağa (Y) önce]
    """

def vertex_insert(
    *,
    object: list[int] = ...,
    vertex: int = ...,
    at: Coord = ...,
    point: Coord = ...,
) -> int:
    """Bir kenarın ortasına yeni köşe ekler.

    Komut: core.vertex_insert (KÖŞEEKLE)
        object — Köşe eklenecek nesnenin kimliği [kalıcı nesne anahtarı]
        vertex — Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir
        at — Kenarı gösteren nokta: kose verilmezse en yakın kenar, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce]
        point — Yeni köşenin yeri [mm, Sağa (Y) önce]
    """

def vertex_delete(
    *,
    object: list[int] = ...,
    vertex: int = ...,
    at: Coord = ...,
    shared_point: Coord = ...,
) -> int:
    """Bir çizginin, alanın, yaylı çoklu çizginin ya da spline'ın köşesini siler; iki kenar tek kenar olur.

    Komut: core.vertex_delete (KÖŞESİL)
        object — Köşesi silinecek nesne; birden çok nesne verilirse ortak köşeleri birlikte silinir [kalıcı nesne anahtarı]
        vertex — Silinecek köşenin sırası; ilk köşe 1'dir. Verilmezse yer ya da kaynak
        at — Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce]
        shared_point — Ortak köşenin yeri: verilen nesnelerin o noktadaki köşesi birlikte silinir [mm, Sağa (Y) önce]
    """

def edge_kind(
    *,
    object: list[int] = ...,
    edge: int = ...,
    at: Coord = ...,
    kind: str = ...,
    point: Coord = ...,
) -> int:
    """Bir kenarın türünü değiştirir: düz kenarı bir noktadan geçen yaya, yayı düz kenara çevirir; nesnenin kimliği korunur.

    Komut: core.edge_kind (KENARTÜRÜ)
        object — Kenarı değişecek nesnenin kimliği [kalıcı nesne anahtarı]
        edge — Değişecek kenarın sırası; ilk kenar 1'dir. Verilmezse yer
        at — Kenarı gösteren nokta: kenar verilmezse en yakın kenar, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce]
        kind — yay: düz kenar yay olur; duz: yay düz olur. Verilmezse kenarın öbür türü
        point — tur=yay için yayın geçeceği nokta [mm, Sağa (Y) önce]
    """

def to_area(
    *,
    objects: list[int] = ...,
) -> int:
    """Uç uca değen çizgileri tek bir kapalı alana çevirir.

    Komut: core.to_area (ALANAÇEVİR)
        objects — Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
    """

def boundary(
    *,
    point: Coord = ...,
    islands: bool = ...,
    gap: int = ...,
    objects: list[int] = ...,
) -> int:
    """İçine tıklanan kapalı bölgenin sınırını yeni bir alan olarak çıkarır; içerideki adalar delik olur, açık uçlar gösterilir.

    Komut: core.boundary (SINIR)
        point — Sınırı çıkarılacak bölgenin içindeki nokta; yoksa sorulur [mm, Sağa (Y) önce]
        islands — İçerideki kapalı çizgiler delik olsun mu; varsayılan evet
        gap — Bu genişliğe kadar açık uçları köprüle, milimetre; varsayılan 0: hiçbir boşluk kendiliğinden kapanmaz [mm]
        objects — Sınır sayılacak nesneler; yoksa görünen her çizgi [kalıcı nesne anahtarı]
    """

def cleanup(
    *,
    objects: list[int] = ...,
    action: str = ...,
) -> int:
    """Yinelenen, boş ve tekrarlanan köşeli nesneleri bulur; istenirse tek adımda onarır ve değişen alanları önce/sonra raporlar.

    Komut: core.cleanup (TEMİZLE)
        objects — Bakılacak nesneler; yoksa seçim, o da boşsa bütün çizim [kalıcı nesne anahtarı]
        action — bul: bulur, seçer ve işaretler, hiçbir şeyi değiştirmez · onar: yinelenenleri ve boş nesneleri siler, tekrarlanan köşeleri çıkarır
    """

def move(
    *,
    objects: list[int] = ...,
    start: Coord = ...,
    end: Coord = ...,
) -> int:
    """Seçilen nesneleri iki nokta arasındaki kadar taşır.

    Komut: core.move (TAŞI)
        objects — Taşınacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        start — Taşımanın başlangıç noktası [mm, Sağa (Y) önce]
        end — Taşımanın bitiş noktası [mm, Sağa (Y) önce]
    """

def copy(
    *,
    objects: list[int] = ...,
    start: Coord = ...,
    end: Coords = ...,
) -> int:
    """Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar.

    Komut: core.copy (KOPYALA)
        objects — Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        start — Kopyalamanın başlangıç noktası [mm, Sağa (Y) önce]
        end — Kopyaların geleceği noktalar; her nokta bir kopya [mm, Sağa (Y) önce]
    """

def array(
    *,
    objects: list[int] = ...,
    mode: str = ...,
    rows: int = ...,
    columns: int = ...,
    row_spacing: float = ...,
    column_spacing: float = ...,
    center: Coord = ...,
    count: int = ...,
    angle: float = ...,
    path: list[int] = ...,
    path_point: Coord = ...,
    spacing: float = ...,
    follow: bool = ...,
    base_point: Coord = ...,
) -> int:
    """Seçilen nesneleri satır/sütun, bir merkez etrafında ya da bir yol boyunca çoğaltır.

    Komut: core.array (DİZİ)
        objects — Dizilecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        mode — KUTUPSAL için kutupsal dizi, YOL için yol boyunca dizi; verilmezse satır/sütun dizisi
        rows — Satır sayısı (dikdörtgen dizi)
        columns — Sütun sayısı (dikdörtgen dizi)
        row_spacing — Satır aralığı, metre; kuzeye artı
        column_spacing — Sütun aralığı, metre; doğuya artı
        center — Dizinin merkezi (kutupsal dizi) [mm, Sağa (Y) önce]
        count — Toplam kopya sayısı, özgün dahil (kutupsal ve yol boyunca dizi)
        angle — Süpürülecek toplam açı, derece; verilmezse tam tur
        path — mod=yol için dizinin izleyeceği yol: çizgi, yay, daire ya da yaylı çoklu çizgi [kalıcı nesne anahtarı]
        path_point — Yolu gösteren nokta; yol verilmişse sorulmaz [mm, Sağa (Y) önce]
        spacing — mod=yol için kopyalar arası uzaklık, metre; verilmezse sayi
        follow — mod=yol için kopyalar yolun doğrultusuna döndürülsün mü; varsayılan evet
        base_point — mod=yol için nesnelerin yola taşınan taban noktası; varsayılan yolun başı [mm, Sağa (Y) önce]
    """

def combine(
    *,
    objects: list[int] = ...,
) -> int:
    """Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar.

    Komut: core.combine (BİRLEŞTİR)
        objects — Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim [kalıcı nesne anahtarı]
    """

def split(
    *,
    object: list[int] = ...,
    points: Coords = ...,
    point: Coord = ...,
    method: str = ...,
    distance: float = ...,
    count: int = ...,
) -> int:
    """Nesneleri bir kesme çizgisiyle, üstündeki noktalardan, kesişimlerinden, baştan bir uzaklıktan ya da eşit parçalara böler; yaylar yay kalır.

    Komut: core.split (BÖL)
        object — Kesilecek nesneler; yoksa etkin seçim [kalıcı nesne anahtarı]
        points — cizgi: kesme çizgisinin iki noktası · nokta: nesnenin üstündeki bölme noktaları [mm, Sağa (Y) önce]
        point — Bölme noktası (tek çizgi; eski biçim) [mm, Sağa (Y) önce]
        method — cizgi: çizilen kesme çizgisinden · nokta: nesnenin üstündeki noktalardan · kesisim: seçilenlerin birbirini kestiği yerlerden · mesafe: baştan verilen uzaklıktan · esit: eşit parçalara
        distance — mesafe: baştan uzaklık (m) [m]
        count — esit: kaç eşit parça
    """

def trim(
    *,
    object: list[int] = ...,
    boundary: list[int] = ...,
    every_edge: bool = ...,
    point: Coords = ...,
    method: str = ...,
    fence: Coords = ...,
    keep: bool = ...,
    carry_edges: bool = ...,
) -> int:
    """Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve dairede çalışır.

    Komut: core.trim (BUDA)
        object — Budanan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne [kalıcı nesne anahtarı]
        boundary — Kesme sınırları; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne [kalıcı nesne anahtarı]
        every_edge — Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı)
        point — Atılacak her parçanın üzerinde bir nokta, sırayla [mm, Sağa (Y) önce]
        method — Parçalar nasıl gösterilir: tek tek tıklayarak (öntanımlı) ya da çizilen bir çitle
        fence — Çitin köşeleri; çitin geçtiği her parça budanır [mm, Sağa (Y) önce]
        keep — Gösterilen parça kalır; iki yanındaki kesimlerin dışında kalan gider
        carry_edges — Sınırlar kendi yolunda uzatılmış sayılır; nesneye yetişmeyen bir sınır da keser
    """

def extend(
    *,
    object: list[int] = ...,
    boundary: list[int] = ...,
    every_edge: bool = ...,
    point: Coords = ...,
    method: str = ...,
    fence: Coords = ...,
    carry_edges: bool = ...,
) -> int:
    """Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, yayınkini çemberi boyunca.

    Komut: core.extend (UZAT)
        object — Uzatılan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne [kalıcı nesne anahtarı]
        boundary — Uzatılacak sınırlar; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne [kalıcı nesne anahtarı]
        every_edge — Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı)
        point — Uzatılacak her ucun yakınında bir nokta, sırayla [mm, Sağa (Y) önce]
        method — Uçlar nasıl gösterilir: tek tek tıklayarak (öntanımlı) ya da çizilen bir çitle
        fence — Çitin köşeleri; çitin yanından geçtiği her uç uzatılır [mm, Sağa (Y) önce]
        carry_edges — Sınırlar kendi yolunda uzatılmış sayılır; ucun doğrultusuna yetişmeyen bir sınıra da ulaşılır
    """

def chamfer(
    *,
    object: list[int] = ...,
    point: Coord = ...,
    distance: float = ...,
    second_point: Coord = ...,
    second_distance: float = ...,
    trim: bool = ...,
    every_corner: bool = ...,
) -> int:
    """Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar).

    Komut: core.chamfer (PAH)
        object — Köşesi kesilecek nesne; iki nesne verilirse aralarındaki köşe; hepsi=evet ile bir ya da daha çok nesne [kalıcı nesne anahtarı]
        point — Tek nesnede işlem yapılacak köşe; iki nesnede birincinin kalacak parçası; hepsi=evet ise verilmez [mm, Sağa (Y) önce]
        distance — Köşeden her iki kenar boyunca kesilecek mesafe, metre
        second_point — İki nesnede ikincinin kalacak parçası [mm, Sağa (Y) önce]
        second_distance — İki çizgi arasında ikinci çizgi boyunca kesilecek mesafe, metre; verilmezse mesafe [m]
        trim — İki nesnede nesneler köşeye kadar kısaltılıp uzatılsın mı; varsayılan evet
        every_corner — Verilen nesnelerin bütün köşeleri aynı değerle; sığmayan köşe atlanır
    """

def fillet(
    *,
    object: list[int] = ...,
    point: Coord = ...,
    radius: float = ...,
    second_point: Coord = ...,
    trim: bool = ...,
    every_corner: bool = ...,
) -> int:
    """Bir köşeyi ya da iki nesne (çizgi, yay) arasındaki köşeyi verilen yarıçapta yayla yuvarlatır; 0 yarıçap keskin köşe kurar.

    Komut: core.fillet (YUVARLA)
        object — Köşesi yuvarlatılacak nesne; iki nesne verilirse aralarındaki köşe; hepsi=evet ile bir ya da daha çok nesne [kalıcı nesne anahtarı]
        point — Tek nesnede işlem yapılacak köşe; iki nesnede birincinin kalacak parçası; hepsi=evet ise verilmez [mm, Sağa (Y) önce]
        radius — Yuvarlatma yarıçapı, metre; iki nesnede 0 keskin köşe
        second_point — İki nesnede ikincinin kalacak parçası [mm, Sağa (Y) önce]
        trim — İki nesnede nesneler teğet noktalarına kadar kısaltılıp uzatılsın mı; varsayılan evet
        every_corner — Verilen nesnelerin bütün köşeleri aynı değerle; sığmayan köşe atlanır
    """

def set_layer(
    *,
    objects: list[int] = ...,
    layer: str = ...,
) -> int:
    """Seçilen nesneleri başka bir katmana taşır.

    Komut: core.set_layer (KATMANAT)
        objects — Taşınacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        layer — Hedef katmanın adı; yoksa oluşturulur
    """

def match_style(
    *,
    source: list[int] = ...,
    objects: list[int] = ...,
    point: Coord = ...,
) -> int:
    """Bir nesnenin stilini seçilen nesnelere uygular.

    Komut: core.match_style (STİLKOPYALA)
        source — Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne [kalıcı nesne anahtarı]
        objects — Stili alacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        point — Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde [mm, Sağa (Y) önce]
    """

def colour(
    *,
    objects: list[int] = ...,
    color: str = ...,
    fill: str = ...,
) -> int:
    """Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine döndürür.

    Komut: core.colour (RENK)
        objects — Rengi değişecek nesneler; verilmezse etkin seçim, o da boşsa sorulur [kalıcı nesne anahtarı]
        color — Çizgi rengi: #RRGGBB (ya da saydamlıkla #AARRGGBB) veya katman
        fill — Dolgu rengi: #RRGGBB, yok (dolgusuz) ya da katman
    """

def rotate(
    *,
    objects: list[int] = ...,
    center: Coord = ...,
    angle: float = ...,
    angle_point: Coord = ...,
    method: str = ...,
    reference: float = ...,
    reference_point: Coords = ...,
    copy: bool = ...,
) -> int:
    """Seçilen nesneleri bir merkez etrafında döndürür; açı verilir, gösterilir ya da bir referans doğrultudan bulunur.

    Komut: core.rotate (DÖNDÜR)
        objects — Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        center — Döndürme merkezi [mm, Sağa (Y) önce]
        angle — Dönme açısı, derece; artı yön saat yönünün tersi. Verilmezse yeni doğrultu gösterilir
        angle_point — Dönme açısının gösterildiği nokta; aci verilmişse sorulmaz [mm, Sağa (Y) önce]
        method — referans: bir doğrultu yenisine döndürülür; referans doğrultu iki noktayla gösterilir
        reference — Referans doğrultunun açısı, derece; aci onun yeni açısıdır
        reference_point — Referans doğrultuyu gösteren iki nokta [mm, Sağa (Y) önce]
        copy — evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün yerinde kalır
    """

def scale(
    *,
    objects: list[int] = ...,
    center: Coord = ...,
    factor: float = ...,
    factor_point: Coord = ...,
    factor_y: float = ...,
    method: str = ...,
    reference: float = ...,
    new_length: float = ...,
    reference_point: Coords = ...,
    copy: bool = ...,
) -> int:
    """Seçilen nesneleri bir merkeze göre büyütür ya da küçültür; iki çarpanla eşit olmayan ölçek, referans uzunlukla ölçek.

    Komut: core.scale (ÖLÇEKLE)
        objects — Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        center — Ölçekleme merkezi; bu nokta yerinde kalır [mm, Sağa (Y) önce]
        factor — Ölçek çarpanı; sıfırdan büyük. Verilmezse merkezden uzaklık gösterilir
        factor_point — Çarpanın gösterildiği nokta; carpan verilmişse sorulmaz [mm, Sağa (Y) önce]
        factor_y — Yukarı yöndeki çarpan; verilirse carpan yalnız sağa yöndeki çarpandır ve daire elips olur
        method — referans: bir uzunluk yenisine ölçeklenir; referans uzunluk iki noktayla gösterilir
        reference — Referans uzunluk, metre; yeni onun olacağı uzunluktur
        new_length — Referans uzunluğun yeni değeri, metre
        reference_point — Referans uzunluğu gösteren iki nokta [mm, Sağa (Y) önce]
        copy — evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün yerinde kalır
    """

def mirror(
    *,
    objects: list[int] = ...,
    start: Coord = ...,
    end: Coord = ...,
    copy: bool = ...,
) -> int:
    """Seçilen nesneleri iki noktadan geçen eksende aynalar.

    Komut: core.mirror (AYNALA)
        objects — Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        start — Ayna ekseninin ilk noktası [mm, Sağa (Y) önce]
        end — Ayna ekseninin ikinci noktası [mm, Sağa (Y) önce]
        copy — evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün yerinde kalır
    """

def measure(
    *,
    start: Coord = ...,
    end: Coord = ...,
    more: Coords = ...,
) -> int:
    """Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla nokta kenarları ve toplam uzunluğu verir.

    Komut: core.measure (ÖLÇ)
        start — Ölçümün ilk noktası [mm, Sağa (Y) önce]
        end — Ölçümün ikinci noktası [mm, Sağa (Y) önce]
        more — Sonraki noktalar: her biri bir kenar daha ekler, toplam da yazılır [mm, Sağa (Y) önce]
    """

def measure_area(
    *,
    objects: list[int] = ...,
    method: str = ...,
    points: Coords = ...,
) -> int:
    """Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini yazar.

    Komut: core.measure_area (ALANÖLÇ)
        objects — Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
        method — nesne: seçilen nesnelerin alanı (öntanımlı); nokta: köşeleri gösterilen alan
        points — yontem=nokta için alanın köşeleri; verilirse yöntem kendiliğinden nokta olur [mm, Sağa (Y) önce]
    """

def coordinate(
    *,
    point: Coord = ...,
) -> int:
    """Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.

    Komut: core.coordinate (KOORDİNAT)
        point — Okunacak nokta [mm, Sağa (Y) önce]
    """

def pan(
    *,
    start: Coord = ...,
    end: Coord = ...,
) -> int:
    """Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.

    Komut: core.pan (KAYDIR)
        start — Kaydırmanın tutulacağı nokta [mm, Sağa (Y) önce]
        end — O noktanın taşınacağı yer [mm, Sağa (Y) önce]
    """

def offset(
    *,
    objects: list[int] = ...,
    distance: int = ...,
    corner: str = ...,
    side: str = ...,
    through: Coord = ...,
    source: str = ...,
    properties: str = ...,
    attributes: str = ...,
) -> int:
    """Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire.

    Komut: core.offset (OFSET)
        objects — Ofseti alınacak nesneler; yoksa etkin seçim [kalıcı nesne anahtarı]
        distance — Paralel mesafesi, milimetre. Taraf verilmez ve gösterilmezse işaret anlam taşır: kapalı şekilde artı dışarı, eksi içeri
        corner — KÖŞE | YUVARLAK | PAH — dış köşenin biçimi
        side — Paralelin tarafı: açık çizgide sol ya da sag (çizim yönüne göre), kapalı şekilde dis ya da ic, iki her iki yan
        through — Tarafı gösteren nokta: her nesnenin paraleli bu noktanın olduğu yana düşer [mm, Sağa (Y) önce]
        source — Kaynak nesne: koru (öntanımlı) ya da paralel çizilince sil
        properties — Paralelin katmanı ve stili: kaynak nesneninki (öntanımlı) ya da etkin katman
        attributes — Kaynağın öznitelik değerleri: paralele aktar (öntanımlı) ya da aktarma — parselin içine çizilen çekme hattı gibi kaynağın kendisi olmayan bir çizgi için
    """

def sector(
    *,
    center: Coord = ...,
    start: Coord = ...,
    end: Coord = ...,
) -> int:
    """Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.

    Komut: core.sector (DİLİM)
        center — Dilimin merkezi [mm, Sağa (Y) önce]
        start — İlk kenarın ucu; yarıçapı bu belirler [mm, Sağa (Y) önce]
        end — İkinci kenarın yönü; süpürme saat yönünün tersinedir [mm, Sağa (Y) önce]
    """

def annulus(
    *,
    center: Coord = ...,
    inner: Coord = ...,
    outer: Coord = ...,
) -> int:
    """Merkez, iç ve dış yarıçaptan delikli halka çizer.

    Komut: core.annulus (HALKA)
        center — Halkanın merkezi [mm, Sağa (Y) önce]
        inner — İç çember üzerinde bir nokta [mm, Sağa (Y) önce]
        outer — Dış çember üzerinde bir nokta [mm, Sağa (Y) önce]
    """

def ellipse_draw(
    *,
    center: Coord = ...,
    first: Coord = ...,
    second: Coord = ...,
    method: str = ...,
    second_end: Coord = ...,
    start: float = ...,
    end: float = ...,
) -> int:
    """Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.

    Komut: core.ellipse_draw (ELİPS)
        center — Elipsin merkezi [mm, Sağa (Y) önce]
        first — merkez: birinci eksenin ucu · eksen: birinci eksenin bir ucu [mm, Sağa (Y) önce]
        second — İkinci eksenin uzaklığı; eksene dik ölçülür [mm, Sağa (Y) önce]
        method — merkez: merkez + eksen ucu · eksen: eksenin iki ucu
        second_end — eksen: birinci eksenin öteki ucu [mm, Sağa (Y) önce]
        start — Kısmi elips: başlangıç açısı, derece, birinci eksenden saat yönünün tersine
        end — Kısmi elips: bitiş açısı, derece; baslangic ile birlikte
    """

def spline(
    *,
    points: Coords = ...,
    degree: int = ...,
    closed: bool = ...,
) -> int:
    """Kontrol noktalarından NURBS eğrisi (spline) çizer.

    Komut: core.spline (SPLINE)
        points — Kontrol noktaları [mm, Sağa (Y) önce]
        degree — Eğrinin derecesi, 1–15; varsayılan 3
        closed — Son noktadan ilkine kapansın mı; varsayılan hayır
    """

def hatch(
    *,
    points: Coords = ...,
    objects: list[int] = ...,
    pattern: str = ...,
    angle: float = ...,
    scale: float = ...,
    catalog: str = ...,
) -> int:
    """Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.

    Komut: core.hatch (TARAMA)
        points — Sınır köşeleri, nesne seçmek yerine; en az üç nokta [mm, Sağa (Y) önce]
        objects — Sınırı verecek kapalı nesneler; yoksa etkin seçim ya da noktalar= [kalıcı nesne anahtarı]
        pattern — Katalogdaki desen adı: SOLID, ANSI31, NET…; varsayılan SOLID
        angle — Desenin dönme açısı, derece; varsayılan 0
        scale — Desen ölçeği; varsayılan pafta ölçeğinin paydası (AYAR plan_ölçeği)
        catalog — Desen kataloğu dosyası; varsayılan TERCİH desen_kataloğu
    """

def block(
    *,
    name: str = ...,
    base: Coord = ...,
    objects: list[int] = ...,
    note: str = ...,
) -> int:
    """Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.

    Komut: core.block (BLOK)
        name — Bloğun adı; Türkçe katlanmış hâliyle benzersiz
        base — Taban noktası: referansların yerleştirildiği nokta [mm, Sağa (Y) önce]
        objects — Bloğa girecek nesneler; yoksa etkin seçim [kalıcı nesne anahtarı]
        note — Serbest açıklama
    """

def insert(
    *,
    name: str = ...,
    point: Coord = ...,
    scale: float = ...,
    scale_y: float = ...,
    angle: float = ...,
    columns: int = ...,
    rows: int = ...,
    column_spacing: int = ...,
    row_spacing: int = ...,
) -> int:
    """Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.

    Komut: core.insert (BLOKEKLE)
        name — Yerleştirilecek bloğun adı
        point — Ekleme noktası [mm, Sağa (Y) önce]
        scale — Ölçek; eksi değer x'te aynalar; varsayılan 1
        scale_y — Y ölçeği, farklıysa; varsayılan olcek
        angle — Dönme açısı, derece; varsayılan 0
        columns — Dizi sütun sayısı; varsayılan 1
        rows — Dizi satır sayısı; varsayılan 1
        column_spacing — Sütunlar arası, milimetre, döndürülmüş eksende
        row_spacing — Satırlar arası, milimetre, döndürülmüş eksende
    """

def dimension(
    *,
    first: Coord = ...,
    second: Coord = ...,
    position: Coord = ...,
    type: str = ...,
    apex: Coord = ...,
    end: Coord = ...,
    style: str = ...,
    text: str = ...,
    catalog: str = ...,
    associate: bool = ...,
) -> int:
    """İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.

    Komut: core.dimension (ÖLÇÜ)
        first — Birinci nokta; açısal ölçüde birinci kolun ucu [mm, Sağa (Y) önce]
        second — İkinci nokta; açısal ölçüde ikinci kolun ucu [mm, Sağa (Y) önce]
        position — Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta [mm, Sağa (Y) önce]
        type — hizali (varsayılan), dogrusal, yaricap, cap, acisal, koordinat, yay
        apex — Açısal ölçünün tepe noktası [mm, Sağa (Y) önce]
        end — Yay uzunluğu ölçüsünün bitiş noktası [mm, Sağa (Y) önce]
        style — Katalogdaki ölçü stili: ISO-25 (varsayılan), STANDARD, MIMARI
        text — Ölçülen değer yerine yazılacak metin
        catalog — Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri
        associate — Tam denk geldiği köşe, merkez ya da yay ucuna bağlansın mı; bağlı ölçü kaynağı değişince güncellenir. Varsayılan evet
    """

def leader(
    *,
    points: Coords = ...,
    text: str = ...,
    style: str = ...,
    catalog: str = ...,
) -> int:
    """Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.

    Komut: core.leader (LİDER)
        points — Okun ucundan yazının yanına köşeler [mm, Sağa (Y) önce]
        text — Son köşenin yanına yazılacak metin
        style — Ok ve yazı boyunu veren ölçü stili; varsayılan ISO-25
        catalog — Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri
    """

def points(
    *,
    file: str = ...,
    mode: str = ...,
    objects: list[int] = ...,
    axis_order: str = ...,
) -> int:
    """Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).

    Komut: core.points (NOKTALAR)
        file — Nokta listesi dosyasının yolu
        mode — oku (varsayılan) | yaz
        objects — yon=yaz ile: köşeleri yazılacak nesneler; verilmezse çizimdeki noktalar [kalıcı nesne anahtarı]
        axis_order — Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY
    """

def guide(
    *,
    direction: str = ...,
    value: int = ...,
    point: Coord = ...,
    type: str = ...,
    delete: bool = ...,
) -> int:
    """Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler.

    Komut: core.guide (KILAVUZ)
        direction — yatay | düşey | bir açı (45, 45g, 30d); yoksa kılavuzlar listelenir
        value — Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa
        point — Açılı kılavuzun geçtiği nokta; yalnız `yon` bir açıysa [mm, Sağa (Y) önce]
        type — doğru: iki yöne sonsuz · ışın: noktadan ileriye
        delete — Verilen yerdeki kılavuzu siler
    """

def attribute(
    *,
    name: str = ...,
    object: int = ...,
    value: str = ...,
) -> int:
    """Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.

    Komut: core.attribute (ÖZNİTELİK)
        name — Öznitelik kimliği; yoksa tanımlı sütunlar listelenir
        object — Nesnenin kalıcı kimliği
        value — Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır
    """

def column(
    *,
    id: str = ...,
    type: str = ...,
    name: str = ...,
    note: str = ...,
    required: bool = ...,
    catalog: str = ...,
    digits: int = ...,
    layer: str = ...,
    delete: bool = ...,
) -> int:
    """Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler.

    Komut: core.column (SÜTUN)
        id — Sütun kimliği; yoksa tanımlı sütunlar listelenir
        type — tam_sayi, ondalik, uzunluk, evet_hayir, metin, tarih, kod
        name — Panelde görünen Türkçe ad
        note — Tek satırlık açıklama
        required — Her satır bir değer taşımalı mı
        catalog — Yalnız 'kod' türü için: katalog kimliği
        digits — Yalnız 'ondalik' için: noktadan sonraki basamak sayısı
        layer — Sütunu yalnız bu katmana tanımlar; yoksa proje geneli
        delete — Sütunu ve içindeki bütün değerleri siler
    """

def erase(
    *,
    objects: list[int] = ...,
) -> int:
    """Seçilen nesneleri siler.

    Komut: core.erase (SİL)
        objects — Silinecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı]
    """

def select(
    *,
    mode: str = ...,
    points: Coords = ...,
    type: str = ...,
    objects: list[int] = ...,
    layer: str = ...,
    action: str = ...,
    tolerance: float = ...,
    order: float = ...,
) -> int:
    """Nesneleri seçer: tümü, kimlikle, katman, pencere, kesen kutu, çokgen, çit, önceki seçim, son nesne ya da tek nokta.

    Komut: core.select (SEÇ)
        mode — TÜMÜ | TEMİZLE | NESNE | KATMAN | PENCERE | KESEN | KUTU | NOKTA | ÇOKGEN | ÇOKGENKESEN | ÇİT | ÖNCEKİ | SON
        points — Kutu köşeleri (iki nokta), çokgen/çit köşeleri ya da tek tıklama noktası [mm, Sağa (Y) önce]
        type — Yalnız bu türdeki nesneler: ÇOKLUÇİZGİ, DAİRE, YAY, NOKTA, ELİPS…
        objects — NESNE modunda nesne kimlikleri [kalıcı nesne anahtarı]
        layer — KATMAN modunda katman adı
        action — DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE
        tolerance — NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı
        order — NOKTA modunda kaçıncı nesne: 1 en yakını, 2 altındaki
    """

def label(
    *,
    layer: str = ...,
    format: str = ...,
    target_layer: str = ...,
    height: int = ...,
    offset: int = ...,
) -> int:
    """Katmandaki nesneleri özniteliklerinden okuyarak etiketler.

    Komut: core.label (ETİKET)
        layer — Etiketlenecek katmanın adı
        format — Etiket biçimi; {sutun} o sütunun değeriyle değişir, \n satır kırar. Sembol alan bildiriyorsa gerekmez
        target_layer — Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET'
        height — Yazı yüksekliği, zemin milimetresi
        offset — Nesnenin ortasından dikey kaydırma, zemin milimetresi; artı yukarı
    """

def layer(
    *,
    name: str = ...,
    group: str = ...,
    visible: bool = ...,
    locked: bool = ...,
    color: int = ...,
) -> int:
    """Katman oluşturur, aktif yapar ve özelliklerini değiştirir.

    Komut: core.layer (KATMAN)
        name — Katman adı; yoksa oluşturulur ve aktif yapılır
        group — Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök
        visible — Katmanın görünürlüğü
        locked — Katmanın kilit durumu
        color — Çizim rengi, 0xAARRGGBB
    """

def layer_visibility(
    *,
    action: str = ...,
    layer: str = ...,
) -> int:
    """Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir.

    Komut: core.layer_visibility (KATMANGÖRÜNÜM)
        action — goster, gizle, yalniz (yalnız bu katman), tumu (hepsini göster) ya da tersine
        layer — Katman adı; goster, gizle ve yalniz için gerekir, tersine için isteğe bağlı (verilmezse bütün katmanlar), tumu ile verilemez
    """

def layout(
    *,
    action: str = ...,
    name: str = ...,
    new_name: str = ...,
    paper: str = ...,
    width: int = ...,
    height: int = ...,
    orientation: str = ...,
    margin: int = ...,
    dpi: int = ...,
    page: int = ...,
    new_order: int = ...,
    layer: str = ...,
    sort_by: str = ...,
    group_by: str = ...,
    margin_percent: int = ...,
    single_file: bool = ...,
) -> int:
    """Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir.

    Komut: core.layout (ÇIKTIYERLEŞİMİ)
        action — Ne yapılacağı
        name — Yerleşimin adı; listele dışında gerekir
        new_name — islem=ad için yeni yerleşim adı
        paper — A5, A4, A3, A2, A1, A0 ya da ozel (varsayılan A4)
        width — ozel kâğıt için sayfa genişliği [kâğıt mm]
        height — ozel kâğıt için sayfa yüksekliği [kâğıt mm]
        orientation — Sayfa yönü (varsayılan dikey)
        margin — Kenar boşluğu (varsayılan 10) [kâğıt mm]
        dpi — Çıktı çözünürlüğü (varsayılan 300)
        page — Hangi sayfa (1'den başlar). sayfa işleminde verilmezse bütün sayfalar değişir
        new_order — sayfatasi için sayfanın gideceği sıra
        layer — atlas: hangi katmanın nesneleri için bir sayfa basılacak; 'yok' atlası kapatır
        sort_by — atlas: sayfaların sıralanacağı ve adlandırılacağı öznitelik sütunu; verilmezse nesne anahtarı
        group_by — rapor: bölümlerin oluşturulacağı öznitelik sütunu (ada_no gibi); verilmezse tek bölüm
        margin_percent — atlas: nesnenin çevresinde bırakılacak pay, yüzde (varsayılan 10)
        single_file — atlas: tek çok sayfalı belge mi, nesne başına bir dosya mı (varsayılan evet)
    """

def layout_item(
    *,
    action: str = ...,
    layout: str = ...,
    name: str = ...,
    type: str = ...,
    x: float = ...,
    y: float = ...,
    width: float = ...,
    height: float = ...,
    text: str = ...,
    text_height: float = ...,
    scale: int = ...,
    window: Coords = ...,
    grid: str = ...,
    grid_spacing: int = ...,
    locked: bool = ...,
    frame: bool = ...,
    page: int = ...,
    new_name: str = ...,
    max_rows: int = ...,
    fields: list[str] = ...,
    layers: list[str] = ...,
    map: str = ...,
    order: int = ...,
) -> int:
    """Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler.

    Komut: core.layout_item (ÇIKTIÖĞE)
        action — Ne yapılacağı
        layout — Hangi çıktı yerleşimi; çizimde tek yerleşim varsa gerekmez
        name — Öğe adı; ekle dışında gerekir, ekle'de verilmezse türetilir
        type — islem=ekle için öğe türü
        x — Sol kenardan uzaklık [kâğıt mm]
        y — ÜST kenardan uzaklık [kâğıt mm]
        width — Genişlik [kâğıt mm]
        height — Yükseklik [kâğıt mm]
        text — Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer tutucuları çizim anında çözülür
        text_height — Yazı yüksekliği [kâğıt mm]
        scale — Harita öğesinin ölçeği 1:N; 0 kapsama uyar
        window — Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve seçmek bu satırı yazar [ZEMİN koordinatı — kâğıt değil]
        grid — Harita öğesinin koordinat ızgarası
        grid_spacing — Izgara aralığı, zemin milimetresi; 0 ölçeğe göre seçilir
        locked — Öğeyi taşımaya kapatır
        frame — Öğenin çevresine çerçeve çizer
        page — Öğenin duracağı sayfa (1'den başlar); tasi ile verilir
        new_name — islem=ad için öğenin yeni adı
        max_rows — Tablo öğesinin yazacağı en çok satır; 0 = kutuya kaç satır sığıyorsa o kadar
        fields — Tablo öğesinin yazacağı öznitelik sütunları, sırasıyla; anahtar birden çok kez yazılır. Verilmezse katmanın bütün sütunları, 'hepsi' listeyi boşaltır
        layers — Harita çerçevesinin çizeceği katmanlar; anahtar birden çok kez yazılır. Verilmezse görünür bütün katmanlar, 'hepsi' listeyi boşaltır
        map — Bu öğenin bağlı olduğu harita çerçevesinin adı. Verilmezse ilk harita. 'ilk' bağı kaldırır
        order — Çizim sırası; büyük olan üstte
    """

def layout_template(
    *,
    action: str = ...,
    name: str = ...,
    layout: str = ...,
) -> int:
    """Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz.

    Komut: core.layout_template (ÇIKTIŞABLON)
        action — Ne yapılacağı
        name — Şablonun adı; listele dışında gerekir
        layout — kaydet: hangi yerleşim saklanacak (tek yerleşim varsa gerekmez). uygula: kurulacak yerleşimin adı (verilmezse şablonun adı)
    """

def style(
    *,
    layer: str = ...,
    package: str = ...,
    scale_min: int = ...,
    scale_max: int = ...,
    classify_by: str = ...,
    code: str = ...,
    scale: int = ...,
    color: int = ...,
    line_width: int = ...,
    fill: int = ...,
    order: int = ...,
    reset: bool = ...,
    layer_type: str = ...,
    add: bool = ...,
    shape: str = ...,
    placement: str = ...,
    unit: str = ...,
    size_unit: str = ...,
    spacing_unit: str = ...,
    spacing_y_unit: str = ...,
    offset_unit: str = ...,
    size: int = ...,
    spacing: int = ...,
    spacing_y: int = ...,
    angle: int = ...,
    offset: int = ...,
    phase: int = ...,
    phase_unit: str = ...,
    opacity: int = ...,
    pattern: str = ...,
    text: str = ...,
    fields: str = ...,
) -> int:
    """Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.

    Komut: core.style (STİL)
        layer — Stilin yazılacağı katmanın adı; katman var olmalı
        package — Stil kataloğu paketinin dosya yolu
        scale_min — Bu ölçek paydasından daha yakında çizilmez (1:N'deki N)
        scale_max — Bu ölçek paydasından daha uzakta çizilmez
        classify_by — Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir
        code — Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir
        scale — Ölçek paydası (1:N); 0 = ölçekten bağımsız
        color — Çizgi rengi, 0xAARRGGBB
        line_width — Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm)
        fill — Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz
        order — Çizim sırası; büyük olan üste gelir
        reset — Stili siler; nesneler katman varsayılanına döner
        layer_type — Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci
        add — Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir
        shape — İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik
        placement — İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta
        unit — Ölçülerin birimi: kagit (µm), zemin (mm), piksel
        size_unit — Yalnız `boyut` için birim; verilmezse `birim` geçerlidir
        spacing_unit — Yalnız `aralik` için birim; verilmezse `birim` geçerlidir
        spacing_y_unit — Yalnız `aralik_y` için birim; verilmezse `birim` geçerlidir
        offset_unit — Yalnız `kaydirma` için birim; verilmezse `birim` geçerlidir
        size — İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden
        spacing — Çizgi boyunca ya da desende birinci eksende aralık
        spacing_y — Nokta deseninde ikinci eksen; verilmezse kare desen
        angle — Desen açısı ya da işaretçi dönüklüğü, mikro derece
        offset — Geometriden dik kaydırma, `birim` cinsinden
        phase — İlk işaretçinin çizgi boyunca kaç birim ileride başlayacağı; verilmezse aralığın yarısı
        phase_unit — Yalnız `faz` için birim; verilmezse `birim` geçerlidir
        opacity — Katman saydamlığı 0-255; 255 tam opak
        pattern — Çizgi tipi: sürekli, ya da çizgi kalınlığının katı olarak çizgi/boşluk uzunlukları — '8 1 1 1' gibi (kesik-nokta)
        text — yazi-isaretci katmanının yazdığı sabit metin
        fields — Nesneden alınacak parametreler, virgülle: sütun[:özellik[:tür]] — 'kod:yazi:metin, kat:kalinlik'. Sütun yoksa tanımlanır
    """

def symbol(
    *,
    package: str = ...,
    group: str = ...,
    search: str = ...,
    code: str = ...,
) -> int:
    """Gösterim rafını yükler, ağacında gezer ve içinde arar.

    Komut: core.symbol (SEMBOL)
        package — Yüklenecek gösterim paketinin dosya yolu
        group — Gezilecek grup yolu, düzeyler '>' ile ayrılır
        search — Etikette, kimlikte ve grup yolunda arar
        code — Tek bir gösterimin ayrıntısı
    """

def zoom(
    *,
    mode: str = ...,
    factor: float = ...,
) -> int:
    """Görünümü çizim kapsamına veya verilen çarpana ayarlar.

    Komut: core.zoom (YAKINLAŞ)
        mode — KAPSAM | ÇARPAN | SIFIRLA
        factor — ÇARPAN modunda ölçek katsayısı
    """

def undo(
    *,
) -> int:
    """Son işlemi geri alır.

    Komut: core.undo (GERİAL)
    """

def redo(
    *,
) -> int:
    """Geri alınan işlemi yineler.

    Komut: core.redo (YİNELE)
    """

def new(
    *,
) -> int:
    """Boş bir çizim açar; ekrandaki çizimin yerine geçer.

    Komut: core.new (YENİ)
    """

def open(
    *,
    file: str = ...,
) -> int:
    """Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar.

    Komut: core.open (AÇ)
        file — Açılacak KentOSCad proje dosyasının yolu (.pcad)
    """

def save(
    *,
    file: str = ...,
) -> int:
    """Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder.

    Komut: core.save (KAYDET)
        file — Hedef yol; verilmezse çizimin bağlı olduğu dosyaya yazılır
    """

def saveas(
    *,
    file: str = ...,
) -> int:
    """Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar.

    Komut: core.saveas (FARKLIKAYDET)
        file — Yeni proje dosyasının yolu (.pcad)
    """

def import(
    *,
    file: str = ...,
    format: str = ...,
    layers: str = ...,
    fields: str = ...,
) -> int:
    """Dış bir veri dosyasını çizime ekler.

    Komut: core.import (İÇEAKTAR)
        file — İçe aktarılacak dosyanın yolu
        format — Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur
        layers — Yalnızca bu katmanlar okunur, virgülle ayrılır; verilmezse tümü
        fields — Sütun olarak okunacak öznitelik alanları, virgülle; * hepsi; verilmezse alan okunmaz
    """

def export(
    *,
    file: str = ...,
    format: str = ...,
    version: int = ...,
) -> int:
    """Çizimi dış bir veri biçimine yazar.

    Komut: core.export (DIŞAAKTAR)
        file — Yazılacak dosyanın yolu
        format — Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur
        version — DXF sürümü: 2000, 2004, 2007 (varsayılan), 2010, 2013, 2018
    """

def script(
    *,
    file: str = ...,
) -> int:
    """Bir betik dosyasını komut veri yolu üzerinden çalıştırır.

    Komut: core.script (BETİK)
        file — Çalıştırılacak betik dosyasının yolu
    """

def python(
    *,
    code: str = ...,
) -> int:
    """Bir Python parçacığını komut veri yolu üzerinden çalıştırır.

    Komut: core.python (PYTHON)
        code — Çalıştırılacak Python kaynağı
    """

def database(
    *,
    action: str = ...,
    target: str = ...,
    layer: str = ...,
) -> int:
    """PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar.

    Komut: core.database (VERİTABANI)
        action — baglan | kes | tablolar | katmanyaz | projekaydet | projeac | projeler | projesil
        target — baglan: bağlantı dizesi; katmanyaz: tablo adı; proje işlemleri: proje adı
        layer — katmanyaz: yazılacak katman; yoksa etkin katman
    """

def print(
    *,
    window: Coords = ...,
    center: Coord = ...,
    scale: int = ...,
    layout: str = ...,
    file: str = ...,
    printer: str = ...,
    profile: str = ...,
    paper: str = ...,
    width: int = ...,
    height: int = ...,
    orientation: str = ...,
    dpi: int = ...,
    margin: int = ...,
    title: str = ...,
    author: str = ...,
    password: str = ...,
    owner_password: str = ...,
    printable: bool = ...,
    copyable: bool = ...,
    modifiable: bool = ...,
) -> int:
    """Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir.

    Komut: core.print (YAZDIR)
        window — Yazdırılacak alanın iki köşesi; merkez verilmezse ve bu da verilmezse tıklatılır [mm, Sağa (Y) önce]
        center — Kâğıdın ortalanacağı nokta; pencere yerine kullanılır [mm, Sağa (Y) önce]
        scale — Ölçek paydası (1000 = 1/1000); merkez ile kullanılır, verilmezse projenin plan ölçeği
        layout — Basılacak çıktı yerleşiminin adı (ÇIKTIYERLEŞİMİ ile kurulur). Verildiğinde kâğıt, kenar ve harita penceresi yerleşimden gelir; pencere, merkez, olcek ve profil ile birlikte verilmez
        file — PDF yazılacak dosya; yazici ile birlikte verilmez
        printer — Yazıcının adı; "" sistem varsayılanı. dosya ile birlikte verilmez
        profile — Yazdırma profili; verilmezse varsayılan profil
        paper — Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel (genislik ve yukseklik ile)
        width — ozel kâğıdın eni, milimetre (dikey duruşta)
        height — ozel kâğıdın boyu, milimetre (dikey duruşta)
        orientation — dikey ya da yatay
        dpi — Çözünürlük, inç başına nokta (72–4800)
        margin — Dört yandaki kenar boşluğu, milimetre
        title — PDF belge başlığı
        author — PDF yazar alanı
        password — PDF açma şifresi (kullanıcı şifresi); günlüğe yazılmaz
        owner_password — PDF izinlerini değiştirme şifresi (sahip şifresi); günlüğe yazılmaz
        printable — Şifreli PDF: sahip şifresi olmayan yazdırabilir mi; varsayılan evet
        copyable — Şifreli PDF: metin ve grafik kopyalanabilir mi; varsayılan evet
        modifiable — Şifreli PDF: belge değiştirilebilir mi; varsayılan evet
    """

def print_profile(
    *,
    action: str = ...,
    name: str = ...,
    paper: str = ...,
    width: int = ...,
    height: int = ...,
    orientation: str = ...,
    dpi: int = ...,
    margin: int = ...,
) -> int:
    """Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır.

    Komut: core.print_profile (YAZDIRMAPROFİLİ)
        action — listele, ekle, sil ya da varsayilan
        name — Profilin adı (ekle, sil, varsayilan)
        paper — Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel; ekle için, varsayılan A4
        width — ozel kâğıdın eni, milimetre
        height — ozel kâğıdın boyu, milimetre
        orientation — dikey ya da yatay; varsayılan dikey
        dpi — Çözünürlük; varsayılan 300
        margin — Kenar boşluğu, milimetre; varsayılan 10
    """

def setting(
    *,
    name: str = ...,
    value: str = ...,
) -> int:
    """Proje ayarlarını listeler, okur ve değiştirir.

    Komut: core.setting (AYAR)
        name — Ayar adı veya kimliği; yoksa liste
        value — Yeni değer; yoksa yalnızca okur
    """

def preference(
    *,
    name: str = ...,
    value: str = ...,
) -> int:
    """Uygulama tercihlerini listeler, okur ve değiştirir.

    Komut: core.preference (TERCİH)
        name — Tercih adı veya kimliği; yoksa liste
        value — Yeni değer; yoksa yalnızca okur
    """

def mode(
    *,
    name: str = ...,
    value: str = ...,
) -> int:
    """Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir.

    Komut: core.mode (MOD)
        name — Mod adı veya kimliği; yoksa liste
        value — Yeni değer; yoksa yalnızca okur
    """

def help(
    *,
    command: str = ...,
) -> int:
    """Komut listesini veya tek bir komutun ayrıntısını gösterir.

    Komut: core.help (YARDIM)
        command — Ayrıntısı istenen komut adı
    """

def buffer(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
    distance: float = ...,
    dissolve: bool = ...,
    corner: str = ...,
    end: str = ...,
) -> int:
    """Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen tamponlar tek alan olur.

    Komut: islem.tampon (TAMPON)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
        distance — Tampon mesafesi, metre; eksi değer yalnız alanları içeri aşındırır
        dissolve — Üst üste binen tamponları tek alanda birleştir; kapalıysa her nesnenin tamponu ayrı alan olur; varsayılan evet
        corner — Dış köşelerin biçimi (yuvarlak / koseli / pah); varsayılan yuvarlak
        end — Çizgi uçlarının biçimi (yuvarlak / duz / kare); varsayılan yuvarlak
    """

def adjust_area(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
    area: float = ...,
    mode: str = ...,
    edge: int = ...,
    vertex: int = ...,
    point: Coord = ...,
) -> int:
    """Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz.

    Komut: islem.alan_duzenle (ALANDÜZENLE)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
        area — Hedef alan, metrekare
        mode — Nasıl getirileceği (hepsi / kenar / kose); varsayılan hepsi
        edge — Kaydırılacak kenar (ilk köşeden çıkan kenar 1); mod=kenar
        vertex — Çekilecek köşe; mod=kose
        point — Kenarın ya da köşenin gideceği yer; verilmezse arayüz sürükletir, komut satırı hedefe tam oturtur [mm, Sağa (Y) önce]
    """

def label_length(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
    unit: str = ...,
    decimals: int = ...,
    format: str = ...,
    decimal_separator: str = ...,
    side: str = ...,
    height: int = ...,
    gap: int = ...,
    min_length: int = ...,
    attach: bool = ...,
) -> int:
    """Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir.

    Komut: islem.uzunluk_yaz (UZUNLUKYAZ)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
        unit — Uzunluğun yazılacağı birim (metre / santimetre / milimetre / kilometre); varsayılan metre
        decimals — Virgülden sonraki basamak sayısı; varsayılan 2
        format — Yazının kalıbı; {} sayının yerini tutar (örnek: "{} m", "L={}")
        decimal_separator — Ondalık ayracı (virgul / nokta); varsayılan virgul
        side — Yazının kenarın hangi yanına düşeceği (otomatik / sol / sag / dis / ic); varsayılan otomatik
        height — Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0
        gap — Kenar ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0
        min_length — Bundan kısa kenarlara yazı yazılmaz, milimetre; varsayılan 0
        attach — Yazıyı kenarına bağla: kenar taşınınca yazı izler, uzunluk yeniden yazılır; varsayılan evet
    """

def number_vertices(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
    start: Coord = ...,
    direction: str = ...,
    prefix: str = ...,
    digits: int = ...,
    pad: str = ...,
    first_number: int = ...,
    suffix: str = ...,
    height: int = ...,
    gap: int = ...,
    attach: bool = ...,
) -> int:
    """Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler.

    Komut: islem.kose_numarala (KÖŞENUMARALA)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
        start — Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe [mm, Sağa (Y) önce]
        direction — Sayım yönü (ters / saat); varsayılan ters
        prefix — Numaranın önüne gelen yazı (örnek: A, K-)
        digits — Numaranın en az basamak sayısı; eksikler dolgu ile tamamlanır; varsayılan 0
        pad — Basamak dolgusu; varsayılan 0
        first_number — İlk köşenin numarası; varsayılan 1
        suffix — Numaranın arkasına gelen yazı
        height — Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0
        gap — Köşe ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0
        attach — Numarayı köşesine bağla: köşe taşınınca numara izler; varsayılan evet
    """

def detach(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
) -> int:
    """Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır.

    Komut: islem.bag_coz (BAĞÇÖZ)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
    """

def attach(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
    source: list[int] = ...,
    attach_to: str = ...,
    type: str = ...,
    unit: str = ...,
    decimals: int = ...,
    format: str = ...,
    decimal_separator: str = ...,
) -> int:
    """Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur.

    Komut: islem.bagla (BAĞLA)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
        source — Yazıların bağlanacağı nesne (çizgi ya da alan) [kalıcı nesne anahtarı]
        attach_to — Neye bağlanacağı: en yakın kenar ya da en yakın köşe (kenar / kose); varsayılan kenar
        type — Yazının sözü: kendi yazısı kalır ya da kenarın uzunluğu olur (sabit / uzunluk); varsayılan sabit
        unit — Uzunluğun birimi (tur=uzunluk) (metre / santimetre / milimetre / kilometre); varsayılan metre
        decimals — Virgülden sonraki basamak sayısı (tur=uzunluk); varsayılan 2
        format — Uzunluk yazısının kalıbı; {} sayının yerini tutar (tur=uzunluk)
        decimal_separator — Ondalık ayracı (tur=uzunluk) (virgul / nokta); varsayılan virgul
    """

def polygonize(
    *,
    objects: list[int] = ...,
    scope: str = ...,
    window: Coords = ...,
    layer: str = ...,
    islands: bool = ...,
    gap: float = ...,
) -> int:
    """Kapsamdaki çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki adalar delik olur, açık uçlar sayılıp gösterilir ve hiçbiri kendiliğinden kapanmaz.

    Komut: islem.alan_uret (ALANÜRET)
        objects — Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı]
        scope — secili (varsayılan), gorunum ya da proje: nesneler nereden alınır
        window — gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce]
        layer — Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman
        islands — Bir gözün içindeki kapalı çizgiler o alanın deliği olsun; kapalıysa göz dış sınırıyla dolu çizilir; varsayılan evet
        gap — Bu genişliğe kadar açık uçları köprüle, metre; 0: hiçbir boşluk kendiliğinden kapanmaz; varsayılan 0
    """

def fit(
    *,
    points: Coords = ...,
    scale_locked: bool = ...,
    crs: str = ...,
) -> int:
    """Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).

    Komut: core.fit (OTURT)
        points — Kontrol çiftleri: yerel, harita, yerel, harita... [mm, Sağa (Y) önce]
        scale_locked — Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez
        crs — Oturtulduktan sonraki koordinat sistemi, örnek TUREF/TM36
    """

def stakeout(
    *,
    station: Coord = ...,
    backsight: Coord = ...,
    objects: list[int] = ...,
) -> int:
    """İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).

    Komut: core.stakeout (APLİKASYON)
        station — Aletin durduğu nokta [mm, Sağa (Y) önce]
        backsight — Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür [mm, Sağa (Y) önce]
        objects — Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar [kalıcı nesne anahtarı]
    """

def reproject(
    *,
    target: str = ...,
    source: str = ...,
) -> int:
    """Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.

    Komut: core.reproject (DÖNÜŞTÜR)
        target — Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36
        source — Kaynak sistem; yoksa çizimin kendi koordinat sistemi
    """

def traverse(
    *,
    start: Coord = ...,
    backsight: Coord = ...,
    angle: list[float] = ...,
    distance: list[float] = ...,
    end: Coord = ...,
    end_backsight: Coord = ...,
    tolerance_class: str = ...,
    first_number: int = ...,
    distribution: str = ...,
    connect: bool = ...,
) -> int:
    """Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler.

    Komut: geodesy.traverse (POLİGON)
        start — Başlangıç istasyonu (bilinen) [mm, Sağa (Y) önce]
        backsight — Başlangıçtaki bağlama noktası (bilinen) [mm, Sağa (Y) önce]
        angle — Her istasyonda okunan kırılma açısı, ölçü karnesi sırasıyla
        distance — Her istasyondan sonraki kenar (m) [m]
        end — Bitiş istasyonu (bilinen); verilirse kapanma hesaplanır [mm, Sağa (Y) önce]
        end_backsight — Bitişteki bağlama noktası; açı kapanması için gerekir [mm, Sağa (Y) önce]
        tolerance_class — Tolerans sınıfı; katalogdan okunur
        first_number — İlk istasyonun nokta numarası; varsayılan 1
        distribution — Kenar kapanmasının dağıtımı: eşit ya da kenar orantılı
        connect — Güzergâhı çizgiyle bağlar; varsayılan evet
    """

def merge(
    *,
    objects: list[int] = ...,
) -> int:
    """Komşu parselleri tek parselde birleştirir (tevhit).

    Komut: core.merge (TEVHİT)
        objects — Birleştirilecek parseller; yoksa etkin seçim [kalıcı nesne anahtarı]
    """

def split_parcel(
    *,
    points: Coords = ...,
    objects: list[int] = ...,
) -> int:
    """Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).

    Komut: core.split_parcel (İFRAZ)
        points — Ayırma çizgisinin iki ucu [mm, Sağa (Y) önce]
        objects — Ayrılacak parsel; yoksa etkin seçim [kalıcı nesne anahtarı]
    """

def split_area(
    *,
    direction: Coords = ...,
    objects: list[int] = ...,
    area: int = ...,
    tolerance: int = ...,
) -> int:
    """Parselden verilen yöne paralel, istenen alanda bir parça ayırır.

    Komut: core.split_area (ALANİFRAZ)
        direction — Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır) [mm, Sağa (Y) önce]
        objects — Ayrılacak parsel; yoksa etkin seçim [kalıcı nesne anahtarı]
        area — Ayrılacak alan, mm² (400 m² = 400000000)
        tolerance — Kabul toleransı, mm²; varsayılan 10000 (0,01 m²)
    """

def topology(
    *,
    objects: list[int] = ...,
) -> int:
    """Kendini kesen sınır, sıfır alan ve örtüşen parselleri; yinelenen ve boş nesneleri, tekrarlanan köşeleri ve çizgi ağındaki boşlukları raporlar.

    Komut: core.topology (TOPOLOJİ)
        objects — Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim [kalıcı nesne anahtarı]
    """

def contour(
    *,
    interval: int = ...,
    layer: str = ...,
) -> int:
    """Kotlu noktalardan eş yükselti eğrileri çizer.

    Komut: core.contour (EŞYÜKSELTİ)
        interval — Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m)
        layer — Eğrilerin çizileceği katman; varsayılan ESYUKSELTI
    """

def earthwork(
    *,
    elevation: int = ...,
) -> int:
    """Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar.

    Komut: core.earthwork (HACİM)
        elevation — Karşılaştırma kotu, milimetre (845 m = 845000)
    """

def layers(
    *,
) -> int:
    """Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler.

    Komut: core.layers (KATMANLAR)
    """

def attr_schema(
    *,
) -> int:
    """Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler.

    Komut: core.attr_schema (ÖZNİTELİKŞEMASI)
    """

def query(
    *,
    layer: str = ...,
    field: str = ...,
    value: str = ...,
    limit: int = ...,
) -> int:
    """Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.

    Komut: core.query (SORGULA)
        layer — Hangi katmanda aranacağı; verilmezse bütün çizim
        field — Öznitelik sütunu; verilirse o sütunu taşıyan nesneler
        value — Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte
        limit — En çok kaç nesne bildirileceği; varsayılan 200
    """

def selection_info(
    *,
) -> int:
    """Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar.

    Komut: core.selection_info (SEÇİMBİLGİSİ)
    """

def object_points(
    *,
    objects: list[int] = ...,
    which: str = ...,
) -> int:
    """Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir; bir ajan bunları yeni çizimin taban noktası olarak kullanır.

    Komut: core.object_points (NESNENOKTALARI)
        objects — Noktaları istenen nesneler [kalıcı nesne anahtarı]
        which — Hangi noktalar: merkez (alanın ağırlık merkezi, çizginin uzunluk ortası, dairenin merkezi), köşeler, uçlar, kutunun köşeleri ya da kenar ortaları; varsayılan merkez
    """

def view_info(
    *,
) -> int:
    """Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir.

    Komut: core.view_info (GÖRÜNÜMBİLGİSİ)
    """

def context(
    *,
) -> int:
    """Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, seçili nesneler ve görünüm. Özet verir, döküm değil.

    Komut: core.context (BAĞLAM)
    """

def tool_search(
    *,
    query: str = ...,
    field: str = ...,
    limit: int = ...,
) -> int:
    """Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır.

    Komut: core.tool_search (ARAÇARA)
        query — Aranan sözcük; ad ve özet içinde Türkçe katlamayla eşleşir
        field — Nerede aranacağı: hepsi (öntanımlı), ad ya da ozet
        limit — En çok kaç sonuç gösterilsin; öntanımlı 20. Eşleşme sayısı her hâlde bildirilir
    """

def job_template(
    *,
    action: str = ...,
    template: str = ...,
) -> int:
    """Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç yüzeyinden gönderilir ve yazan her adım yine öneri olur.

    Komut: core.job_template (İŞŞABLONU)
        action — Ne yapılacağı: listele ya da goster
        template — Şablonun kimliği; goster için gerekir
    """

def suggestion(
    *,
    action: str = ...,
    suggestion: str = ...,
) -> int:
    """Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder.

    Komut: core.suggestion (ÖNERİ)
        action — Ne yapılacağı: uygula, reddet, durum ya da listele
        suggestion — Öneri kimliği; uygula, reddet ve durum için gerekir
    """

def mcp(
    *,
    action: str = ...,
    port: int = ...,
    name: str = ...,
) -> int:
    """Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler, yeni bir erişim belirteci üretir, bağlı istemcileri listeler ve tek bir istemcinin yetkisini kaldırır.

    Komut: core.mcp (MCPSUNUCU)
        action — Ne yapılacağı: baslat, durdur, durum, belirtec (yeni belirteç üretir), istemciler, iptal (bir istemcinin yetkisini kaldırır), izin (geri verir) ya da sina (bağlantıyı sınar)
        port — Yalnız bu başlatma için port; verilmezse ayardaki port
        name — İstemcinin adı; iptal ve izin için gerekir. Adları islem=istemciler ile görün
    """

def ai_provider(
    *,
    action: str = ...,
    name: str = ...,
    dialect: str = ...,
    endpoint: str = ...,
    path: str = ...,
    model: str = ...,
    key_ref: str = ...,
    context: int = ...,
    max_tokens: int = ...,
    temperature: float = ...,
    stream: bool = ...,
    thinking: bool = ...,
    tools: bool = ...,
) -> int:
    """Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar adını taşır.

    Komut: core.ai_provider (YAPAYZEKAMODELİ)
        action — Ne yapılacağı: listele, ekle, sil, varsayilan ya da dene (bağlantıyı dener)
        name — Profilin adı; ekle, sil, varsayilan ve dene için gerekir
        dialect — Uç noktanın konuştuğu telli dil; ekle için, varsayılan openai_chat
        endpoint — Uç noktanın adresi: http:// ya da https:// ile başlar, satıcının ön eki dahil
        path — Adresin altındaki uç nokta; '/' ile başlar: /chat/completions, /messages, /api/chat
        model — Model kimliği, uç noktanın yazdığı gibi
        key_ref — Anahtarı tutan kaydın adı — anahtar zincirindeki kayıt ya da bir ortam değişkeni (örnek: DEEPSEEK_API_KEY). Anahtarın kendisi buraya yazılmaz
        context — Bağlam penceresi, jeton; 0 bilinmiyor demektir
        max_tokens — Çıktı jeton sınırı; 0 demek 'bu alanı hiç gönderme'
        temperature — Örnekleme sıcaklığı, 0 ile 2 arasında; verilmezse hiç gönderilmez
        stream — Cevap parça parça mı istensin; varsayılan evet
        thinking — Modelin düşünme metni gösterilsin mi
        tools — Uç noktaya araç kataloğu gönderilsin mi; varsayılan evet
    """

