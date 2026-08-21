// SPDX-License-Identifier: GPL-3.0-or-later
// Entity kinds (.claude/model.md R22–R26) and attribute columns (R27–R29).
#include "microtest.hpp"

#include "piricad/core/attribute.hpp"
#include "piricad/core/entity_kind.hpp"
#include "piricad/core/geometry.hpp"

#include <array>
#include <span>
#include <string>
#include <vector>

using namespace piricad::core;

namespace {

// A 10 m x 10 m square as an exterior ring, and a two-vertex open polyline.
// Closure is implied, never stored: the kind's emit() flags the run closed and
// the closing segment is reconstructed from that (see polyline_emit).
constexpr Mm k10m = 10 * kMmPerMetre;

RingGeometry two_slot_geometry(std::uint32_t& parcel_out, std::uint32_t& line_out)
{
    RingGeometry g;

    const Point2 square[4]{{0, 0}, {k10m, 0}, {k10m, k10m}, {0, k10m}};
    RingGeometry::RingInput exterior{};
    exterior.points = square;
    exterior.role   = RingRole::Exterior;
    auto parcel     = g.append(std::span<const RingGeometry::RingInput>(&exterior, 1));
    CHECK(parcel.ok());
    parcel_out = parcel.value_or(0u);

    const Point2 open[3]{{0, 0}, {k10m, 0}, {k10m, k10m}};
    RingGeometry::RingInput line{};
    line.points   = open;
    line.role     = RingRole::Open;
    auto polyline = g.append(std::span<const RingGeometry::RingInput>(&line, 1));
    CHECK(polyline.ok());
    line_out = polyline.value_or(0u);

    return g;
}

/// A catalogue as /src/io would build one from a /data package: identity,
/// package version and codes, all from parsed text. No code list in C++.
Catalogue catalogue_from_data(const std::vector<std::string>& codes)
{
    Catalogue c("test.katalog", "1.2.3");
    for (const auto& code : codes)
        c.add_code(code);
    return c;
}

AttrSpec spec_from_data(std::string id, std::string name, AttrType type, bool required,
                        std::string catalog = {})
{
    AttrSpec s;
    s.id       = std::move(id);
    s.name_tr  = std::move(name);
    s.type     = type;
    s.required = required;
    s.catalog  = std::move(catalog);
    return s;
}

} // namespace

// ==================================================== entity kinds (R22-R26) ==

TEST_CASE("nesne türü kimliğe göre bulunur")
{
    const KindSpec* pl = builtin_kinds().find(1);
    CHECK(pl != nullptr);
    if (pl == nullptr) return;
    CHECK_EQ(std::string(pl->stable_id), std::string("core.polyline"));
    CHECK_EQ(pl->id, KindId{1});

    // R22: six function pointers, all present. A null one is an indirect call
    // through zero in the middle of a frame.
    CHECK(pl->bbox != nullptr && pl->emit != nullptr && pl->hit != nullptr);
    CHECK(pl->area != nullptr && pl->read != nullptr && pl->write != nullptr);

    CHECK(builtin_kinds().find(9999) == nullptr);
}

TEST_CASE("nesne türü Türkçe katlanmış adla bulunur")
{
    const KindTable& t = builtin_kinds();

    // The i/I pair is the whole reason folding is not std::toupper: 'çizgi'
    // upper-cases to 'ÇİZGİ' in Turkish, not 'CIZGI'.
    CHECK(t.find_name("çokluçizgi") == t.find(1));
    CHECK(t.find_name("ÇOKLUÇİZGİ") == t.find(1));
    CHECK(t.find_name("cokluCIZGI") == t.find(1));
    CHECK(t.find_name("POLYLINE") == t.find(1));
    CHECK(t.find_name("pl") == t.find(1));
    CHECK(t.find_name("parsel") == nullptr);

    // Not asserted, and deliberately so: turkish_upper() is an upper-caser, not
    // a case-folder, so a lower-case ASCII alias ("polyline") folds to
    // "POLYLİNE" and misses. Registry::find has the identical gap for "line".
    // The fix is one shared fold in the /src/command folding table that both
    // lookups use — core.md P6 puts that table there, not here, and a second
    // folding rule in core is exactly what P6 forbids.
}

TEST_CASE("builtin_kinds her çağrıda aynı tabloyu verir")
{
    // R24 / core.md P8: immutable const state, not a registry. Two references to
    // the same object, so no caller can be handed a table someone else mutated.
    CHECK(&builtin_kinds() == &builtin_kinds());
    CHECK_EQ(builtin_kinds().size(), std::size_t{1});
    CHECK_EQ(std::string(builtin_kinds().all()[0].stable_id), std::string("core.polyline"));
}

TEST_CASE("eksik ya da çakışan tür bildirimi reddedilir")
{
    KindTable t;
    CHECK(t.add(piricad_kind_polyline()).ok());

    // Same id twice.
    CHECK(!t.add(piricad_kind_polyline()).ok());

    KindSpec renamed = piricad_kind_polyline();
    renamed.id       = 2;
    // Names still collide, and a name that resolves to two kinds makes the
    // command line ambiguous.
    CHECK(!t.add(renamed).ok());

    KindSpec missing_fn = piricad_kind_polyline();
    missing_fn.id       = 3;
    missing_fn.names[0] = "TEST_TÜR";
    missing_fn.names[1] = nullptr;
    missing_fn.names[2] = nullptr;
    missing_fn.names[3] = nullptr;
    missing_fn.hit      = nullptr;
    const auto st       = t.add(missing_fn);
    CHECK(!st.ok());
    CHECK_EQ(static_cast<int>(st.error().code), static_cast<int>(ErrorCode::InvalidArgument));

    KindSpec no_stable_id  = piricad_kind_polyline();
    no_stable_id.id        = 4;
    no_stable_id.stable_id = "";
    CHECK(!t.add(no_stable_id).ok());

    CHECK_EQ(t.size(), std::size_t{1});
}

TEST_CASE("KindSpec ek alanlara açık kalır")
{
    // The leading size field is what makes the record additive-only across a
    // later ABI boundary (.claude/plugin-api.md).
    CHECK_EQ(piricad_kind_polyline().size, static_cast<std::uint32_t>(sizeof(KindSpec)));
}

TEST_CASE("çokluçizgi türü sınır kutusunu ve alanı geometriyle aynı hesaplar")
{
    std::uint32_t parcel = 0, line = 0;
    const RingGeometry g = two_slot_geometry(parcel, line);

    const KindSpec* pl = builtin_kinds().find(1);
    CHECK(pl != nullptr);
    if (pl == nullptr) return;

    // R23: one dispatch for the whole batch, not one per entity.
    const std::array<std::uint32_t, 2> slots{parcel, line};
    std::array<Box2, 2> boxes{};
    std::array<Mm2, 2> areas{};
    pl->bbox(g, slots, boxes);
    pl->area(g, slots, areas);

    CHECK(boxes[0] == g.bounds_of(parcel));
    CHECK(boxes[1] == g.bounds_of(line));
    CHECK_EQ(areas[0], g.area_of(parcel));
    CHECK_EQ(areas[1], g.area_of(line));

    // 10 m x 10 m = 100 m² = 1e8 mm². Alan hesabı is the legal output (R12).
    CHECK_EQ(areas[0], Mm2{100} * kMmPerMetre * kMmPerMetre);
    // An open ring encloses nothing.
    CHECK_EQ(areas[1], Mm2{0});

    CHECK(boxes[0] == (Box2{0, 0, k10m, k10m}));
}

TEST_CASE("çokluçizgi türü isabet testi toleransa uyar")
{
    std::uint32_t parcel = 0, line = 0;
    const RingGeometry g = two_slot_geometry(parcel, line);
    const KindSpec* pl   = builtin_kinds().find(1);
    CHECK(pl != nullptr);
    if (pl == nullptr) return;

    const std::array<std::uint32_t, 1> slots{line};
    std::array<std::uint8_t, 1> hits{};

    // 100 mm off the first segment.
    pl->hit(g, slots, Point2{5 * kMmPerMetre, 100}, 150, hits);
    CHECK_EQ(hits[0], std::uint8_t{1});

    pl->hit(g, slots, Point2{5 * kMmPerMetre, 100}, 50, hits);
    CHECK_EQ(hits[0], std::uint8_t{0});

    // The parcel's implied closing segment is part of it: a probe on the left
    // edge, which exists only because the ring is closed, must hit.
    const std::array<std::uint32_t, 1> parcel_slot{parcel};
    pl->hit(g, parcel_slot, Point2{0, 5 * kMmPerMetre}, 10, hits);
    CHECK_EQ(hits[0], std::uint8_t{1});
}

TEST_CASE("çokluçizgi türü çizim akışını halka halka üretir")
{
    std::uint32_t parcel = 0, line = 0;
    const RingGeometry g = two_slot_geometry(parcel, line);
    const KindSpec* pl   = builtin_kinds().find(1);
    CHECK(pl != nullptr);
    if (pl == nullptr) return;

    const std::array<std::uint32_t, 2> slots{parcel, line};
    EmitBuffer buf;
    pl->emit(g, slots, buf);

    CHECK_EQ(buf.run_total(), std::size_t{2});
    CHECK_EQ(buf.run_count[0], std::uint32_t{4});
    CHECK_EQ(buf.run_closed[0], std::uint8_t{1});
    CHECK_EQ(buf.run_count[1], std::uint32_t{3});
    CHECK_EQ(buf.run_closed[1], std::uint8_t{0});
    CHECK_EQ(buf.xs.size(), std::size_t{7});
    CHECK_EQ(buf.xs[buf.run_start[1]], Mm{0});
}

TEST_CASE("çokluçizgi yükü aynen geri okunur")
{
    std::uint32_t parcel = 0, line = 0;
    const RingGeometry g = two_slot_geometry(parcel, line);
    const KindSpec* pl   = builtin_kinds().find(1);
    CHECK(pl != nullptr);
    if (pl == nullptr) return;

    const std::array<std::uint32_t, 2> slots{parcel, line};
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint32_t> ends;
    pl->write(g, slots, bytes, ends);
    CHECK_EQ(ends.size(), std::size_t{2});

    RingGeometry back;
    std::uint32_t from = 0;
    for (std::size_t i = 0; i < ends.size(); ++i) {
        const std::span<const std::uint8_t> payload(bytes.data() + from, ends[i] - from);
        auto slot = pl->read(back, payload);
        CHECK(slot.ok());
        if (!slot) return;
        CHECK_EQ(slot.value(), static_cast<std::uint32_t>(i));
        from = ends[i];
    }

    // R26: what came off disk is what went on it, geometry for geometry.
    CHECK(back.bounds_of(0) == g.bounds_of(parcel));
    CHECK_EQ(back.area_of(0), g.area_of(parcel));
    CHECK(back.bounds_of(1) == g.bounds_of(line));
    CHECK_EQ(back.area_of(1), g.area_of(line));

    // Re-writing the round-tripped geometry must produce the same bytes.
    std::vector<std::uint8_t> again;
    std::vector<std::uint32_t> again_ends;
    const std::array<std::uint32_t, 2> back_slots{0, 1};
    pl->write(back, back_slots, again, again_ends);
    CHECK(again == bytes);
}

TEST_CASE("bozuk çokluçizgi yükü hata döndürür")
{
    const KindSpec* pl = builtin_kinds().find(1);
    CHECK(pl != nullptr);
    if (pl == nullptr) return;

    RingGeometry g;

    // Empty: not even a ring count.
    const auto empty = pl->read(g, {});
    CHECK(!empty.ok());
    CHECK_EQ(static_cast<int>(empty.error().code), static_cast<int>(ErrorCode::ParseError));

    // A ring count no file could satisfy — refused before anything is reserved
    // for it, because a hostile DWG will claim four billion rings.
    const std::array<std::uint8_t, 4> huge{0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(!pl->read(g, huge).ok());

    // One ring declaring more vertices than the payload holds.
    std::vector<std::uint8_t> truncated{1, 0, 0, 0,  // ring count
                                        1, 0, 0,     // role Exterior, part 0
                                        4, 0, 0, 0}; // 4 vertices, none present
    CHECK(!pl->read(g, truncated).ok());

    // An unknown ring role is a format the running build does not understand.
    std::vector<std::uint8_t> bad_role{1, 0, 0, 0, 77, 0, 0, 0, 0, 0, 0};
    CHECK(!pl->read(g, bad_role).ok());

    CHECK_EQ(g.slot_count(), std::size_t{0});
}

// ================================================ attribute columns (R27-R29) ==

TEST_CASE("metin sütunu sözlükle kodlanır")
{
    AttrColumn mahalle(spec_from_data("mahalle_adi", "Mahalle adı", AttrType::Text, false));
    mahalle.resize(5);

    // Five parcels, two mahalle. A cadastral layer has five million rows and
    // perhaps two hundred distinct names; that ratio is why the pool exists.
    CHECK(mahalle.set(0, attr_text("Karşıyaka")).ok());
    CHECK(mahalle.set(1, attr_text("Bostanlı")).ok());
    CHECK(mahalle.set(2, attr_text("Karşıyaka")).ok());
    CHECK(mahalle.set(3, attr_text("Bostanlı")).ok());
    CHECK(mahalle.set(4, attr_text("Karşıyaka")).ok());

    CHECK_EQ(mahalle.pool_size(), std::size_t{2});
    CHECK_EQ(mahalle.rows(), std::size_t{5});
    CHECK_EQ(std::string(mahalle.text(4)), std::string("Karşıyaka"));

    // A numeric column interns nothing.
    AttrColumn ada(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false));
    ada.resize(3);
    CHECK(ada.set(0, attr_int64(1234)).ok());
    CHECK_EQ(ada.pool_size(), std::size_t{0});
}

TEST_CASE("öznitelik yazımı önceki değeri döndürür")
{
    // R28: column, row, previous value is everything an Op needs. Adding a new
    // attribute must add ZERO Op variants, and it can only do that if the undo
    // record is this generic.
    AttrColumn ada(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false));
    ada.resize(2);

    auto first = ada.set(0, attr_int64(101));
    CHECK(first.ok());
    CHECK(!first.value().present); // nothing was there
    CHECK_EQ(static_cast<int>(first.value().type), static_cast<int>(AttrType::Int64));

    auto second = ada.set(0, attr_int64(202));
    CHECK(second.ok());
    CHECK(second.value().present);
    CHECK_EQ(second.value().number, std::int64_t{101});

    // Replaying the returned previous value restores the cell exactly — this is
    // undo, expressed with no knowledge of what an ada number is.
    CHECK(ada.set(0, second.value()).ok());
    CHECK_EQ(ada.get(0).value().number, std::int64_t{101});

    // Clearing is the same operation.
    auto cleared = ada.set(0, attr_absent(AttrType::Int64));
    CHECK(cleared.ok());
    CHECK_EQ(cleared.value().number, std::int64_t{101});
    CHECK(!ada.present(0));
    CHECK(!ada.get(0).value().present);
}

TEST_CASE("öznitelik yazımı tür ve satır sınırlarını doğrular")
{
    AttrColumn ada(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false));
    ada.resize(2);

    const auto wrong_type = ada.set(0, attr_text("yüz bir"));
    CHECK(!wrong_type.ok());
    CHECK_EQ(static_cast<int>(wrong_type.error().code),
             static_cast<int>(ErrorCode::InvalidArgument));
    // An actionable message names both types (result.hpp).
    CHECK(wrong_type.error().message.find("Int64") != std::string::npos);

    const auto no_row = ada.set(7, attr_int64(1));
    CHECK(!no_row.ok());
    CHECK_EQ(static_cast<int>(no_row.error().code), static_cast<int>(ErrorCode::NotFound));
    CHECK(!ada.get(7).ok());

    // Length is Mm, so it is an integer column and never a double (R21, P8).
    AttrColumn cephe(spec_from_data("cephe", "Cephe", AttrType::Length, false));
    cephe.resize(1);
    CHECK(cephe.set(0, attr_mm(mm_from_metres(12.345))).ok());
    CHECK_EQ(cephe.get(0).value().number, std::int64_t{12345});
    CHECK(!cephe.set(0, attr_int64(5)).ok());
}

TEST_CASE("boy değiştirme silinen hücreyi geri getirmez")
{
    AttrColumn ada(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false));
    ada.resize(100);
    CHECK(ada.set(99, attr_int64(7)).ok());
    CHECK(ada.present(99));

    ada.resize(10);
    CHECK(!ada.present(99));
    ada.resize(100);
    CHECK(!ada.present(99));
    CHECK(!ada.get(99).value().present);
}

TEST_CASE("zorunlu öznitelik boşsa satır geçersizdir")
{
    AttrTable t;
    const auto ada = t.add(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, true));
    CHECK(ada.ok());
    const auto note = t.add(spec_from_data("aciklama", "Açıklama", AttrType::Text, false));
    CHECK(note.ok());
    t.resize(1);

    const auto missing = t.validate_required(0);
    CHECK(!missing.ok());
    CHECK_EQ(static_cast<int>(missing.error().code), static_cast<int>(ErrorCode::ValidationFailed));
    CHECK(missing.error().message.find("Ada numarası") != std::string::npos);

    CHECK(t.set(ada.value_or(kNoAttr), 0, attr_int64(1234)).ok());
    CHECK(t.validate_required(0).ok());

    // The optional column stays empty and nobody minds.
    CHECK(!t.get(note.value_or(kNoAttr), 0).value().present);

    // A row that does not exist is a different failure from an empty cell.
    CHECK_EQ(static_cast<int>(t.validate_required(1).error().code),
             static_cast<int>(ErrorCode::NotFound));
}

TEST_CASE("katalog kodu çalışma anında yüklenen katalogla doğrulanır")
{
    // The catalogue is built here from data, exactly as /src/io builds one from
    // a /data package. A code list in C++ is banned (CLAUDE.md 5.13).
    CatalogueSet catalogues;
    catalogues.add(catalogue_from_data({"3110", "3120", "3200"}));

    AttrTable t;
    const auto code =
        t.add(spec_from_data("kod", "Katalog kodu", AttrType::CodeRef, true, "test.katalog"));
    CHECK(code.ok());
    t.resize(2);

    const AttrId col = code.value_or(kNoAttr);
    CHECK(t.set(col, 0, attr_code("3120")).ok());
    CHECK(t.validate_row(0, catalogues).ok());

    CHECK(t.set(col, 1, attr_code("9999")).ok());
    const auto unknown = t.validate_codes(1, catalogues);
    CHECK(!unknown.ok());
    CHECK_EQ(static_cast<int>(unknown.error().code), static_cast<int>(ErrorCode::ValidationFailed));
    // R35: the message names the package version the code was checked against —
    // "not in the catalogue" is only answerable against a stated version.
    CHECK(unknown.error().message.find("1.2.3") != std::string::npos);

    // No catalogue loaded at all: loud, not a silent pass (data.md R6).
    const CatalogueSet empty;
    const auto no_catalogue = t.validate_codes(0, empty);
    CHECK(!no_catalogue.ok());
    CHECK_EQ(static_cast<int>(no_catalogue.error().code), static_cast<int>(ErrorCode::NotFound));

    // A retired code is still a valid code: a withdrawal must not retroactively
    // invalidate a signed document (data.md R5).
    Catalogue with_retired = catalogue_from_data({"3110", "3120", "3200"});
    with_retired.add_code("3120"); // duplicate load is a no-op
    CHECK_EQ(with_retired.size(), std::size_t{3});
}

TEST_CASE("katalog kodu sütunu kataloğunu bildirmek zorunda")
{
    AttrTable t;
    CHECK(!t.add(spec_from_data("kod", "Katalog kodu", AttrType::CodeRef, true)).ok());
    CHECK(!t.add(spec_from_data("", "Adsız", AttrType::Int64, false)).ok());
    CHECK(t.add(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false)).ok());
    CHECK(!t.add(spec_from_data("ada_no", "Ada numarası (kopya)", AttrType::Int64, false)).ok());
    CHECK_EQ(t.columns(), std::size_t{1});
    CHECK_EQ(t.find("ada_no"), AttrId{0});
    CHECK_EQ(t.find("parsel_no"), kNoAttr);
    CHECK(t.column(kNoAttr) == nullptr);
}

TEST_CASE("öznitelik parmak izi içerikten belirlenir")
{
    const AttrSpec spec = spec_from_data("mahalle_adi", "Mahalle adı", AttrType::Text, false);

    // Same content, different interning order: the pool ids differ, the hash
    // must not. A hash that depended on insertion order would make a golden
    // fixture depend on the order the user typed (test.md R19).
    AttrColumn a(spec), b(spec);
    a.resize(3);
    b.resize(3);
    CHECK(a.set(0, attr_text("Bostanlı")).ok());
    CHECK(a.set(1, attr_text("Karşıyaka")).ok());
    CHECK(a.set(2, attr_text("Bostanlı")).ok());
    CHECK(b.set(2, attr_text("Bostanlı")).ok());
    CHECK(b.set(1, attr_text("Karşıyaka")).ok());
    CHECK(b.set(0, attr_text("Bostanlı")).ok());
    CHECK_EQ(a.fold(0), b.fold(0));

    // Turkish is not case-folded here: 'Bostanlı' and 'BOSTANLI' are different
    // values, and only lookup folds (core.md P6).
    AttrColumn c(spec);
    c.resize(3);
    CHECK(c.set(0, attr_text("BOSTANLI")).ok());
    CHECK(c.set(1, attr_text("Karşıyaka")).ok());
    CHECK(c.set(2, attr_text("Bostanlı")).ok());
    CHECK(a.fold(0) != c.fold(0));

    // An empty cell is not a zero: "ada numarası kayıtlı değil" and "ada
    // numarası 0" are different facts about a parcel.
    const AttrSpec num = spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false);
    AttrColumn absent(num), zero(num);
    absent.resize(1);
    zero.resize(1);
    CHECK(zero.set(0, attr_int64(0)).ok());
    CHECK(absent.fold(0) != zero.fold(0));

    // Bool is normalised on the way in, so two documents that recorded "true"
    // as 1 and as 7 agree.
    const AttrSpec flag = spec_from_data("tescilli", "Tescilli", AttrType::Bool, false);
    AttrColumn one(flag), seven(flag);
    one.resize(1);
    seven.resize(1);
    AttrValue raw = attr_bool(true);
    raw.number    = 7;
    CHECK(one.set(0, attr_bool(true)).ok());
    CHECK(seven.set(0, raw).ok());
    CHECK_EQ(one.fold(0), seven.fold(0));
}

TEST_CASE("öznitelik tablosu satır sayısını sütunlara yayar")
{
    AttrTable t;
    CHECK(t.add(spec_from_data("ada_no", "Ada numarası", AttrType::Int64, false)).ok());
    t.resize(4);
    CHECK(t.add(spec_from_data("parsel_no", "Parsel numarası", AttrType::Int64, false)).ok());

    // A column added after the rows exist is still full length, or the second
    // attribute of a five-million-row layer would index out of bounds.
    CHECK_EQ(t.column(1)->rows(), std::size_t{4});
    CHECK(t.set(1, 3, attr_int64(9)).ok());
    CHECK_EQ(t.rows(), std::size_t{4});

    const auto bad_column = t.set(9, 0, attr_int64(1));
    CHECK(!bad_column.ok());
    CHECK_EQ(static_cast<int>(bad_column.error().code), static_cast<int>(ErrorCode::NotFound));
}
