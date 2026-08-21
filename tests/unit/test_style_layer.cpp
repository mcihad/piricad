// SPDX-License-Identifier: GPL-3.0-or-later
// Style interning and the layer table — .claude/model.md R13–R19, R30–R34.
#include "microtest.hpp"

#include "piricad/core/layer.hpp"
#include "piricad/core/style.hpp"
#include "piricad/core/text.hpp"

#include <vector>

using namespace piricad::core;

namespace {

/// Fully explicit, and every field distinct from its default, so that a variant
/// differing in one field really does differ in one field.
Appearance explicit_base()
{
    Appearance a;
    a.rgba       = 0xFF112233u;
    a.width_um   = 350;
    a.dash       = 4;
    a.symbol     = 9;
    a.fill_rgba  = 0x80445566u;
    a.hatch      = 7;
    a.z_order    = -3;
    a.src_colour = Source::Explicit;
    a.src_width  = Source::Explicit;
    a.src_dash   = Source::Explicit;
    a.src_fill   = Source::Explicit;
    return a;
}

Appearance layer_base()
{
    Appearance a;
    a.rgba      = 0xFFAABBCCu;
    a.width_um  = 1200;
    a.dash      = 11;
    a.symbol    = 21;
    a.fill_rgba = 0xFF223344u;
    a.hatch     = 13;
    a.z_order   = 42;
    return a;
}

Layer named(std::string name)
{
    Layer l;
    l.name = std::move(name);
    return l;
}

} // namespace

// ------------------------------------------------------------- StyleTable ---

TEST_CASE("StyleTable: entry 0 is the ByLayer sentinel and costs nothing")
{
    StyleTable t;
    CHECK_EQ(t.size(), std::size_t{1});
    CHECK_EQ(t.at(kByLayerStyle), Appearance{});

    // R13: an entity that inherits everything from its layer must intern to 0,
    // because that is the overwhelmingly common case in a cadastral drawing.
    CHECK_EQ(t.intern(Appearance{}), kByLayerStyle);
    CHECK_EQ(t.size(), std::size_t{1});
}

TEST_CASE("StyleTable: interning dedups and hands out ids in first-seen order")
{
    StyleTable t;
    const Appearance a = explicit_base();
    Appearance b       = explicit_base();
    b.rgba             = 0xFF000000u;

    const StyleId ia = t.intern(a);
    const StyleId ib = t.intern(b);
    CHECK_EQ(ia, StyleId{1});
    CHECK_EQ(ib, StyleId{2});
    CHECK_EQ(t.size(), std::size_t{3});

    CHECK_EQ(t.intern(a), ia);
    CHECK_EQ(t.intern(b), ib);
    CHECK_EQ(t.size(), std::size_t{3});

    CHECK_EQ(t.at(ia), a);
    CHECK_EQ(t.at(ib), b);
}

TEST_CASE("StyleTable: the same intern sequence yields the same ids in every run")
{
    // Golden fixtures store the id, not the appearance (§7.3). If interning ever
    // depended on an address or on hash-map iteration order this would drift.
    std::vector<Appearance> seq;
    for (int i = 0; i < 64; ++i) {
        Appearance a = explicit_base();
        a.width_um   = 100 + i;
        a.z_order    = static_cast<std::int16_t>(i - 32);
        seq.push_back(a);
    }

    StyleTable first;
    StyleTable second;
    for (const auto& a : seq)
        CHECK_EQ(first.intern(a), second.intern(a));
    CHECK_EQ(first.fold(0), second.fold(0));
}

TEST_CASE("StyleTable: every Appearance field is part of the identity")
{
    const Appearance base = explicit_base();

    std::vector<Appearance> variants;
    auto vary = [&](auto mutate) {
        Appearance a = base;
        mutate(a);
        variants.push_back(a);
    };

    vary([](Appearance& a) { a.rgba = 0xFF999999u; });
    vary([](Appearance& a) { a.width_um = 351; });
    vary([](Appearance& a) { a.dash = 5; });
    vary([](Appearance& a) { a.symbol = 10; });
    vary([](Appearance& a) { a.fill_rgba = 0x80445567u; });
    vary([](Appearance& a) { a.hatch = 8; });
    vary([](Appearance& a) { a.z_order = -2; });
    vary([](Appearance& a) { a.src_colour = Source::ByLayer; });
    vary([](Appearance& a) { a.src_width = Source::ByLayer; });
    vary([](Appearance& a) { a.src_dash = Source::ByLayer; });
    vary([](Appearance& a) { a.src_fill = Source::ByLayer; });

    StyleTable t;
    const StyleId id_base = t.intern(base);
    for (const auto& v : variants)
        CHECK(t.intern(v) != id_base);

    // Sentinel + base + one id per varied field, none of them collapsed.
    CHECK_EQ(t.size(), std::size_t{2} + variants.size());

    // The loop above proves nothing about the HASH: intern() looks the appearance
    // up in an unordered_map, so a distinct id comes from operator==, not from
    // fold_appearance. A fold that hashed only rgba would still hand out twelve
    // distinct ids — pure collisions — and pass every assertion above, while a
    // document whose çizgi kalınlığı, dolgu rengi or tarama deseni changed would
    // fingerprint identically. So drive the coverage through fold(), in separate
    // tables so the id is the same on both sides and only the content differs.
    for (std::size_t i = 0; i < variants.size(); ++i) {
        StyleTable only_base;
        StyleTable only_variant;
        only_base.intern(base);
        only_variant.intern(variants[i]);
        if (only_base.fold(0) == only_variant.fold(0))
            ::microtest::report(__FILE__, __LINE__, "Appearance alanı parmak izine girmiyor",
                                std::string("varyant #") + std::to_string(i));
    }
}

TEST_CASE("StyleTable: an out-of-range id degrades to entry 0, it does not crash")
{
    StyleTable t;
    t.intern(explicit_base());

    CHECK(t.contains(kByLayerStyle));
    CHECK(t.contains(StyleId{1}));
    CHECK(!t.contains(StyleId{2}));

    // A corrupt style column must not be able to read past the end mid-frame.
    CHECK_EQ(t.at(StyleId{2}), Appearance{});
    CHECK_EQ(t.at(0xFFFFFFFFu), Appearance{});
}

TEST_CASE("StyleTable: fold reacts to content and to id order, and to nothing else")
{
    StyleTable empty_a;
    StyleTable empty_b;
    CHECK_EQ(empty_a.fold(0), empty_b.fold(0));

    // Idempotent: folding twice without touching the table gives the same answer.
    CHECK_EQ(empty_a.fold(0), empty_a.fold(0));
    CHECK(empty_a.fold(0) != empty_a.fold(1));

    Appearance a = explicit_base();
    Appearance b = explicit_base();
    b.z_order    = 100;

    StyleTable forward;
    forward.intern(a);
    forward.intern(b);

    StyleTable reversed;
    reversed.intern(b);
    reversed.intern(a);

    // Same entries, different ids — and the id is what every entity stores.
    CHECK(forward.fold(0) != reversed.fold(0));

    StyleTable one;
    one.intern(a);
    CHECK(one.fold(0) != forward.fold(0));
}

// ----------------------------------------------------------- KeyAllocator ---
//
// model.md Enforcement names "key monotonicity and non-reuse" as a /tests/unit
// responsibility. The allocator had no direct test at all: it appeared only as an
// argument to LayerTable::add, and an allocator that started at 0, that wrapped
// past 2^63-1, or that reissued after adopt_* passed the whole suite. A reused key
// makes "which parcel was this?" unanswerable, and R4 calls that a legal question.

TEST_CASE("R4: anahtarlar kesin artan, hiçbiri None değil")
{
    KeyAllocator keys;

    std::uint64_t previous = 0;
    for (int i = 0; i < 1000; ++i) {
        const EntityKey e = keys.mint_entity();
        CHECK(e != EntityKey::None);
        CHECK(raw(e) > previous);
        previous = raw(e);
    }
    // Zero is reserved for "none", so minting starts at one and never returns it.
    CHECK_EQ(previous, std::uint64_t{1000});

    // The two spaces are independent: an entity key and a layer key may collide
    // numerically because they are different types and never compared.
    KeyAllocator other;
    CHECK_EQ(raw(other.mint_layer()), std::uint64_t{1});
    CHECK_EQ(raw(other.mint_entity()), std::uint64_t{1});
}

TEST_CASE("R4: adopt ileri sarar, geri sarmaz")
{
    KeyAllocator keys;
    CHECK(keys.adopt_entity(static_cast<EntityKey>(5000)));
    CHECK_EQ(raw(keys.mint_entity()), std::uint64_t{5001});

    // A LOWER adopted value must not rewind the counter, or the next mint would
    // reissue a key the document is already using.
    CHECK(keys.adopt_entity(static_cast<EntityKey>(12)));
    CHECK_EQ(raw(keys.mint_entity()), std::uint64_t{5002});

    CHECK(keys.adopt_layer(static_cast<LayerKey>(9)));
    CHECK_EQ(raw(keys.mint_layer()), std::uint64_t{10});
}

TEST_CASE("R3: anahtar alanı 2^63-1'de biter — tükenme bildirilir, sarılmaz")
{
    // R3 caps the space because command::Value::Kind::IdList is
    // std::vector<std::int64_t>: a key above this silently becomes NEGATIVE in the
    // journal. The cap is pinned here so a later "u64 is u64" simplification has
    // to argue with the journal.
    CHECK_EQ(kMaxKey, (std::uint64_t{1} << 63) - 1);

    KeyAllocator keys;
    keys.seek_entity(kMaxKey);
    const EntityKey last = keys.mint_entity();
    CHECK_EQ(raw(last), kMaxKey); // the last valid key IS handed out
    CHECK(keys.mint_entity() == EntityKey::None);
    CHECK(keys.mint_entity() == EntityKey::None); // and stays exhausted

    keys.seek_layer(kMaxKey);
    CHECK_EQ(raw(keys.mint_layer()), kMaxKey);
    CHECK(keys.mint_layer() == LayerKey::None);
}

TEST_CASE("R3/R4: aralık dışı bir anahtar benimsenmez, sayaç sarılmaz")
{
    // The failure this pins: adopt_entity(0xFFFFFFFFFFFFFFFF) used to set the
    // counter to 0, so the next mint returned EntityKey{0} — indistinguishable
    // from None, i.e. a FALSE exhaustion report — and every mint after that handed
    // out 1, 2, 3: keys the file had already used. Reached from untrusted input.
    KeyAllocator keys;
    CHECK(!keys.adopt_entity(static_cast<EntityKey>(0xFFFFFFFFFFFFFFFFull)));
    CHECK(keys.mint_entity() == EntityKey::None); // exhausted, not wrapped to 0

    KeyAllocator at_cap;
    CHECK(at_cap.adopt_entity(static_cast<EntityKey>(kMaxKey))); // kMaxKey is legal
    CHECK(at_cap.mint_entity() == EntityKey::None);

    KeyAllocator layers;
    CHECK(!layers.adopt_layer(static_cast<LayerKey>(kMaxKey + 1)));
    CHECK(layers.mint_layer() == LayerKey::None);
}

TEST_CASE("R4: anahtarı tükenmiş tabloya katman eklenemez, uydurulmaz")
{
    LayerTable t;
    KeyAllocator keys;
    keys.seek_layer(kMaxKey + 1);

    const auto refused = t.add(named("parsel"), keys);
    CHECK(!refused.ok());
    if (!refused.ok())
        CHECK_EQ(static_cast<int>(refused.error().code), static_cast<int>(ErrorCode::Internal));
    CHECK_EQ(t.size(), std::size_t{1}); // nothing was added under a bogus key
}

// ------------------------------------------------------ resolve_appearance ---

TEST_CASE("resolve_appearance: ByLayer takes every property from the layer")
{
    Appearance own; // default is ByLayer on all four properties
    own.symbol  = 5;
    own.z_order = -1;

    const Appearance layer = layer_base();
    const Appearance r     = resolve_appearance(own, layer);

    CHECK_EQ(r.rgba, layer.rgba);
    CHECK_EQ(r.width_um, layer.width_um);
    CHECK_EQ(r.dash, layer.dash);
    CHECK_EQ(r.fill_rgba, layer.fill_rgba);
    CHECK_EQ(r.hatch, layer.hatch);

    // symbol and z_order have no Source of their own, so they never cascade.
    CHECK_EQ(r.symbol, std::uint16_t{5});
    CHECK_EQ(r.z_order, std::int16_t{-1});
}

TEST_CASE("resolve_appearance: Explicit keeps the entity's own property")
{
    const Appearance own   = explicit_base();
    const Appearance layer = layer_base();
    const Appearance r     = resolve_appearance(own, layer);

    CHECK_EQ(r.rgba, own.rgba);
    CHECK_EQ(r.width_um, own.width_um);
    CHECK_EQ(r.dash, own.dash);
    CHECK_EQ(r.fill_rgba, own.fill_rgba);
    CHECK_EQ(r.hatch, own.hatch);
}

TEST_CASE("resolve_appearance: the cascade is per property, not per record")
{
    Appearance own = explicit_base();
    own.src_width  = Source::ByLayer;
    own.src_fill   = Source::ByLayer;

    const Appearance layer = layer_base();
    const Appearance r     = resolve_appearance(own, layer);

    CHECK_EQ(r.rgba, own.rgba);           // Explicit
    CHECK_EQ(r.dash, own.dash);           // Explicit
    CHECK_EQ(r.width_um, layer.width_um); // ByLayer
    CHECK_EQ(r.fill_rgba, layer.fill_rgba);
    CHECK_EQ(r.hatch, layer.hatch);
}

TEST_CASE("resolve_appearance: ByBlock resolves as ByLayer until blocks exist")
{
    Appearance own = explicit_base();
    own.src_colour = Source::ByBlock;
    own.src_width  = Source::ByBlock;
    own.src_dash   = Source::ByBlock;
    own.src_fill   = Source::ByBlock;

    Appearance by_layer = explicit_base();
    by_layer.src_colour = Source::ByLayer;
    by_layer.src_width  = Source::ByLayer;
    by_layer.src_dash   = Source::ByLayer;
    by_layer.src_fill   = Source::ByLayer;

    const Appearance layer = layer_base();
    CHECK_EQ(resolve_appearance(own, layer), resolve_appearance(by_layer, layer));
}

TEST_CASE("resolve_appearance: the result is resolved, so every Source is Explicit")
{
    const Appearance r = resolve_appearance(Appearance{}, layer_base());
    CHECK_EQ(static_cast<int>(r.src_colour), static_cast<int>(Source::Explicit));
    CHECK_EQ(static_cast<int>(r.src_width), static_cast<int>(Source::Explicit));
    CHECK_EQ(static_cast<int>(r.src_dash), static_cast<int>(Source::Explicit));
    CHECK_EQ(static_cast<int>(r.src_fill), static_cast<int>(Source::Explicit));

    // R14: resolving twice is a no-op, so a style column can be re-resolved by a
    // later transaction without drifting.
    CHECK_EQ(resolve_appearance(r, layer_base()), r);

    // And the result is MATERIALISED, not merely consistent: re-resolving it
    // against a DIFFERENT layer must change nothing. Re-resolving against the same
    // layer, as the line above does, cannot tell a materialised value from a
    // resolver that is still consulting the layer every time — which is exactly
    // the difference R14 exists to make: the renderer reads one u32 and evaluates
    // no cascade.
    Appearance other = layer_base();
    other.rgba       = 0xFF000001u;
    other.width_um   = 4321;
    other.dash       = 31;
    other.fill_rgba  = 0x11223344u;
    other.hatch      = 29;
    CHECK_EQ(resolve_appearance(r, other), r);
}

TEST_CASE("R14: sınıflandırma kuralı adımı henüz yok — sıra iki adımla sınanıyor")
{
    // R14 declares a THREE-step order: explicit style[e], else the LAYER'S
    // CLASSIFICATION RULE over the entity's attributes (the GIS renderer), else
    // the layer's own appearance. The cases above test steps 1 and 3 only.
    //
    // Step 2 has no implementation to test: R32 lists a "classification rule
    // reference" among a layer's fields and `Layer` carries none, so there is
    // nothing for a rule to be attached to. This case exists so the gap is
    // VISIBLE in the suite instead of being implied by a green file that reads as
    // if R14 were covered. It will assert the middle step when the rule reference
    // lands on the layer record (CLAUDE.md 11.8: future tense, phase named).
    //
    // What can be pinned today is the boundary that already holds: with no rule,
    // an entity with no explicit style takes the layer's appearance and nothing
    // else, and kByLayerStyle{0} is what makes that the free case.
    CHECK_EQ(kByLayerStyle, StyleId{0});

    StyleTable t;
    CHECK_EQ(t.intern(Appearance{}), kByLayerStyle);

    const Appearance inherited = resolve_appearance(t.at(kByLayerStyle), layer_base());
    CHECK_EQ(inherited.rgba, layer_base().rgba);
    CHECK_EQ(inherited.width_um, layer_base().width_um);
}

// ------------------------------------------------------------- LayerTable ---

TEST_CASE("LayerTable: yeni tablo '0' katmanıyla açılır")
{
    LayerTable t;
    CHECK_EQ(t.size(), std::size_t{1});
    CHECK_EQ(t.find("0"), LayerId{0});

    const Layer* zero = t.at(0);
    REQUIRE(zero != nullptr);
    {
        CHECK_EQ(zero->name, std::string("0"));
        CHECK_EQ(zero->folded, std::string("0"));
        CHECK(zero->key != LayerKey::None);
    }
}

TEST_CASE("LayerTable: anahtarlar tekrar kullanılmaz, '0' katmanının anahtarı dahil")
{
    LayerTable t;
    KeyAllocator keys;

    const auto parsel = t.add(named("parsel"), keys);
    REQUIRE(parsel.ok());

    // R4: the default layer already holds the first key a fresh allocator would
    // mint, so add() must step over it rather than issue it twice.
    CHECK(t.key_of(parsel.value()) != t.key_of(0));

    const auto bina = t.add(named("bina"), keys);
    REQUIRE(bina.ok());
    CHECK(raw(t.key_of(bina.value())) > raw(t.key_of(parsel.value())));
}

TEST_CASE("LayerTable: slot ve anahtar çevirisi gidip gelir")
{
    LayerTable t;
    KeyAllocator keys;

    const auto added = t.add(named("yol"), keys);
    REQUIRE(added.ok());
    const LayerId slot = added.value();
    const LayerKey key = t.key_of(slot);

    CHECK_EQ(t.slot_of(key), slot);
    CHECK_EQ(raw(t.key_of(t.slot_of(key))), raw(key));

    CHECK_EQ(t.slot_of(LayerKey::None), kNoLayer);
    CHECK_EQ(t.slot_of(static_cast<LayerKey>(999999u)), kNoLayer);
    CHECK_EQ(raw(t.key_of(kNoLayer)), raw(LayerKey::None));
    CHECK(t.at(kNoLayer) == nullptr);
}

TEST_CASE("LayerTable: katman adı Türkçe katlanır — ışık/IŞIK aynı, isik değil")
{
    LayerTable t;
    KeyAllocator keys;

    const auto isik = t.add(named("ışık"), keys);
    REQUIRE(isik.ok());
    const LayerId slot = isik.value();

    CHECK_EQ(t.find("ışık"), slot);
    CHECK_EQ(t.find("IŞIK"), slot);
    CHECK_EQ(t.find("Işık"), slot);

    // std::toupper would fold "isik" onto "ISIK" and match; Turkish does not.
    CHECK_EQ(t.find("isik"), kNoLayer);
    CHECK_EQ(t.find("ISIK"), kNoLayer);

    CHECK_EQ(t.find("parsel"), kNoLayer);
}

TEST_CASE("LayerTable: aynı ad büyük/küçük harfe bakılmadan reddedilir")
{
    LayerTable t;
    KeyAllocator keys;

    CHECK(t.add(named("Parsel"), keys).ok());
    const std::size_t before = t.size();

    const auto dup = t.add(named("PARSEL"), keys);
    CHECK(!dup.ok());
    if (!dup.ok()) {
        CHECK(dup.error().code == ErrorCode::ValidationFailed);
        // Actionable: it must name the layer that is in the way.
        CHECK(dup.error().message.find("Parsel") != std::string::npos);
        CHECK(!dup.error().message.empty());
    }
    CHECK_EQ(t.size(), before);

    // Noktalı ve noktasız i ayrı harflerdir: "imar" ve "IMAR" çakışmaz.
    CHECK(t.add(named("imar"), keys).ok());
    CHECK(t.add(named("IMAR"), keys).ok());
    CHECK(t.find("imar") != t.find("IMAR"));
}

TEST_CASE("LayerTable: boş ad reddedilir")
{
    LayerTable t;
    KeyAllocator keys;

    const auto empty = t.add(named(""), keys);
    CHECK(!empty.ok());
    if (!empty.ok()) CHECK(empty.error().code == ErrorCode::InvalidArgument);
    CHECK_EQ(t.size(), std::size_t{1});
}

TEST_CASE("LayerTable: rename yalnızca adı değiştirir — anahtar ve kayıt yerinde kalır")
{
    LayerTable t;
    KeyAllocator keys;

    Layer l       = named("parsel");
    l.description = "kadastro parselleri";
    l.locked      = true;
    l.plottable   = false;
    l.min_scale   = 25000;
    l.max_scale   = 500;
    l.opacity     = 128;
    l.catalog_ref = "kadastro/parsel";
    l.appearance  = layer_base();

    const auto added = t.add(std::move(l), keys);
    REQUIRE(added.ok());
    const LayerId slot = added.value();
    REQUIRE(t.at(slot) != nullptr);
    const Layer before   = *t.at(slot);
    const std::size_t sz = t.size();

    CHECK(t.rename(slot, "Ada/Parsel").ok());

    REQUIRE(t.at(slot) != nullptr);
    const Layer after = *t.at(slot);

    // R30: the identity is the key, and renaming must not disturb anything an
    // entity or an external reference could be pointing at.
    CHECK_EQ(raw(after.key), raw(before.key));
    CHECK_EQ(t.slot_of(before.key), slot);
    CHECK_EQ(t.size(), sz);

    CHECK_EQ(after.name, std::string("Ada/Parsel"));
    CHECK_EQ(after.folded, turkish_upper("Ada/Parsel"));
    CHECK_EQ(after.description, before.description);
    CHECK_EQ(after.locked, before.locked);
    CHECK_EQ(after.plottable, before.plottable);
    CHECK_EQ(after.visible, before.visible);
    CHECK_EQ(after.min_scale, before.min_scale);
    CHECK_EQ(after.max_scale, before.max_scale);
    CHECK_EQ(after.opacity, before.opacity);
    CHECK_EQ(after.catalog_ref, before.catalog_ref);
    CHECK(after.appearance == before.appearance);

    // The name index moved with it.
    CHECK_EQ(t.find("parsel"), kNoLayer);
    CHECK_EQ(t.find("ADA/PARSEL"), slot);
}

TEST_CASE("LayerTable: rename hata yolları")
{
    LayerTable t;
    KeyAllocator keys;

    const LayerId a = t.add(named("parsel"), keys).value_or(kNoLayer);
    const LayerId b = t.add(named("bina"), keys).value_or(kNoLayer);
    REQUIRE(a != kNoLayer);
    REQUIRE(b != kNoLayer);
    REQUIRE(t.at(a) != nullptr && t.at(b) != nullptr);

    const Status taken = t.rename(b, "PARSEL");
    CHECK(!taken.ok());
    if (!taken.ok()) CHECK(taken.error().code == ErrorCode::ValidationFailed);
    CHECK_EQ(t.at(b)->name, std::string("bina"));

    const Status unknown = t.rename(kNoLayer, "yeni");
    CHECK(!unknown.ok());
    if (!unknown.ok()) CHECK(unknown.error().code == ErrorCode::NotFound);

    const Status blank = t.rename(a, "");
    CHECK(!blank.ok());
    if (!blank.ok()) CHECK(blank.error().code == ErrorCode::InvalidArgument);
    CHECK_EQ(t.at(a)->name, std::string("parsel"));

    // Renaming a layer onto its own folded name is a presentation change.
    CHECK(t.rename(a, "Parsel").ok());
    CHECK_EQ(t.at(a)->name, std::string("Parsel"));
    CHECK_EQ(t.find("parsel"), a);
}

TEST_CASE("LayerTable: visible_at ölçek sınırları dahildir")
{
    LayerTable t;
    KeyAllocator keys;

    Layer l            = named("bina");
    l.min_scale        = 25000; // zoomed-out limit: hidden past 1:25000
    l.max_scale        = 1000;  // zoomed-in limit: hidden past 1:1000
    const LayerId slot = t.add(std::move(l), keys).value_or(kNoLayer);
    CHECK(slot != kNoLayer);

    // Inclusive at both ends.
    CHECK(t.visible_at(slot, 25000));
    CHECK(t.visible_at(slot, 1000));
    CHECK(t.visible_at(slot, 5000));

    CHECK(!t.visible_at(slot, 25001)); // one step further out
    CHECK(!t.visible_at(slot, 999));   // one step further in
}

TEST_CASE("LayerTable: visible_at sıfır sınırı sınırsız demektir")
{
    LayerTable t;
    KeyAllocator keys;

    Layer open_out     = named("kadastro");
    open_out.max_scale = 1000; // min_scale stays 0
    const LayerId a    = t.add(std::move(open_out), keys).value_or(kNoLayer);

    Layer open_in     = named("pafta");
    open_in.min_scale = 25000; // max_scale stays 0
    const LayerId b   = t.add(std::move(open_in), keys).value_or(kNoLayer);

    CHECK(t.visible_at(a, 4000000000u));
    CHECK(t.visible_at(a, 1000));
    CHECK(!t.visible_at(a, 999));

    CHECK(t.visible_at(b, 1));
    CHECK(t.visible_at(b, 25000));
    CHECK(!t.visible_at(b, 25001));

    // Both zero: drawn at every scale.
    CHECK(t.visible_at(0, 0));
    CHECK(t.visible_at(0, 4000000000u));

    // A hidden layer is drawn at no scale, and an unknown slot at none either.
    Layer hidden    = named("kapali");
    hidden.visible  = false;
    const LayerId c = t.add(std::move(hidden), keys).value_or(kNoLayer);
    CHECK(!t.visible_at(c, 1000));
    CHECK(!t.visible_at(kNoLayer, 1000));
}

TEST_CASE("LayerTable: fold her saklanan alana tepki verir")
{
    LayerTable base;
    KeyAllocator base_keys;
    CHECK(base.add(named("parsel"), base_keys).ok());

    const std::uint64_t reference = base.fold(0);
    CHECK_EQ(base.fold(0), reference); // idempotent
    CHECK(base.fold(1) != reference);  // the seed is carried

    LayerTable twin;
    KeyAllocator twin_keys;
    CHECK(twin.add(named("parsel"), twin_keys).ok());
    CHECK_EQ(twin.fold(0), reference); // same construction, same fingerprint

    struct Change
    {
        const char* what;
        void (*apply)(Layer&);
    };

    const Change changes[] = {
        {"name", [](Layer& l) { l.name = "Parsel"; }},
        {"description", [](Layer& l) { l.description = "kadastro"; }},
        {"visible", [](Layer& l) { l.visible = false; }},
        {"locked", [](Layer& l) { l.locked = true; }},
        {"plottable", [](Layer& l) { l.plottable = false; }},
        // Every Appearance field, not just two of them: the layer's default
        // appearance is a stored record, and a stored field left out of the
        // fingerprint is a field a save can change without the document noticing.
        {"appearance.rgba", [](Layer& l) { l.appearance.rgba = 0xFF010203u; }},
        {"appearance.width_um", [](Layer& l) { l.appearance.width_um = 777; }},
        {"appearance.dash", [](Layer& l) { l.appearance.dash = 6; }},
        {"appearance.symbol", [](Layer& l) { l.appearance.symbol = 13; }},
        {"appearance.fill_rgba", [](Layer& l) { l.appearance.fill_rgba = 0x80010203u; }},
        {"appearance.hatch", [](Layer& l) { l.appearance.hatch = 3; }},
        {"appearance.z_order", [](Layer& l) { l.appearance.z_order = 9; }},
        {"appearance.src_colour", [](Layer& l) { l.appearance.src_colour = Source::Explicit; }},
        {"appearance.src_width", [](Layer& l) { l.appearance.src_width = Source::Explicit; }},
        {"appearance.src_dash", [](Layer& l) { l.appearance.src_dash = Source::Explicit; }},
        {"appearance.src_fill", [](Layer& l) { l.appearance.src_fill = Source::Explicit; }},
        {"min_scale", [](Layer& l) { l.min_scale = 25000; }},
        {"max_scale", [](Layer& l) { l.max_scale = 500; }},
        {"opacity", [](Layer& l) { l.opacity = 128; }},
        {"catalog_ref", [](Layer& l) { l.catalog_ref = "kadastro/parsel"; }},
    };

    for (const auto& c : changes) {
        LayerTable t;
        KeyAllocator keys;
        Layer l = named("parsel");
        c.apply(l);
        CHECK(t.add(std::move(l), keys).ok());
        if (t.fold(0) == reference)
            ::microtest::report(__FILE__, __LINE__, "fold() ignored a stored field", c.what);
    }

    // A rename is a document change: the file keeps the casing the user typed.
    LayerTable renamed;
    KeyAllocator renamed_keys;
    CHECK(renamed.add(named("parsel"), renamed_keys).ok());
    CHECK(renamed.rename(1, "Parsel").ok());
    CHECK(renamed.fold(0) != reference);
}
