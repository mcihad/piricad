// SPDX-License-Identifier: GPL-3.0-or-later
//
// DYNAMIC INPUT (TODOS U-02): the length and the angle typed beside the cursor.
//
// The class under test holds a few flags and does no geometry; what these cases pin is that
// every figure it makes is the figure of a LINE A PERSON COULD HAVE TYPED, read by the one
// grammar — so a locked length held while the mouse swings, a typed pair and the same pair in
// a script are one point.
#include "piricad_test.hpp"

#include "piricad/command/dynamic_entry.hpp"
#include "piricad/core/angle.hpp"
#include "piricad/core/geometry.hpp"

#include <cmath>
#include <cstdint>
#include <random>
#include <string>

using namespace piricad;
using namespace piricad::command;

namespace {

constexpr core::Point2 kOrigin{485'300'000, 4'310'200'000};

ResolveContext semt_grad()
{
    ResolveContext ctx;
    ctx.convention = core::AngleConvention{core::AngleUnit::Grad, core::AngleRule::Semt};
    return ctx;
}

ResolveContext math_degree()
{
    ResolveContext ctx;
    ctx.convention = core::AngleConvention{core::AngleUnit::Degree, core::AngleRule::Matematik};
    return ctx;
}

core::Point2 typed(std::string_view line, const ResolveContext& ctx)
{
    auto read = parse_point(line, kOrigin, ctx);
    REQUIRE_MESSAGE(read.ok(), line);
    return read.value();
}

double metres_between(core::Point2 a, core::Point2 b)
{
    return std::hypot(static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y)) / 1000.0;
}

} // namespace

TEST_CASE("DİNAMİK GİRDİ: yalın sayı bir uzunluktur, virgüllü olan koordinattır")
{
    for (const char* text : {"12.5", "12.5 m", "1250cm", "(2m+50cm)", " 7 ", "0.25"})
        CHECK_MESSAGE(DynamicEntry::is_bare_length(text), text);
    // `12,5` is the point (12; 5) at a point prompt, as it always was.
    for (const char* text : {"12,5", "10,0", "@10,0", "@10<45", "", "abc", "dik(1,2,3,4,5,6)"})
        CHECK_MESSAGE(!DynamicEntry::is_bare_length(text), text);
}

TEST_CASE("DİNAMİK GİRDİ: kilitsiz yazılan uzunluk imlecin yönünde o kadar gider")
{
    const ResolveContext ctx = semt_grad();
    DynamicEntry entry;
    // The cursor is 30 m east and 40 m north: 50 m away, 53.13° from north-to-east.
    const core::Point2 cursor{kOrigin.x + 30'000, kOrigin.y + 40'000};

    auto point = entry.resolve(kOrigin, cursor, "12.5", ctx);
    REQUIRE(point.ok());
    CHECK_MESSAGE(std::abs(metres_between(kOrigin, point.value()) - 12.5) <= 0.0015,
                  metres_between(kOrigin, point.value()));
    // On the line from the origin to the cursor, to a millimetre across.
    const double ux = 30.0 / 50.0;
    const double uy = 40.0 / 50.0;
    CHECK(std::abs(static_cast<double>(point.value().x - kOrigin.x) / 1000.0 - 12.5 * ux) <= 0.002);
    CHECK(std::abs(static_cast<double>(point.value().y - kOrigin.y) / 1000.0 - 12.5 * uy) <= 0.002);

    // Nothing typed and nothing locked: the cursor's own point, to the millimetre.
    auto none = entry.resolve(kOrigin, cursor, "", ctx);
    REQUIRE(none.ok());
    CHECK(std::abs(static_cast<double>(none.value().x - cursor.x)) <= 1.0);
    CHECK(std::abs(static_cast<double>(none.value().y - cursor.y)) <= 1.0);
}

TEST_CASE("DİNAMİK GİRDİ: Tab uzunluğu kilitler, fare oynayınca uzunluk değişmez")
{
    const ResolveContext ctx = semt_grad();
    DynamicEntry entry;
    CHECK(entry.active() == DynField::Length);

    auto next = entry.tab("12.5", ctx);
    REQUIRE(next.ok());
    CHECK(next.value().empty()); ///< the line is cleared for the angle
    CHECK(entry.locked(DynField::Length));
    CHECK_FALSE(entry.locked(DynField::Angle));
    CHECK(entry.active() == DynField::Angle);

    // The mouse swings through a quarter turn at very different reaches: always 12.5 m out.
    for (const core::Point2 cursor :
         {core::Point2{kOrigin.x + 3'000, kOrigin.y}, core::Point2{kOrigin.x, kOrigin.y + 90'000},
          core::Point2{kOrigin.x - 40'000, kOrigin.y + 40'000},
          core::Point2{kOrigin.x + 700, kOrigin.y - 1'200}}) {
        auto point = entry.resolve(kOrigin, cursor, "", ctx);
        REQUIRE(point.ok());
        CHECK_MESSAGE(std::abs(metres_between(kOrigin, point.value()) - 12.5) <= 0.0015,
                      metres_between(kOrigin, point.value()));
    }
}

TEST_CASE("DİNAMİK GİRDİ: iki alan kilitliyse nokta yazılan satırın noktasıdır")
{
    for (const ResolveContext& ctx : {semt_grad(), math_degree()}) {
        DynamicEntry entry;
        REQUIRE(entry.tab("12.5", ctx).ok());
        REQUIRE(entry.tab("45", ctx).ok());
        CHECK(entry.locked(DynField::Length));
        CHECK(entry.locked(DynField::Angle));

        // Wherever the cursor is — it decides nothing now — and byte for byte what typing
        // `@12.5<45` gives.
        const core::Point2 cursor{kOrigin.x + 1'234, kOrigin.y - 98'765};
        auto point = entry.resolve(kOrigin, cursor, "", ctx);
        REQUIRE(point.ok());
        CHECK(point.value() == typed("@12.5<45", ctx));
    }
}

TEST_CASE("DİNAMİK GİRDİ: yalnız açı kilitliyse uzunluk imlecin erişimidir")
{
    const ResolveContext ctx = semt_grad();
    DynamicEntry entry;
    // Tab with nothing typed walks to the angle without locking anything.
    auto walked = entry.tab("", ctx);
    REQUIRE(walked.ok());
    CHECK(entry.active() == DynField::Angle);
    CHECK_FALSE(entry.any_locked());

    REQUIRE(entry.tab("100", ctx).ok()); ///< 100 grad: due east under semt
    CHECK(entry.locked(DynField::Angle));

    // The cursor is 20 m away to the north-east; the point is 20 m away due east.
    const core::Point2 cursor{kOrigin.x + 14'142, kOrigin.y + 14'142};
    auto point = entry.resolve(kOrigin, cursor, "", ctx);
    REQUIRE(point.ok());
    CHECK(std::abs(static_cast<double>(point.value().y - kOrigin.y)) <= 2.0);
    CHECK(std::abs(metres_between(kOrigin, point.value()) - 20.0) <= 0.002);
}

TEST_CASE("DİNAMİK GİRDİ: birim, ifade ve ondalık virgül uzunlukta çalışır")
{
    const ResolveContext ctx = semt_grad();
    for (const char* text : {"1250 cm", "12.5 m", "12500mm", "(2m+10.5)*1", "12,5"}) {
        DynamicEntry entry;
        if (std::string(text) == "(2m+10.5)*1") continue; ///< a length plus a bare number: refused
        REQUIRE_MESSAGE(entry.tab(text, ctx).ok(), text);
        REQUIRE(entry.tab("100", ctx).ok());
        auto point = entry.resolve(kOrigin, kOrigin, "", ctx);
        REQUIRE_MESSAGE(point.ok(), text);
        CHECK_MESSAGE(point.value() == typed("@12.5<100", ctx), text);
    }
    DynamicEntry mixed;
    CHECK_FALSE(mixed.tab("(2m+10.5)", ctx).ok()); ///< the unit rules of the one parser
}

TEST_CASE("DİNAMİK GİRDİ: geçersiz alan reddedilir, hiçbir şey değişmez")
{
    const ResolveContext ctx = semt_grad();
    DynamicEntry entry;
    for (const char* bad : {"abc", "0", "-3", "1e99", "12 xyz"}) {
        auto refused = entry.tab(bad, ctx);
        CHECK_MESSAGE(!refused.ok(), bad);
        CHECK_FALSE(entry.any_locked());
        CHECK(entry.active() == DynField::Length);
    }
    REQUIRE(entry.tab("5", ctx).ok());
    CHECK_FALSE(entry.tab("45zz", ctx).ok()); ///< no such angle unit
    CHECK_FALSE(entry.locked(DynField::Angle));
    CHECK(entry.locked(DynField::Length)); ///< the earlier lock survives a bad second one

    // A bad figure typed but not yet locked is said so by `shown`, and Enter's compose refuses it.
    auto line = entry.compose(kOrigin, kOrigin, "45zz", ctx);
    CHECK_FALSE(line.ok());
    const auto fields = entry.shown(kOrigin, kOrigin, "45zz", ctx.convention);
    CHECK(fields[1].typing);
    CHECK(fields[1].invalid);
}

TEST_CASE("DİNAMİK GİRDİ: boş satırda Tab alanlar arasında gezer, kilitliyi düzenlemeye açar")
{
    const ResolveContext ctx = semt_grad();
    DynamicEntry entry;
    REQUIRE(entry.tab("12.5 m", ctx).ok());
    REQUIRE(entry.tab("45", ctx).ok());
    CHECK(entry.active() == DynField::Length); ///< round again, onto the first

    // Tab on an empty line goes to the other field; the angle is locked, so it comes back into the
    // line to be edited and is no longer locked.
    auto back = entry.tab("", ctx);
    REQUIRE(back.ok());
    CHECK(entry.active() == DynField::Angle);
    CHECK(back.value() == "45");
    CHECK_FALSE(entry.locked(DynField::Angle));
    CHECK(entry.locked(DynField::Length));

    // Esc and every new question drop it all.
    entry.clear();
    CHECK_FALSE(entry.any_locked());
    CHECK(entry.active() == DynField::Length);
}

TEST_CASE("DİNAMİK GİRDİ: ekranda yazılan şey kilidi, etkin alanı ve yazılanı söyler")
{
    const ResolveContext ctx = semt_grad();
    DynamicEntry entry;
    const core::Point2 cursor{kOrigin.x + 10'000, kOrigin.y};

    // Before anything: the cursor's own figures, length active.
    auto idle = entry.shown(kOrigin, cursor, "", ctx.convention);
    CHECK(idle[0].text == "10 m");
    CHECK(idle[0].active);
    CHECK_FALSE(idle[0].locked);
    CHECK_FALSE(idle[0].typing);
    CHECK(idle[1].text == "100,0000 grad"); ///< due east in semt grad

    // Typing is shown as typed, in metres, before it is locked.
    auto typing = entry.shown(kOrigin, cursor, "1250 cm", ctx.convention);
    CHECK(typing[0].typing);
    CHECK(typing[0].text == "12,5 m");

    REQUIRE(entry.tab("1250 cm", ctx).ok());
    auto after = entry.shown(kOrigin, cursor, "", ctx.convention);
    CHECK(after[0].locked);
    CHECK(after[0].text == "12,5 m");
    CHECK(after[1].active);
    CHECK_FALSE(after[1].locked);

    REQUIRE(entry.tab("45", ctx).ok());
    auto both = entry.shown(kOrigin, cursor, "", ctx.convention);
    CHECK(both[1].locked);
    CHECK(both[1].text == "45 grad");
}

TEST_CASE("ÖZELLİK: DİNAMİK GİRDİ kilitli uzunluk her imleçte bir milimetre içinde kalır")
{
    // Seeded, portable (mt19937_64 + modulo; test.md R26): for any cursor, a locked length gives a
    // point that distance from the origin and in the cursor's direction, and a locked angle gives
    // the cursor's reach in the locked direction.
    const ResolveContext ctx = semt_grad();
    std::mt19937_64 engine(20261003);
    const auto in = [&engine](std::int64_t lo, std::int64_t hi) {
        return lo + static_cast<std::int64_t>(engine() % static_cast<std::uint64_t>(hi - lo + 1));
    };

    for (int i = 0; i < 300; ++i) {
        INFO("tohum " << i);
        const core::Mm length_mm = in(1'000, 90'000);
        DynamicEntry locked_length;
        REQUIRE(locked_length
                    .tab(std::to_string(length_mm / 1000) + "." +
                             std::string(3 - std::to_string(length_mm % 1000).size(), '0') +
                             std::to_string(length_mm % 1000),
                         ctx)
                    .ok());
        const core::Point2 cursor{kOrigin.x + in(-200'000, 200'000),
                                  kOrigin.y + in(-200'000, 200'000)};
        if (core::segment_length(kOrigin, cursor) < 2'000) continue;

        auto point = locked_length.resolve(kOrigin, cursor, "", ctx);
        REQUIRE(point.ok());
        CHECK(std::abs(static_cast<double>(core::segment_length(kOrigin, point.value()) -
                                           length_mm)) <= 1.5);
        // The direction is the cursor's: the angle between them is under what a millimetre of
        // rounding at that length can turn.
        const double a = std::atan2(static_cast<double>(point.value().y - kOrigin.y),
                                    static_cast<double>(point.value().x - kOrigin.x));
        const double b = std::atan2(static_cast<double>(cursor.y - kOrigin.y),
                                    static_cast<double>(cursor.x - kOrigin.x));
        double d       = std::abs(a - b);
        if (d > 3.14159265358979) d = 2.0 * 3.14159265358979 - d;
        CHECK(d <= 2.0 / static_cast<double>(length_mm) + 2e-6);
    }
}
