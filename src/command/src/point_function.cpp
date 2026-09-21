// SPDX-License-Identifier: GPL-3.0-or-later
#include "point_function.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace kentos::command::detail {
namespace {

using core::err;
using core::ErrorCode;
using core::Mm;
using core::Point2;

// ------------------------------------------------------------- lexical ----

bool is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && is_blank(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_blank(s.back()))
        s.remove_suffix(1);
    return s;
}

/// Metres with three decimals, written in integers so no locale and no rounding
/// mode reaches an error message: `-1234,500`.
std::string metres_text(Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
}

/// What a field starts like, which is all the matcher needs to know before the
/// signature tells it what the field must BE.
///
/// `Numeric` and `At` are deliberately disjoint from `Name`: that is what makes
/// `kes(A,açı1,B,açı2)` and `kes(A,B,C,D)` impossible to confuse, because the
/// second field of the first is a number and of the second is a point, and no
/// text is both.
enum class Shape : std::uint8_t {
    Numeric, ///< a number or a parenthesised expression: `30`, `-5`, `(40+5)`
    At,      ///< `@dx` or `@d<a` — the first field of a relative or polar form
    Name,    ///< a word or a nested call: `son`, `n(1284)`, `sol`, `yon=10`
    Other,   ///< empty, or something the grammar has no reading for
};

Shape shape_of(std::string_view f)
{
    if (f.empty()) return Shape::Other;
    const char c = f.front();
    if ((c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-' || c == '(')
        return Shape::Numeric;
    if (c == '@') return Shape::At;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
        static_cast<unsigned char>(c) >= 0x80U)
        return Shape::Name;
    return Shape::Other;
}

/// Whether a `<` appears outside every parenthesis — the mark of the polar form,
/// and the reason `@100<(40+5)` is one field while `@50,30` is two.
bool has_top_level_less(std::string_view f)
{
    int depth = 0;
    for (char c : f) {
        if (c == '(')
            ++depth;
        else if (c == ')')
            --depth;
        else if (c == '<' && depth == 0)
            return true;
    }
    return false;
}

/// Splits a call's body at its top-level commas. An empty body has no fields at
/// all, which is what lets `son()` match a signature that takes none.
std::vector<std::string_view> split_fields(std::string_view body)
{
    std::vector<std::string_view> out;
    if (trim(body).empty()) return out;

    int depth         = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '(')
            ++depth;
        else if (body[i] == ')')
            --depth;
        else if (body[i] == ',' && depth == 0) {
            out.push_back(trim(body.substr(start, i - start)));
            start = i + 1;
        }
    }
    out.push_back(trim(body.substr(start)));
    return out;
}

// --------------------------------------------------------- the functions ----

/// What one argument of a point function is.
enum class ArgKind : std::uint8_t {
    Point,     ///< a coordinate: one field, or two when written `x,y` / `@dx,dy`
    Number,    ///< a distance or a plain number, in metres where it is a length
    Angle,     ///< a number that may carry a `g`/`d`/`r` unit suffix
    Offset,    ///< `@dx,dy` or `@d<a`, measured from the argument before it
    Ratio,     ///< a number, or a number and the word `m` meaning metres
    Direction, ///< `sol` · `sağ`, or `yon=<nokta>` naming the solution wanted
};

/// Short spellings, used only by the table below so a signature fits one line.
constexpr ArgKind kPt  = ArgKind::Point;
constexpr ArgKind kNum = ArgKind::Number;
constexpr ArgKind kAng = ArgKind::Angle;
constexpr ArgKind kOff = ArgKind::Offset;
constexpr ArgKind kRat = ArgKind::Ratio;
constexpr ArgKind kDir = ArgKind::Direction;

/// The longest signature this grammar has — `kes(A,r1,B,r2,yön)`.
inline constexpr std::size_t kMaxArgs = 5;

/// One shape one point function takes.
///
/// A name may have several: `kes` has three, and which one a call took is
/// decided once, by matching, and then recorded in `Token::word` so resolution
/// never has to work it out again from the argument kinds.
struct PointForm
{
    const char* form;   ///< the id in `Token::word`: `orta`, `kes.mesafe`
    const char* key;    ///< the folded name the user types (`turkish_fold_key`)
    const char* syntax; ///< how the docs and every error message spell this shape

    /// The arguments in order; entries past `arity` are never read.
    std::array<ArgKind, kMaxArgs> args;
    std::size_t arity; ///< how many of `args` this shape has
};

/// EVERY POINT FUNCTION, once (CLAUDE.md 5.10 applied to the grammar).
///
/// The name is matched with `turkish_fold_key`, so case and the dotted/dotless
/// i are already handled and `UZANTI`, `uzanti` and `uzantı` are one entry.
/// Order matters only for the message a call that fits nothing gets: the shapes
/// are listed in the order they appear here.
constexpr std::array<PointForm, 12> kForms{{
    {"son", "SON", "son", {{}}, 0},
    {"n", "N", "n(nokta_no)", {{kNum}}, 1},
    {"orta", "ORTA", "orta(A,B)", {{kPt, kPt}}, 2},
    {"ile", "ILE", "ile(P,@dx,dy)", {{kPt, kOff}}, 2},
    {"dik", "DIK", "dik(A,B,ayak,boy)", {{kPt, kPt, kNum, kNum}}, 4},
    {"semt", "SEMT", "semt(S,açı,kenar)", {{kPt, kAng, kNum}}, 3},
    {"kes.dogrultu", "KES", "kes(A,açı1,B,açı2)", {{kPt, kAng, kPt, kAng}}, 4},
    {"kes.mesafe", "KES", "kes(A,r1,B,r2,sol|sağ|yon=<nokta>)", {{kPt, kNum, kPt, kNum, kDir}}, 5},
    {"kes.dogru", "KES", "kes(A,B,C,D)", {{kPt, kPt, kPt, kPt}}, 4},
    {"ara", "ARA", "ara(A,B,oran) · ara(A,B,mesafe m)", {{kPt, kPt, kRat}}, 3},
    {"uzanti", "UZANTI", "uzanti(A,B,mesafe)", {{kPt, kPt, kNum}}, 3},
    {"xy", "XY", "xy(P,Q)", {{kPt, kPt}}, 2},
}};

/// The name a user typed for `form`, which is the id up to its dot: the three
/// shapes of `kes` are all spelled `kes`.
std::string_view display_name(std::string_view form)
{
    const std::size_t dot = form.find('.');
    return dot == std::string_view::npos ? form : form.substr(0, dot);
}

/// Every shape declared under the folded name `key`, appended to `out`.
void forms_named(std::string_view key, std::vector<const PointForm*>& out)
{
    for (const PointForm& f : kForms)
        if (key == f.key) out.push_back(&f);
}

/// The `son` token, built where a bare `son` stands in an argument list.
///
/// A ZERO-ARGUMENT CALL RATHER THAN A WORD, and only inside an argument list.
/// At the top of a line `son` has to stay an ordinary word or `KATMAN son` would
/// name a coordinate instead of a layer — and nothing is lost, because the point
/// before is also what `@0,0` says.
Token son_token()
{
    Token t;
    t.kind = Token::Kind::Call;
    t.word = "son";
    return t;
}

// ----------------------------------------------------------- the matcher ----

/// One attempt to read an argument list against one signature.
///
/// Fields are consumed left to right and a failure stops the attempt with its
/// reason in `why`; a function with a single shape reports that reason, and one
/// with several reports the list of shapes instead, because the reason from
/// whichever shape happened to be tried last would name the wrong mistake.
struct Attempt
{
    Attempt(const std::vector<std::string_view>& all, std::string_view called, int nesting)
        : fields(all), name(called), depth(nesting)
    {}

    const std::vector<std::string_view>& fields;
    std::string_view name;
    int depth;
    std::size_t at{0};
    std::vector<Token> out;
    std::string why;

    std::string where(std::size_t index) const
    {
        return std::string(name) + "(): " + std::to_string(index + 1) + ". argüman ";
    }

    std::string got() const
    {
        return at < fields.size() ? "Girilen: '" + std::string(fields[at]) + "'" : "Argüman bitti.";
    }
};

/// Reads a number, optionally with a one-letter angle unit suffix, out of one
/// field. The suffix is taken off before the expression is read, exactly as the
/// polar form does it, so `(40+5)g` works and `45gg` is refused by name.
core::Result<Token> number_token(std::string_view field, bool allow_angle_suffix)
{
    Token t;
    t.kind = Token::Kind::Number;

    std::string_view body = field;
    if (allow_angle_suffix)
        if (core::AngleUnit named{};
            body.size() > 1 && core::angle_unit_from_suffix(body.back(), named)) {
            t.angle_unit = named;
            body.remove_suffix(1);
        }

    auto value = evaluate_expression(body);
    if (!value) return value.error();
    t.a = value.value();
    return t;
}

bool consume_point(Attempt& m, std::size_t index, std::string_view first);

/// Reads one argument of kind `kind`, appending the token(s) it becomes.
bool consume(Attempt& m, ArgKind kind, std::size_t index)
{
    if (m.at >= m.fields.size()) {
        m.why = m.where(index) + "eksik.";
        return false;
    }
    const std::string_view f = m.fields[m.at];

    switch (kind) {
    case ArgKind::Point: return consume_point(m, index, f);

    case ArgKind::Number:
    case ArgKind::Angle: {
        if (shape_of(f) != Shape::Numeric) {
            m.why = m.where(index) + "sayı olmalı. " + m.got();
            return false;
        }
        auto t = number_token(f, kind == ArgKind::Angle);
        if (!t) {
            m.why =
                m.where(index) + (kind == ArgKind::Angle ? "açı: " : "sayı: ") + t.error().message;
            return false;
        }
        m.out.push_back(std::move(t.value()));
        ++m.at;
        return true;
    }

    case ArgKind::Ratio: {
        // `ara(A,B,30 m)` and `ara(A,B,30m)` are the same thing: a distance in
        // metres rather than a fraction of the way along. The unit is kept as
        // its own `m` token so the resolver reads a shape rather than a flag.
        if (shape_of(f) != Shape::Numeric) {
            m.why = m.where(index) + "oran ya da `mesafe m` olmalı. " + m.got();
            return false;
        }
        std::string_view body = f;
        bool metres           = false;
        if (body.size() > 1 && (body.back() == 'm' || body.back() == 'M')) {
            body.remove_suffix(1);
            body   = trim(body);
            metres = true;
        }
        auto t = number_token(body, false);
        if (!t) {
            m.why = m.where(index) + "sayı: " + t.error().message;
            return false;
        }
        m.out.push_back(std::move(t.value()));
        if (metres) {
            Token unit;
            unit.kind = Token::Kind::Word;
            unit.word = "m";
            m.out.push_back(std::move(unit));
        }
        ++m.at;
        return true;
    }

    case ArgKind::Offset: {
        // Measured FROM the argument before it, so it has to be written as one:
        // an absolute pair here would silently ignore the point it was given.
        if (shape_of(f) != Shape::At) {
            m.why = m.where(index) + "`@dx,dy` ya da `@mesafe<açı` olmalı. " + m.got();
            return false;
        }
        std::string text(f);
        std::size_t used = 1;
        if (!has_top_level_less(f)) {
            if (m.at + 1 >= m.fields.size() || shape_of(m.fields[m.at + 1]) != Shape::Numeric) {
                m.why = m.where(index) + "`@dx,dy` iki sayı ister. " + m.got();
                return false;
            }
            text += ',';
            text += m.fields[m.at + 1];
            used = 2;
        }
        auto t = classify(text, m.depth + 1);
        if (!t ||
            (t.value().kind != Token::Kind::Relative && t.value().kind != Token::Kind::Polar)) {
            m.why = m.where(index) + "göreli biçim okunamadı. Girilen: '" + text + "'";
            return false;
        }
        m.out.push_back(std::move(t.value()));
        m.at += used;
        return true;
    }

    case ArgKind::Direction: {
        // `sol` and `sağ` are bare words, which no other shape of `kes` can
        // swallow. A NEARBY POINT has to be written `yon=…`, because a bare
        // coordinate here would read equally well as the third and fourth
        // points of `kes(A,B,C,D)` and picking one of those in silence is the
        // whole thing this argument exists to prevent (TODOS-CAD P1a-9).
        if (shape_of(f) != Shape::Name) {
            m.why = m.where(index) + "`sol`, `sağ` ya da `yon=<nokta>` olmalı. " + m.got();
            return false;
        }
        const std::size_t eq = f.find('=');
        if (eq == std::string_view::npos) {
            const std::string folded = core::turkish_fold_key(f);
            if (folded != "SOL" && folded != "SAG") {
                m.why = m.where(index) + "`sol`, `sağ` ya da `yon=<nokta>` olmalı. " + m.got();
                return false;
            }
            Token t;
            t.kind = Token::Kind::Word;
            t.word = folded;
            m.out.push_back(std::move(t));
            ++m.at;
            return true;
        }
        if (core::turkish_fold_key(f.substr(0, eq)) != "YON") {
            m.why = m.where(index) + "bilinmeyen anahtar. Beklenen: `yon=<nokta>`. " + m.got();
            return false;
        }
        return consume_point(m, index, f.substr(eq + 1));
    }
    }
    return false;
}

/// Reads a point argument, whose first field is `first` — the field itself,
/// except after a `yon=` where it is what follows the `=`.
///
/// One field when it is polar, `son` or a nested call; two when it is written
/// `x,y` or `@dx,dy`. There is no backtracking and none is needed: a single
/// numeric field is not a point under any reading, so the choice is forced.
bool consume_point(Attempt& m, std::size_t index, std::string_view first)
{
    const Shape shape = shape_of(first);

    if (shape == Shape::At && has_top_level_less(first)) {
        auto t = classify(first, m.depth + 1);
        if (!t || t.value().kind != Token::Kind::Polar) {
            m.why =
                m.where(index) + "kutupsal biçim okunamadı. Girilen: '" + std::string(first) + "'";
            return false;
        }
        m.out.push_back(std::move(t.value()));
        ++m.at;
        return true;
    }

    if (shape == Shape::Name) {
        if (core::turkish_fold_key(first) == "SON") {
            m.out.push_back(son_token());
            ++m.at;
            return true;
        }
        if (!is_call_text(first)) {
            m.why = m.where(index) +
                    "nokta olmalı (x,y | @dx,dy | @mesafe<açı | son | fonksiyon). Girilen: '" +
                    std::string(first) + "'";
            return false;
        }
        auto t = parse_call(first, m.depth + 1);
        if (!t) {
            m.why = t.error().message;
            return false;
        }
        m.out.push_back(std::move(t.value()));
        ++m.at;
        return true;
    }

    if ((shape == Shape::Numeric || shape == Shape::At) && m.at + 1 < m.fields.size() &&
        shape_of(m.fields[m.at + 1]) == Shape::Numeric) {
        std::string text(first);
        text += ',';
        text += m.fields[m.at + 1];
        auto t = classify(text, m.depth + 1);
        if (!t ||
            (t.value().kind != Token::Kind::Absolute && t.value().kind != Token::Kind::Relative)) {
            m.why = m.where(index) + "koordinat okunamadı. Girilen: '" + text + "'";
            return false;
        }
        m.out.push_back(std::move(t.value()));
        m.at += 2;
        return true;
    }

    m.why = m.where(index) +
            "nokta olmalı (x,y | @dx,dy | @mesafe<açı | son | fonksiyon). Girilen: '" +
            std::string(first) + "'";
    return false;
}

/// Tries one signature against the whole field list. A shape fits only when it
/// uses every field: leftovers mean the user meant another shape.
bool try_form(const PointForm& form, const std::vector<std::string_view>& fields,
              std::string_view name, int depth, std::vector<Token>& out, std::string& why)
{
    Attempt m{fields, name, depth};
    for (std::size_t i = 0; i < form.arity; ++i)
        if (!consume(m, form.args[i], i)) {
            why = m.why;
            return false;
        }
    if (m.at != fields.size()) {
        why = std::string(name) + "(): fazla argüman. Beklenen: " + form.syntax;
        return false;
    }
    out = std::move(m.out);
    return true;
}

// -------------------------------------------------------- the geometry ----

/// How far out a direction is carried before two of them are intersected.
///
/// A direction has no second point, so one is made: a hundred kilometres along
/// it. The length is not arbitrary — `polar_offset` rounds its endpoint to the
/// millimetre, so the direction it encodes is off by at most half a millimetre
/// in 10^8, five parts in a thousand million. At a kilometre from the station
/// that is a hundredth of a millimetre, an order below the storage unit, and it
/// buys the reuse of `core::line_intersection` instead of a second intersection
/// written here (CLAUDE.md 5.16).
constexpr double kDirectionRay = 100000.0;

/// The convention an angle argument was written under: the session's, with the
/// token's own `g`/`d`/`r` suffix winning where it has one.
core::AngleConvention applied_to(const Token& angle, const ResolveContext& ctx)
{
    core::AngleConvention applied = ctx.convention;
    if (angle.angle_unit) applied.unit = *angle.angle_unit;
    return applied;
}

/// `boy` metres to the LEFT of A→B, `ayak` metres along it from A.
///
/// LEFT IS POSITIVE, which is what Netcad's dik ayak / dik boy means and what
/// `core::circle_intersection` calls its left solution. The sign is the whole
/// content of the function for the user, so it is stated on the command page
/// and tested both ways (TODOS-CAD P1a-6).
core::Result<Point2> perpendicular(Point2 a, Point2 b, double foot_m, double offset_m)
{
    // THE CONSTRUCTION IS `core`'S, not this file's. The `DİKAYAK` command draws
    // the same points from a hand rather than from a typed expression, and a sign
    // convention written twice is how one of the two ends up mirrored
    // (`core::perpendicular_offset`, CLAUDE.md 5.10).
    Point2 out{};
    if (!core::perpendicular_offset(a, b, core::mm_from_metres(foot_m),
                                    core::mm_from_metres(offset_m), out))
        return err(ErrorCode::InvalidArgument,
                   "dik(): A ve B aynı nokta, dik indirilecek bir doğrultu yok.");
    return out;
}

/// `distance` metres past B, along A→B.
core::Result<Point2> beyond(Point2 a, Point2 b, double distance_m)
{
    const double dx  = static_cast<double>(b.x - a.x);
    const double dy  = static_cast<double>(b.y - a.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len == 0.0)
        return err(ErrorCode::InvalidArgument,
                   "uzanti(): A ve B aynı nokta, uzatılacak bir doğrultu yok.");

    const double d = distance_m * static_cast<double>(core::kMmPerMetre);
    return Point2{b.x + core::mm_round(d * dx / len), b.y + core::mm_round(d * dy / len)};
}

/// Where the direction `ang1` from `a` crosses the direction `ang2` from `b`.
core::Result<Point2> direction_crossing(Point2 a, const Token& ang1, Point2 b, const Token& ang2,
                                        const ResolveContext& ctx)
{
    const core::AngleConvention c1 = applied_to(ang1, ctx);
    const core::AngleConvention c2 = applied_to(ang2, ctx);

    // PARALLEL IS DECIDED IN INTEGERS, before any trigonometry. Two directions a
    // half turn apart give ray endpoints whose cross product is a rounding
    // artefact rather than zero, and `line_intersection` would then answer with
    // a point somewhere past the moon instead of refusing.
    const std::int64_t half = core::kUDegFullCircle / 2;
    std::int64_t apart =
        (core::udeg_from_angle(ang1.a, c1.unit) - core::udeg_from_angle(ang2.a, c2.unit)) % half;
    if (apart < 0) apart += half;

    const auto both_angles = [&] {
        return " Açılar: " +
               core::angle_text(core::turns_from_udeg(core::udeg_from_angle(ang1.a, c1.unit)),
                                c1.unit) +
               " ve " +
               core::angle_text(core::turns_from_udeg(core::udeg_from_angle(ang2.a, c2.unit)),
                                c2.unit) +
               ".";
    };

    if (apart == 0)
        return err(ErrorCode::InvalidArgument,
                   "kes(): iki doğrultu paralel, kesişmiyorlar." + both_angles());

    const Point2 a2 = a + core::polar_offset(kDirectionRay, ang1.a, c1);
    const Point2 b2 = b + core::polar_offset(kDirectionRay, ang2.a, c2);

    Point2 out;
    double t = 0.0;
    double u = 0.0;
    if (!core::line_intersection(a, a2, b, b2, out, t, u))
        return err(ErrorCode::InvalidArgument, "kes(): iki doğrultu kesişmiyor." + both_angles());
    return out;
}

/// Which of the two circle solutions the fifth argument asked for.
core::Result<Point2> pick_side(const Token& which, Point2 left, Point2 right, Point2 last,
                               const ResolveContext& ctx)
{
    if (which.kind == Token::Kind::Word) return which.word == "SOL" ? left : right;

    auto near = resolve_point(which, last, ctx);
    if (!near) return near.error();

    const double to_left  = core::distance_squared(near.value(), left);
    const double to_right = core::distance_squared(near.value(), right);
    if (to_left == to_right)
        return err(ErrorCode::InvalidArgument,
                   "kes(): yön noktası iki çözüme eşit uzaklıkta, hangisi olduğu belli değil. "
                   "`yon=sol` ya da `yon=sağ` yazın.");
    return to_left < to_right ? left : right;
}

/// The two-distance intersection, with the message each way of missing gets.
core::Result<Point2> distance_crossing(Point2 a, double r1_m, Point2 b, double r2_m,
                                       const Token& which, Point2 last, const ResolveContext& ctx)
{
    if (r1_m < 0.0 || r2_m < 0.0)
        return err(
            ErrorCode::InvalidArgument,
            "kes(): yarıçap negatif olamaz. Girilen: " + metres_text(core::mm_from_metres(r1_m)) +
                " m ve " + metres_text(core::mm_from_metres(r2_m)) + " m.");

    const Mm r1 = core::mm_from_metres(r1_m);
    const Mm r2 = core::mm_from_metres(r2_m);

    Point2 left;
    Point2 right;
    const core::CircleMeet meet = core::circle_intersection(a, r1, b, r2, left, right);

    // Every refusal names both radii AND the distance between the centres, so
    // the user can see at a glance which measurement is the wrong one (R19).
    const auto figures = [&] {
        return " Yarıçaplar " + metres_text(r1) + " m ve " + metres_text(r2) +
               " m, merkezler arası " + metres_text(core::segment_length(a, b)) + " m.";
    };

    const char* reason = "kes(): çemberler kesişmiyor.";
    switch (meet) {
    case core::CircleMeet::Two:
    case core::CircleMeet::Tangent: return pick_side(which, left, right, last, ctx);
    case core::CircleMeet::TooFar: reason = "kes(): çemberler birbirine ulaşmıyor."; break;
    case core::CircleMeet::Nested: reason = "kes(): bir çember ötekinin tamamen içinde."; break;
    case core::CircleMeet::SameCentre:
        reason = "kes(): iki merkez aynı nokta, kesişim tek bir nokta değil.";
        break;
    }
    return err(ErrorCode::InvalidArgument, reason + figures());
}

/// Where the line A→B crosses the line C→D.
core::Result<Point2> line_crossing(Point2 a, Point2 b, Point2 c, Point2 d)
{
    if (a == b || c == d)
        return err(ErrorCode::InvalidArgument,
                   "kes(): bir doğrunun iki noktası aynı, doğrultu tanımsız.");

    Point2 out;
    double t = 0.0;
    double u = 0.0;
    if (!core::line_intersection(a, b, c, d, out, t, u))
        return err(ErrorCode::InvalidArgument, "kes(): iki doğru paralel, kesişmiyorlar.");
    return out;
}

} // namespace

// ------------------------------------------------------------- the seam ----

bool is_call_text(std::string_view text)
{
    if (text.size() < 3 || text.back() != ')') return false;

    const std::size_t open = text.find('(');
    if (open == 0 || open == std::string_view::npos) return false;

    const std::string_view name = text.substr(0, open);
    if (shape_of(name) != Shape::Name) return false;
    for (char c : name)
        if (c == ',' || c == '<' || c == '=' || c == '@' || c == ')' || c == '"' || is_blank(c))
            return false;

    // The closing parenthesis must be THE last character and must close the
    // first one, so `n(1)+n(2)` is not read as a call named `n`.
    int depth = 0;
    for (std::size_t i = open; i < text.size(); ++i) {
        if (text[i] == '(')
            ++depth;
        else if (text[i] == ')')
            --depth;
        if (depth == 0 && i + 1 != text.size()) return false;
    }
    if (depth != 0) return false;

    std::vector<const PointForm*> forms;
    forms_named(core::turkish_fold_key(name), forms);
    return !forms.empty();
}

core::Result<Token> parse_call(std::string_view text, int depth)
{
    if (depth > kMaxCallDepth)
        return err(ErrorCode::ParseError,
                   "Nokta fonksiyonları en fazla " + std::to_string(kMaxCallDepth) +
                       " kat iç içe yazılır. Girilen: '" + std::string(text) + "'");

    const std::size_t open      = text.find('(');
    const std::string_view name = text.substr(0, open);
    const std::vector<std::string_view> fields =
        split_fields(text.substr(open + 1, text.size() - open - 2));

    std::vector<const PointForm*> forms;
    forms_named(core::turkish_fold_key(name), forms);

    std::vector<Token> matched;
    const PointForm* took = nullptr;
    std::string single_why;
    for (const PointForm* form : forms) {
        std::vector<Token> out;
        std::string why;
        if (!try_form(*form, fields, name, depth, out, why)) {
            if (forms.size() == 1) single_why = why;
            continue;
        }
        // A second shape fitting the same text would mean the grammar cannot
        // say what the user meant, and guessing is what P1a-9 forbids. No pair
        // of shapes declared above can both fit — a bare word and a coordinate
        // are disjoint, and so are a number and a point — but a shape added
        // later could, and it must be refused rather than silently ordered.
        if (took != nullptr)
            return err(ErrorCode::ParseError,
                       std::string(name) + "(): argümanlar iki biçime birden uyuyor — " +
                           took->syntax + " ve " + form->syntax +
                           ". Hangisi olduğunu yazın. Girilen: '" + std::string(text) + "'");
        matched = std::move(out);
        took    = form;
    }

    if (took == nullptr) {
        if (forms.size() == 1 && !single_why.empty()) return err(ErrorCode::ParseError, single_why);

        std::string shapes;
        for (const PointForm* form : forms) {
            if (!shapes.empty()) shapes += " · ";
            shapes += form->syntax;
        }
        return err(ErrorCode::ParseError,
                   std::string(name) + "(): argümanlar hiçbir biçime uymuyor. Biçimler: " + shapes +
                       ". Girilen: '" + std::string(text) + "'");
    }

    Token t;
    t.kind   = Token::Kind::Call;
    t.word   = took->form;
    t.nested = std::move(matched);
    return t;
}

core::Result<Point2> resolve_call(const Token& t, Point2 last, const ResolveContext& ctx)
{
    const std::string& form = t.word;

    // Every argument that is a point is resolved against the SAME `last` the
    // call itself was written against, so `orta(@50,0,@0,50)` means what it
    // reads: both offsets from the point before. `ile` is the one exception and
    // says so where it is resolved.
    const auto point_at  = [&](std::size_t i) { return resolve_point(t.nested[i], last, ctx); };
    const auto number_at = [&](std::size_t i) { return t.nested[i].a; };

    if (form == "son") return last;

    if (form == "n") {
        const double raw = number_at(0);
        if (raw != std::floor(raw) || raw < -9.0e15 || raw > 9.0e15)
            return err(ErrorCode::InvalidArgument,
                       "n(): nokta numarası tam sayı olmalı. Girilen: " + describe(t.nested[0]));
        if (!ctx.named_point)
            return err(ErrorCode::InvalidArgument,
                       "n(): bu bağlamda çizim yok, numaralı nokta aranamaz.");

        const auto no    = static_cast<std::int64_t>(raw);
        const auto found = ctx.named_point(no);
        if (!found)
            return err(ErrorCode::NotFound,
                       std::to_string(no) +
                           " numaralı nokta yok. Nokta listesini NOKTALAR ile okuyun.");
        return *found;
    }

    if (form == "orta") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(1);
        if (!b) return b.error();
        return Point2{core::mm_round(0.5 * (static_cast<double>(a.value().x) +
                                            static_cast<double>(b.value().x))),
                      core::mm_round(0.5 * (static_cast<double>(a.value().y) +
                                            static_cast<double>(b.value().y)))};
    }

    if (form == "ile") {
        auto p = point_at(0);
        if (!p) return p.error();
        // MEASURED FROM P, not from the point before: that is the whole function.
        return resolve_point(t.nested[1], p.value(), ctx);
    }

    if (form == "dik") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(1);
        if (!b) return b.error();
        return perpendicular(a.value(), b.value(), number_at(2), number_at(3));
    }

    if (form == "semt") {
        auto s = point_at(0);
        if (!s) return s.error();
        return s.value() +
               core::polar_offset(number_at(2), number_at(1), applied_to(t.nested[1], ctx));
    }

    if (form == "kes.dogrultu") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(2);
        if (!b) return b.error();
        return direction_crossing(a.value(), t.nested[1], b.value(), t.nested[3], ctx);
    }

    if (form == "kes.mesafe") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(2);
        if (!b) return b.error();
        return distance_crossing(a.value(), number_at(1), b.value(), number_at(3), t.nested[4],
                                 last, ctx);
    }

    if (form == "kes.dogru") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(1);
        if (!b) return b.error();
        auto c = point_at(2);
        if (!c) return c.error();
        auto d = point_at(3);
        if (!d) return d.error();
        return line_crossing(a.value(), b.value(), c.value(), d.value());
    }

    if (form == "ara") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(1);
        if (!b) return b.error();

        const double dx = static_cast<double>(b.value().x - a.value().x);
        const double dy = static_cast<double>(b.value().y - a.value().y);

        double ratio = number_at(2);
        if (t.nested.size() > 3) { // the `m` token: metres, not a fraction
            const double len = std::sqrt(dx * dx + dy * dy);
            if (len == 0.0)
                return err(ErrorCode::InvalidArgument,
                           "ara(): A ve B aynı nokta, üzerinde mesafe ölçülecek doğru yok.");
            ratio = ratio * static_cast<double>(core::kMmPerMetre) / len;
        }
        return Point2{a.value().x + core::mm_round(ratio * dx),
                      a.value().y + core::mm_round(ratio * dy)};
    }

    if (form == "uzanti") {
        auto a = point_at(0);
        if (!a) return a.error();
        auto b = point_at(1);
        if (!b) return b.error();
        return beyond(a.value(), b.value(), number_at(2));
    }

    if (form == "xy") {
        auto p = point_at(0);
        if (!p) return p.error();
        auto q = point_at(1);
        if (!q) return q.error();
        return Point2{p.value().x, q.value().y};
    }

    return err(ErrorCode::InvalidArgument, "Bilinmeyen nokta fonksiyonu: '" + form + "'");
}

std::string describe_call(const Token& t)
{
    std::string out(display_name(t.word));
    if (t.nested.empty()) return out;

    out += '(';
    for (std::size_t i = 0; i < t.nested.size(); ++i) {
        if (i != 0) out += ',';
        out += describe(t.nested[i]);
    }
    out += ')';
    return out;
}

} // namespace kentos::command::detail
