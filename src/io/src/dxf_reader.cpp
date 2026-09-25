// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: reading a DXF through libdxfrw.
//
// The library parses the file and calls one method here per table entry, block
// and entity; this file turns each call into a document entity through the
// transaction, with every loss said (io.md P11/P13). Nothing here reads the file
// itself, nothing here writes the document outside the transaction (Article 5.9),
// and the DRW_* types stay inside this .cpp (io.md R2).
#include "kentos_cad/io/dxf.hpp"

#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/spline.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/io/vector.hpp"

#include "dxf_common.hpp"
#include "dxf_multileader.hpp"
#include "dxf_units.hpp"
#include "mapped_file.hpp"
#include "prj_sidecar.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef KENTOS_HAVE_DXFRW
#include <drw_entities.h>
#include <drw_header.h>
#include <drw_interface.h>
#include <drw_objects.h>
#include <libdxfrw.h>
#endif

namespace kentos::io {

using core::err;
using core::ErrorCode;

#ifndef KENTOS_HAVE_DXFRW

command::Task<core::Result<DxfReport>> import_dxf(command::Transaction& tx, std::string path,
                                                  ImportOptions options, std::stop_token stop)
{
    (void)tx;
    (void)options;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  "'" + path +
                      "' libdxfrw ile okunamaz: bu yapı KENTOS_WITH_DXFRW=OFF ile "
                      "derlendi. " +
                      dxf_backend_status());
}

#else

namespace {

/// libdxfrw hands out DXF group-code bit flags in a signed `int`. The bits are
/// positive by definition of the format, so the test is done unsigned: a sign bit
/// in a mask is a bug everywhere else in this program and must stay detectable.
constexpr bool has_bit(int flags, unsigned bit) noexcept
{
    return (static_cast<unsigned>(flags) & bit) != 0U;
}

using core::Mm;
using core::Point2;

/// A 2-D affine map: the accumulated INSERT transforms of nested blocks, in
/// drawing units. `x' = a·x + b·y + e`, `y' = c·x + d·y + f`.
struct Xform
{
    double a{1}, b{0}, c{0}, d{1}, e{0}, f{0};

    /// The scale a length gets, when the map scales uniformly.
    double uniform_scale() const noexcept { return std::sqrt(std::abs(a * d - b * c)); }

    /// Whether lengths scale the same along every direction — a circle stays one.
    bool uniform() const noexcept
    {
        const double la = a * a + c * c, lb = b * b + d * d, dot = a * b + c * d;
        return std::abs(la - lb) <= 1e-9 * std::max(la, lb) &&
               std::abs(dot) <= 1e-9 * std::max(la, lb);
    }

    bool mirrored() const noexcept { return a * d - b * c < 0.0; }

    /// The rotation the map applies to the x axis, in micro-degrees.
    std::int64_t rotation_udeg() const noexcept { return dxf::udeg_from_radians(std::atan2(c, a)); }

    dxf::Pt apply(double x, double y) const noexcept
    {
        return dxf::Pt{a * x + b * y + e, c * x + d * y + f};
    }

    /// `this` after `inner`: apply `inner` first, then this.
    Xform then(const Xform& inner) const noexcept
    {
        Xform o;
        o.a = a * inner.a + b * inner.c;
        o.b = a * inner.b + b * inner.d;
        o.c = c * inner.a + d * inner.c;
        o.d = c * inner.b + d * inner.d;
        o.e = a * inner.e + b * inner.f + e;
        o.f = c * inner.e + d * inner.f + f;
        return o;
    }
};

/// What an INSERT hands down to the members it expands: the layer that stands
/// in for "0", the colour that stands in for ByBlock.
struct Inherit
{
    std::string layer;
    int color{DRW::ColorByLayer};
    int color24{-1};
};

/// OCS → WCS by the arbitrary axis algorithm, for an entity whose normal is not
/// +Z. Points of 2-D entities (circle, arc, polyline, text, insert) are given in
/// the entity's own coordinate system when the extrusion is tilted or mirrored.
struct Ocs
{
    double ax[3]{1, 0, 0}, ay[3]{0, 1, 0}, az[3]{0, 0, 1};
    bool identity{true};

    static Ocs from_normal(const DRW_Coord& n)
    {
        Ocs o;
        const double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len == 0.0) return o;
        const double nz[3]{n.x / len, n.y / len, n.z / len};
        if (std::abs(nz[0]) < 1e-12 && std::abs(nz[1]) < 1e-12 && nz[2] > 0.0) return o;
        o.identity = false;
        // Ax = (Wy × N) when |Nx|,|Ny| < 1/64, else (Wz × N); Ay = N × Ax.
        double ax[3];
        if (std::abs(nz[0]) < 1.0 / 64.0 && std::abs(nz[1]) < 1.0 / 64.0) {
            ax[0] = nz[2];
            ax[1] = 0.0;
            ax[2] = -nz[0];
        } else {
            ax[0] = -nz[1];
            ax[1] = nz[0];
            ax[2] = 0.0;
        }
        const double al = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
        for (double& v : ax)
            v /= al;
        const double ay[3]{nz[1] * ax[2] - nz[2] * ax[1], nz[2] * ax[0] - nz[0] * ax[2],
                           nz[0] * ax[1] - nz[1] * ax[0]};
        for (int i = 0; i < 3; ++i) {
            o.ax[i] = ax[i];
            o.ay[i] = ay[i];
            o.az[i] = nz[i];
        }
        return o;
    }

    /// Whether the normal is (0,0,−1): a plain mirror in x, which keeps circles
    /// circles. Anything else tilted is flattened onto the drawing plane.
    bool mirror_only() const noexcept { return !identity && std::abs(az[2] + 1.0) < 1e-12; }

    void to_wcs(double& x, double& y, double& z) const noexcept
    {
        if (identity) return;
        const double wx = ax[0] * x + ay[0] * y + az[0] * z;
        const double wy = ax[1] * x + ay[1] * y + az[1] * z;
        const double wz = ax[2] * x + ay[2] * y + az[2] * z;
        x               = wx;
        y               = wy;
        z               = wz;
    }
};

/// An INSERT met inside a BLOCK definition, kept until every definition has
/// been read: a nested reference may name a block the file defines later, and
/// a reference needs its definition to exist (R45) before it is placed.
struct PendingInsert
{
    /// HELD BEHIND A POINTER, not by value. `DRW_Insert` inherits a container
    /// whose move is a copy, so a vector of these allocates a whole entity every
    /// time it grows — and a move that allocates is a move that can throw.
    std::unique_ptr<DRW_Insert> data;
    core::BlockId in_block{core::kNoBlock};
};

/// A DIMSTYLE row's figures, in drawing units, already multiplied by DIMSCALE.
struct DimStyleFigures
{
    double arrow{2.5};
    double extension_beyond{1.25};
    double extension_offset{0.625};
    double text_gap{0.625};
    double text_height{2.5};
    int precision{2};
    char separator{','};
};

constexpr int kStopStride = 4096;

// ------------------------------------------------------- pattern lines ----
//
// libdxfrw hands a HATCH's boundary over and drops the definition lines that
// follow its group 78 — the lines the pattern is drawn with in that file, at
// that scale, from that origin. Without them a pattern the catalogue does not
// know draws as nothing, and one it does know draws from the catalogue's
// numbers rather than the file's. So they are read here, straight from the
// text, for the hatches that announced some.

/// One HATCH record's definition lines, with its handle (0 when it has none).
struct HatchRecordLines
{
    std::uint32_t handle{0};
    std::vector<dxf::PatternLine> lines;
    bool bad{false}; ///< the lines did not add up to what group 78 announced
};

/// Bounds on what one hatch may announce: no pattern file has patterns of this
/// size, and a hostile file must not be able to allocate past them.
constexpr std::size_t kMaxPatternLines  = 4096;
constexpr std::size_t kMaxPatternDashes = 256;

/// The next line of `text` from `at`, its end-of-line dropped; `at` moves past it.
std::string_view next_line(std::string_view text, std::size_t& at) noexcept
{
    if (at >= text.size()) return {};
    const std::size_t end = std::min(text.find('\n', at), text.size());
    std::string_view line(text.data() + at, end - at);
    at = end == text.size() ? end : end + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        line.remove_suffix(1);
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
        line.remove_prefix(1);
    return line;
}

template<class T> bool number_in(std::string_view text, T& out, int base = 10) noexcept
{
    const char* b = text.data();
    const char* e = text.data() + text.size();
    if constexpr (std::is_floating_point_v<T>) {
        (void)base;
        const auto r = std::from_chars(b, e, out);
        return r.ec == std::errc{} && r.ptr == e;
    } else {
        const auto r = std::from_chars(b, e, out, base);
        return r.ec == std::errc{} && r.ptr == e;
    }
}

/// Every HATCH record of an ASCII DXF, in file order, with the lines each one
/// carries after group 78. A binary DXF gives nothing: the catalogue stands in.
std::vector<HatchRecordLines> scan_hatch_lines(std::string_view text)
{
    std::vector<HatchRecordLines> out;
    if (text.starts_with("AutoCAD Binary DXF")) return out;
    std::size_t from = 0;
    while (true) {
        // A record starts where a `0` group names it: the line before "HATCH"
        // is its group code, and a code line is never a value line.
        const std::size_t hit = text.find("HATCH", from);
        if (hit == std::string_view::npos) break;
        from                         = hit + 5;
        const std::size_t line_start = text.rfind('\n', hit == 0 ? 0 : hit - 1);
        const std::size_t begin      = line_start == std::string_view::npos ? 0 : line_start + 1;
        std::size_t after            = begin;
        if (next_line(text, after) != "HATCH") continue;
        if (begin < 2) continue;
        const std::size_t code_start = text.rfind('\n', begin - 2);
        std::size_t at               = code_start == std::string_view::npos ? 0 : code_start + 1;
        if (next_line(text, at) != "0") continue;

        HatchRecordLines record;
        std::size_t announced = 0; // group 78: how many lines follow
        std::size_t dashes    = 0; // group 79 of the line being read
        bool reading          = false;
        // A line is whole when it holds the dashes its group 79 announced.
        const auto whole = [&record, &dashes] {
            return record.lines.empty() || record.lines.back().dashes.size() == dashes;
        };
        at = after;
        while (at < text.size() && !record.bad) {
            const std::string_view code_text = next_line(text, at);
            const std::string_view value     = next_line(text, at);
            int code                         = -1;
            if (!number_in(code_text, code)) {
                record.bad = true;
                break;
            }
            if (code == 0) break;
            if (code == 5 && record.handle == 0) {
                (void)number_in(value, record.handle, 16);
                continue;
            }
            if (code == 78) {
                int n      = -1;
                record.bad = !number_in(value, n) || n < 0 || std::cmp_greater(n, kMaxPatternLines);
                if (record.bad) break;
                announced = static_cast<std::size_t>(n);
                record.lines.reserve(announced);
                reading = announced > 0;
                continue;
            }
            if (!reading) continue;
            if (code == 53) {
                record.bad = !whole() || record.lines.size() == announced;
                if (record.bad) break;
                record.lines.emplace_back();
                dashes     = 0;
                record.bad = !number_in(value, record.lines.back().angle_deg);
                continue;
            }
            dxf::PatternLine* line = record.lines.empty() ? nullptr : &record.lines.back();
            double v               = 0.0;
            int n                  = -1;
            switch (code) {
            case 43: record.bad = line == nullptr || !number_in(value, line->base_x); break;
            case 44: record.bad = line == nullptr || !number_in(value, line->base_y); break;
            case 45: record.bad = line == nullptr || !number_in(value, line->offset_x); break;
            case 46: record.bad = line == nullptr || !number_in(value, line->offset_y); break;
            case 79:
                record.bad = line == nullptr || !number_in(value, n) || n < 0 ||
                             std::cmp_greater(n, kMaxPatternDashes);
                if (!record.bad) dashes = static_cast<std::size_t>(n);
                break;
            case 49:
                record.bad =
                    line == nullptr || line->dashes.size() >= dashes || !number_in(value, v);
                if (!record.bad) line->dashes.push_back(v);
                break;
            default:
                // The lines are over: the pixel size, the seed points, a gradient.
                reading = false;
                break;
            }
        }
        if (record.lines.size() != announced || !whole()) record.bad = true;
        if (record.bad) record.lines.clear();
        out.push_back(std::move(record));
    }
    return out;
}

/// The receiver of everything libdxfrw parses.
class DxfSink final : public DRW_Interface
{
public:
    using LayerId = core::LayerId;

    DxfSink(command::Transaction& tx, const ImportOptions& options, std::stop_token stop)
        : tx_(tx), options_(options), stop_(std::move(stop)), unit_(options.drawing_unit)
    {
        report_.diagnostics.unit        = unit_;
        report_.diagnostics.unit_source = UnitSource::Setting;
    }

    DxfReport& report() noexcept { return report_; }

    bool cancelled() const noexcept { return cancelled_; }

    const core::Error* failure() const noexcept { return failed_ ? &error_ : nullptr; }

    void set_crs(std::string crs) { report_.crs = std::move(crs); }

    // ---- header and tables --------------------------------------------------

    void addHeader(const DRW_Header* data) override
    {
        if (data == nullptr) return;
        // The header's getters are the library's own; the variable map is public.
        const auto var = [data](const char* key) -> const DRW_Variant* {
            const auto it = data->vars.find(key);
            return it == data->vars.end() ? nullptr : it->second;
        };
        if (const DRW_Variant* v = var("$INSUNITS");
            v != nullptr && v->type() == DRW_Variant::INTEGER) {
            const int code = v->content.i;
            if (const auto known = drawing_unit_from_insunits(code); known.has_value())
                report_.diagnostics.declared_unit = known;
            else if (code != 0)
                note(Severity::Warning,
                     "$INSUNITS=" + std::to_string(code) +
                         (insunits_code_known(code) ? " bu sürümde çevrilmiyor"
                                                    : " DXF'in tanımadığı bir kod") +
                         "; çizim " + core::drawing_unit_name(unit_) +
                         " olarak okundu (AYAR çizim_birimi).");
        }
        codepage_seen_ = var("$DWGCODEPAGE") != nullptr;
        if (const DRW_Variant* v = var("$LTSCALE");
            v != nullptr && v->type() == DRW_Variant::DOUBLE && v->content.d != 1.0)
            note(Severity::Info, "Çizgi tipi ölçeği ($LTSCALE " + trim_number(v->content.d) +
                                     ") bu sürümde uygulanmadı; çizgi tipleri bire bir.");
    }

    void addLType(const DRW_LType& data) override
    {
        ++ltypes_;
        (void)data;
    }

    void addLayer(const DRW_Layer& data) override
    {
        LayerInfo info;
        info.name                                  = data.name;
        info.frozen                                = has_bit(data.flags, 1);
        info.locked                                = has_bit(data.flags, 4);
        info.off                                   = data.color < 0;
        info.color                                 = std::abs(data.color);
        info.color24                               = data.color24;
        info.weight                                = DRW_LW_Conv::lineWidth2dxfInt(data.lWeight);
        layers_[core::turkish_fold_key(data.name)] = std::move(info);
    }

    void addDimStyle(const DRW_Dimstyle& data) override
    {
        // The style's figures, as the file meant them: DIMSCALE multiplies every
        // length, and a zero scale means "fit the paper", read as 1.
        DimStyleFigures f;
        const double scale                            = data.dimscale > 0.0 ? data.dimscale : 1.0;
        f.arrow                                       = data.dimasz * scale;
        f.extension_beyond                            = data.dimexe * scale;
        f.extension_offset                            = data.dimexo * scale;
        f.text_gap                                    = data.dimgap * scale;
        f.text_height                                 = data.dimtxt * scale;
        f.precision                                   = std::clamp(data.dimdec, 0, 8);
        f.separator                                   = data.dimdsep == '.' ? '.' : ',';
        dimstyles_[core::turkish_fold_key(data.name)] = f;
    }

    void addVport(const DRW_Vport&) override {}

    void addTextStyle(const DRW_Textstyle& data) override
    {
        // Which face each text style asks for, so a caption can be counted
        // against it (`count_face`). A big-font-only style names its face there.
        const std::string face = !data.font.empty() ? data.font : data.bigFont;
        if (!data.name.empty() && !face.empty())
            style_faces_[core::turkish_fold_key(data.name)] = face;
    }

    void addAppId(const DRW_AppId&) override {}

    // ---- blocks -------------------------------------------------------------

    void addBlock(const DRW_Block& data) override
    {
        // The two space blocks and the anonymous ones (`*D1`, a dimension's own
        // drawing) are containers the file always has; a definition is anything
        // else, and it goes into the document's block table (model.md R45) so
        // its members are read once and every INSERT draws them from there.
        if (data.name.empty() || data.name[0] == '*') {
            skipping_block_ = true;
            return;
        }
        auto made =
            tx_.add_block(data.name, "", to_mm(dxf::Pt{data.basePoint.x, data.basePoint.y}));
        if (!made) {
            fail(err(made.error().code,
                     "BLOCK '" + data.name + "' okunamadı: " + made.error().message));
            skipping_block_ = true;
            return;
        }
        in_block_                                  = made.value();
        blocks_[core::turkish_fold_key(data.name)] = in_block_;
        ++blocks_read_;
        // AN XREF (group 70 bit 4, overlay bit 8): its objects live in another
        // file, and this reader does not get its path (group 1) from the
        // library — so it arrives an empty block, and that is SAID, by name,
        // rather than left to look like a block that draws nothing.
        if ((static_cast<unsigned>(data.flags) & 0x0CU) != 0U) xrefs_.push_back(data.name);
    }

    void setBlock(const int) override {}

    void endBlock() override
    {
        in_block_       = core::kNoBlock;
        skipping_block_ = false;
    }

    // ---- entities -----------------------------------------------------------

    void addPoint(const DRW_Point& data) override
    {
        defer_or_emit(data, [this](const DRW_Point& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "POINT", in)) return;
            double px = e.basePoint.x, py = e.basePoint.y, pz = e.basePoint.z;
            const Ocs ocs = Ocs::from_normal(e.extPoint);
            ocs.to_wcs(px, py, pz);
            const dxf::Pt w = x.apply(px, py);
            auto made       = place_point(layer_for(e, in), to_mm(w));
            finish(e, "POINT", made, {pz});
        });
    }

    void addLine(const DRW_Line& data) override
    {
        defer_or_emit(data, [this](const DRW_Line& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "LINE", in)) return;
            const dxf::Pt a = x.apply(e.basePoint.x, e.basePoint.y);
            const dxf::Pt b = x.apply(e.secPoint.x, e.secPoint.y);
            const Point2 pts[2]{to_mm(a), to_mm(b)};
            if (pts[0] == pts[1]) {
                skip("LINE", "sıfır uzunlukta çizgi");
                return;
            }
            auto made = place_polyline(layer_for(e, in), std::span<const Point2>(pts, 2));
            finish(e, "LINE", made, {e.basePoint.z, e.secPoint.z});
        });
    }

    void addRay(const DRW_Ray& data) override
    {
        defer_skip(data, "RAY", "sonsuz ışın çizimde durmaz");
    }

    void addXline(const DRW_Xline& data) override
    {
        defer_skip(data, "XLINE", "sonsuz doğru çizimde durmaz");
    }

    void addArc(const DRW_Arc& data) override
    {
        defer_or_emit(data, [this](const DRW_Arc& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "ARC", in)) return;
            emit_arc(e, x, in, e.staangle, e.endangle, "ARC");
        });
    }

    void addCircle(const DRW_Circle& data) override
    {
        defer_or_emit(data, [this](const DRW_Circle& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "CIRCLE", in)) return;
            // libdxfrw has already turned an OCS centre into WCS (`applyExtrusion`);
            // a tilted normal is counted, its circle drawn where its centre landed.
            note_tilt(e.extPoint);
            const double cx = e.basePoint.x, cy = e.basePoint.y, cz = e.basePoint.z;
            if (x.uniform()) {
                const dxf::Pt c = x.apply(cx, cy);
                const Mm r      = to_mm_len(e.radious * x.uniform_scale());
                if (r <= 0) {
                    skip("CIRCLE", "yarıçap bir milimetrenin altında");
                    return;
                }
                auto made = place_circle(layer_for(e, in), to_mm(c), r);
                finish(e, "CIRCLE", made, {cz});
                return;
            }
            // Stretched by a non-uniform INSERT: exactly an ellipse.
            const dxf::Pt c  = x.apply(cx, cy);
            const dxf::Pt ma = x.apply(cx + e.radious, cy);
            const dxf::Pt mi = x.apply(cx, cy + e.radious);
            auto made        = place_ellipse(layer_for(e, in), to_mm(c), to_mm(ma), to_mm(mi), {});
            finish(e, "CIRCLE", made, {cz});
        });
    }

    void addEllipse(const DRW_Ellipse& data) override
    {
        defer_or_emit(data, [this](const DRW_Ellipse& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "ELLIPSE", in)) return;
            note_tilt(e.extPoint); // the library extruded the centre already
            const double cx = e.basePoint.x, cy = e.basePoint.y, cz = e.basePoint.z;
            const double mx = e.secPoint.x, my = e.secPoint.y;
            // The minor axis is the major turned a quarter turn, scaled by ratio.
            const double nx = -my * e.ratio, ny = mx * e.ratio;
            const dxf::Pt c  = x.apply(cx, cy);
            const dxf::Pt ma = x.apply(cx + mx, cy + my);
            const dxf::Pt mi = x.apply(cx + nx, cy + ny);
            const bool full  = std::abs(e.endparam - e.staparam) >= 2.0 * core::kPi - 1e-9 ||
                              (e.staparam == 0.0 && e.endparam == 0.0);
            if (full) {
                auto made = place_ellipse(layer_for(e, in), to_mm(c), to_mm(ma), to_mm(mi), {});
                finish(e, "ELLIPSE", made, {cz});
                return;
            }
            // A partial ellipse is the same ellipse with a sweep in its payload
            // (core/ellipse.hpp): the parameter angles, counter-clockwise from
            // the first axis, exactly as the file states them.
            const auto norm = [](std::int64_t udeg) {
                udeg %= core::kUDegFullCircle;
                return udeg < 0 ? udeg + core::kUDegFullCircle : udeg;
            };
            std::int64_t s0 = norm(dxf::udeg_from_radians(e.staparam));
            std::int64_t s1 = norm(dxf::udeg_from_radians(e.endparam));
            if (x.mirrored()) std::swap(s0, s1);
            if (s0 == s1) {
                auto made = place_ellipse(layer_for(e, in), to_mm(c), to_mm(ma), to_mm(mi), {});
                finish(e, "ELLIPSE", made, {cz});
                return;
            }
            auto made = place_ellipse(layer_for(e, in), to_mm(c), to_mm(ma), to_mm(mi),
                                      core::encode_ellipse_arc(core::EllipseArc{s0, s1}));
            finish(e, "ELLIPSE", made, {cz});
        });
    }

    void addLWPolyline(const DRW_LWPolyline& data) override
    {
        defer_or_emit(data,
                      [this](const DRW_LWPolyline& e, const Xform& x, const Inherit& in, int) {
                          if (!begin(e, "LWPOLYLINE", in)) return;
                          std::vector<dxf::Pt> verts;
                          std::vector<double> bulges;
                          bool varying = false;
                          for (const auto& v : e.vertlist) {
                              if (!v) continue;
                              verts.push_back(dxf::Pt{v->x, v->y});
                              bulges.push_back(v->bulge);
                              if (v->stawidth != 0.0 || v->endwidth != 0.0) varying = true;
                          }
                          note_tilt(e.extPoint); // vertices arrive in WCS from the library
                          emit_polyline(e, x, in, verts, bulges, has_bit(e.flags, 1), "LWPOLYLINE",
                                        {e.elevation}, varying);
                      });
    }

    void addPolyline(const DRW_Polyline& data) override
    {
        defer_or_emit(data, [this](const DRW_Polyline& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "POLYLINE", in)) return;
            // A 3-D polyline (flag 8) is a polyline whose vertices carry Z; its
            // plan view is what a plan shows, and the Z goes the way every other
            // Z goes (kot or the degradation count). A mesh (16) or a polyface
            // mesh (64) is a surface this program has no kind for.
            if (has_bit(e.flags, 16U | 64U)) {
                skip("POLYLINE", "ağ ya da çok yüzlü ağ (mesh) bu sürümde okunmuyor");
                return;
            }
            std::vector<dxf::Pt> verts;
            std::vector<double> bulges;
            std::vector<double> zs;
            bool varying = e.defstawidth != e.defendwidth;
            for (const auto& v : e.vertlist) {
                if (!v) continue;
                verts.push_back(dxf::Pt{v->basePoint.x, v->basePoint.y});
                zs.push_back(v->basePoint.z);
                bulges.push_back(v->bulge);
                if (v->stawidth != 0.0 || v->endwidth != 0.0) varying = true;
            }
            emit_polyline(e, x, in, verts, bulges, has_bit(e.flags, 1), "POLYLINE", zs, varying);
        });
    }

    void addSpline(const DRW_Spline* data) override
    {
        if (data == nullptr) return;
        defer_or_emit(*data, [this](const DRW_Spline& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "SPLINE", in)) return;
            emit_spline(e, x, in);
        });
    }

    void addKnot(const DRW_Entity&) override {}

    void addInsert(const DRW_Insert& data) override
    {
        defer_or_emit(data, [this](const DRW_Insert& e, const Xform&, const Inherit& in, int) {
            if (!begin(e, "INSERT", in)) return;
            // Inside a definition the reference waits for every block to be
            // known; on the drawing it is placed at once.
            if (in_block_ != core::kNoBlock) {
                pending_inserts_.push_back(
                    PendingInsert{std::make_unique<DRW_Insert>(e), in_block_});
                return;
            }
            place_insert(e, core::kNoBlock);
        });
    }

    void addTrace(const DRW_Trace& data) override
    {
        defer_or_emit(data, [this](const DRW_Trace& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "TRACE", in)) return;
            emit_quad(e, x, in, "TRACE");
        });
    }

    void add3dFace(const DRW_3Dface& data) override
    {
        defer_or_emit(data, [this](const DRW_3Dface& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "3DFACE", in)) return;
            emit_quad(e, x, in, "3DFACE");
        });
    }

    void addSolid(const DRW_Solid& data) override
    {
        defer_or_emit(data, [this](const DRW_Solid& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "SOLID", in)) return;
            emit_quad(e, x, in, "SOLID");
        });
    }

    void addMText(const DRW_MText& data) override
    {
        defer_or_emit(data, [this](const DRW_MText& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "MTEXT", in)) return;
            count_face(e.style);
            // THE ATTACHMENT POINT IS THE ANCHOR (code 71): 1–3 the top row, 4–6
            // the middle, 7–9 the bottom, each left, centre, right — the nine
            // `core::TextAnchor` values, one for one (TODOS C-12).
            core::TextAnchor anchor = core::TextAnchor::BaselineLeft;
            if (const int attach = e.textgen; attach >= 1 && attach <= 9) {
                constexpr std::array<int, 3> kRow{2, 1, 0}; // top, middle, bottom
                anchor = core::text_anchor_at((attach - 1) % 3,
                                              kRow[static_cast<std::size_t>((attach - 1) / 3)]);
            } else {
                ++text_align_approx_;
            }
            // The spacing (44), as a share of the standard pitch; the width (41)
            // the lines break to, when it has one.
            core::TextLines lines;
            if (std::isfinite(e.interlin) && e.interlin > 0.0)
                lines.spacing = static_cast<std::uint16_t>(
                    std::clamp<long>(std::lround(e.interlin * 1000.0), core::kTextSpacingMin,
                                     core::kTextSpacingMax));
            Mm width = 0;
            if (std::isfinite(e.widthscale) && e.widthscale > 0.0) {
                width      = to_mm_len(e.widthscale * x.uniform_scale());
                lines.wrap = width > 0;
            }
            // Code 11 of an MTEXT is its X-axis direction; when the file gave one
            // it stands in for the angle.
            double angle_deg = e.angle;
            if (e.secPoint.x != 0.0 || e.secPoint.y != 0.0)
                angle_deg = std::atan2(e.secPoint.y, e.secPoint.x) * 180.0 / core::kPi;
            emit_text(e, x, in, e.basePoint, dxf::strip_mtext(e.text), e.height, angle_deg, anchor,
                      "MTEXT", lines, width);
        });
    }

    void addText(const DRW_Text& data) override
    {
        defer_or_emit(data, [this](const DRW_Text& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "TEXT", in)) return;
            count_face(e.style);
            // Code 72 is the column (left, centre, right; aligned, middle, fit),
            // 73 the row (baseline, bottom, middle, top).
            int column = 0;
            switch (e.alignH) {
            case DRW_Text::HCenter:
            case DRW_Text::HMiddle: column = 1; break;
            case DRW_Text::HRight: column = 2; break;
            default: break;
            }
            int row = 0;
            switch (e.alignV) {
            case DRW_Text::VMiddle: row = 1; break;
            case DRW_Text::VTop: row = 2; break;
            default: break;
            }
            // "Middle" (72 = 4) is centred both ways, whatever 73 says.
            if (e.alignH == DRW_Text::HMiddle) row = 1;
            const core::TextAnchor anchor = core::text_anchor_at(column, row);

            // WHERE THE ANCHOR IS. A left, baseline text is placed by its first
            // point (10); every other by its alignment point (11) — except
            // ALIGNED and FIT, whose two points are both ends of the line: the
            // first is its start, and the second only its direction. Those, and
            // a bottom row that sits on the descenders rather than the
            // baseline, are drawn the nearest way this program has, and said.
            const bool stretched = e.alignH == DRW_Text::HAligned || e.alignH == DRW_Text::HFit;
            const bool by_second =
                !stretched && (e.alignH != DRW_Text::HLeft || e.alignV != DRW_Text::VBaseLine);
            if (stretched || e.alignV == DRW_Text::VBottom) ++text_align_approx_;
            double angle = e.angle;
            if (stretched && (e.secPoint.x != e.basePoint.x || e.secPoint.y != e.basePoint.y))
                angle = std::atan2(e.secPoint.y - e.basePoint.y, e.secPoint.x - e.basePoint.x) *
                        180.0 / core::kPi;
            emit_text(e, x, in, by_second ? e.secPoint : e.basePoint,
                      dxf::expand_text_codes(e.text), e.height, angle, anchor, "TEXT",
                      core::TextLines{}, 0);
        });
    }

    void addDimAlign(const DRW_DimAligned* d) override
    {
        if (d == nullptr) return;
        const DRW_Coord a = d->getDef1Point();
        const DRW_Coord b = d->getDef2Point();
        const DRW_Coord l = d->getDimPoint();
        emit_dimension(*d, core::DimensionType::Aligned, {a, b, l}, 0.0);
    }

    void addDimLinear(const DRW_DimLinear* d) override
    {
        if (d == nullptr) return;
        const DRW_Coord a = d->getDef1Point();
        const DRW_Coord b = d->getDef2Point();
        const DRW_Coord l = d->getDimPoint();
        emit_dimension(*d, core::DimensionType::Linear, {a, b, l}, d->getAngle());
    }

    void addDimRadial(const DRW_DimRadial* d) override
    {
        if (d == nullptr) return;
        emit_dimension(*d, core::DimensionType::Radial,
                       {d->getCenterPoint(), d->getDiameterPoint()}, 0.0);
    }

    void addDimDiametric(const DRW_DimDiametric* d) override
    {
        if (d == nullptr) return;
        emit_dimension(*d, core::DimensionType::Diametric,
                       {d->getDiameter1Point(), d->getDiameter2Point()}, 0.0);
    }

    void addDimAngular(const DRW_DimAngular* d) override
    {
        if (d == nullptr) return;
        emit_dimension(*d, core::DimensionType::Angular,
                       {d->getFirstLine1(), d->getFirstLine2(), d->getSecondLine1(),
                        d->getSecondLine2(), d->getDimPoint()},
                       0.0);
    }

    void addDimAngular3P(const DRW_DimAngular3p* d) override
    {
        if (d == nullptr) return;
        emit_dimension(
            *d, core::DimensionType::Angular3P,
            {d->getVertexPoint(), d->getFirstLine(), d->getSecondLine(), d->getDimPoint()}, 0.0);
    }

    void addDimOrdinate(const DRW_DimOrdinate* d) override
    {
        if (d == nullptr) return;
        emit_dimension(*d, core::DimensionType::Ordinate,
                       {d->getOriginPoint(), d->getFirstLine(), d->getSecondLine()}, 0.0);
    }

    void addLeader(const DRW_Leader* data) override
    {
        if (data == nullptr) return;
        defer_or_emit(*data, [this](const DRW_Leader& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "LEADER", in)) return;
            std::vector<Point2> pts;
            for (const auto& v : e.vertexlist)
                if (v) push_unique(pts, to_mm(x.apply(v->x, v->y)));
            if (pts.size() < 2) {
                skip("LEADER", "ikiden az köşe");
                return;
            }
            core::LeaderDef def;
            def.arrow      = e.arrow != 0;
            def.spline     = e.leadertype == 1;
            def.arrow_size = to_mm_len(figures_of(e.style).arrow);
            const core::RingGeometry::RingInput ring{pts, core::RingRole::Open, 0};
            auto made = tx_.add_kind(layer_for(e, in), core::kLeaderKind,
                                     std::span<const core::RingGeometry::RingInput>(&ring, 1),
                                     core::encode_leader(def), in_block_);
            finish(e, "LEADER", made, {}, def.spline);
        });
    }

    void addHatch(const DRW_Hatch* data) override
    {
        if (data == nullptr) return;
        // Counted before anything is skipped: the n-th HATCH handed over is the
        // n-th HATCH record of the file, which is how a hatch with no handle
        // finds its pattern lines.
        const std::size_t ordinal = hatches_seen_++;
        defer_or_emit(*data,
                      [this, ordinal](const DRW_Hatch& e, const Xform& x, const Inherit& in, int) {
                          if (!begin(e, "HATCH", in)) return;
                          emit_hatch(e, x, in, ordinal);
                      });
    }

    void addViewport(const DRW_Viewport& data) override
    {
        defer_skip(data, "VIEWPORT", "bakış penceresi çizim değildir");
    }

    void addImage(const DRW_Image* data) override
    {
        if (data == nullptr) return;
        defer_skip(*data, "IMAGE", "raster resim bu sürümde okunmuyor");
    }

    void linkImage(const DRW_ImageDef*) override {}

    void addComment(const char*) override {}

    void addPlotSettings(const DRW_PlotSettings*) override {}

    // ---- the writer half of the interface: unused on a read ----------------
    void writeHeader(DRW_Header&) override {}

    void writeBlocks() override {}

    void writeBlockRecords() override {}

    void writeEntities() override {}

    void writeLTypes() override {}

    void writeLayers() override {}

    void writeTextstyles() override {}

    void writeVports() override {}

    void writeDimstyles() override {}

    void writeObjects() override {}

    void writeAppId() override {}

    // ---- after the read -----------------------------------------------------

    /// The notes that are counts, said once; the census; the layer list.
    core::Status conclude()
    {
        if (failed_) return error_;
        for (const LayerId l : lock_later_)
            if (auto st = tx_.set_layer_locked(l, true); !st) return st;

        if (!pending_inserts_.empty()) flush_pending_inserts();
        if (blocks_read_ != 0)
            note(Severity::Info, std::to_string(blocks_read_) + " blok tanımı ve " +
                                     std::to_string(refs_read_) +
                                     " blok referansı (INSERT) yapısıyla okundu; üyeler blok "
                                     "içinde durur, referans yerleştirir.");
        if (!xrefs_.empty()) {
            std::string names;
            for (const std::string& n : xrefs_)
                names += (names.empty() ? "" : ", ") + n;
            note(Severity::Degraded,
                 std::to_string(xrefs_.size()) + " blok dosyada dış referans (XREF): " + names +
                     ". Dosyaları bu sürümde DXF'ten okunmuyor, boş blok olarak geldi; "
                     "çizime DIŞREFERANS ile bağlayın.");
        }
        if (spline_fit_only_ != 0)
            note(Severity::Degraded, std::to_string(spline_fit_only_) +
                                         " spline yalnız uydurma noktası taşıyordu; uydurma "
                                         "noktaları kontrol noktası olarak alındı.");
        if (hatch_unknown_ != 0)
            note(Severity::Degraded,
                 std::to_string(hatch_unknown_) +
                     " taramanın deseni katalogda yok; sınırı, adı ve açısı korundu, deseni "
                     "çizilmez. Deseni TERCİH desen_kataloğu dosyasına ekleyin.");
        if (dim_style_missing_ != 0)
            note(Severity::Info, std::to_string(dim_style_missing_) +
                                     " ölçünün stili dosyada tanımlı değildi; ISO-25 ölçüleri "
                                     "kullanıldı.");
        if (widths_dropped_ != 0)
            note(Severity::Degraded, std::to_string(widths_dropped_) +
                                         " çoklu çizginin kalınlığı (genişlik) okunmadı.");
        if (linetypes_dropped_ != 0)
            note(Severity::Degraded, std::to_string(linetypes_dropped_) +
                                         " öğenin çizgi tipi okunmadı; LTYPE tablosu bu sürümde "
                                         "uygulanmıyor, çizgiler düz.");
        if (z_varies_ != 0)
            note(Severity::Degraded,
                 std::to_string(z_varies_) +
                     " öğede köşeler farklı yüksekliklerdeydi; çizim iki boyutludur, kot "
                     "yazılmadı.");
        if (kot_written_ != 0)
            note(Severity::Info,
                 std::to_string(kot_written_) + " öğenin yüksekliği (Z) `kot` sütununa yazıldı.");
        if (text_align_approx_ != 0)
            note(Severity::Degraded, std::to_string(text_align_approx_) +
                                         " yazının hizası en yakın desteklenen hizaya çevrildi.");
        if (byblock_seen_ != 0)
            note(Severity::Info, std::to_string(byblock_seen_) +
                                     " öğe rengini bloğundan alıyordu (ByBlock); blok dışında "
                                     "katman rengi kullanıldı.");
        if (tilted_ != 0)
            note(Severity::Degraded, std::to_string(tilted_) +
                                         " öğe eğik bir düzlemdeydi (OCS); çizim düzlemine "
                                         "düzleştirildi.");
        if (xdata_kept_ != 0)
            note(Severity::Info, std::to_string(xdata_kept_) +
                                     " öğenin ek verisi (XDATA) bayt bayt korundu; öznitelik "
                                     "panelinde `ek_veri` olarak sayılır.");
        if (!foreign_faces_.empty()) {
            // EVERY CAPTION IS SET IN THE BUNDLED FACE (TODOS C-12), so one made
            // in another is drawn narrower or wider, and its lines break
            // elsewhere, than its author saw them. Said here, with the faces
            // and their counts, rather than left to be noticed on the sheet.
            std::size_t texts = 0;
            std::string named;
            for (const auto& [face, count] : foreign_faces_) {
                texts += count;
                named +=
                    (named.empty() ? "'" : ", '") + face + "' (" + std::to_string(count) + " yazı)";
            }
            note(Severity::Degraded,
                 std::to_string(texts) + " yazının istediği yazı tipi bu programda yok: " + named +
                     ". Bu yazılar IBM Plex Sans ile çizildi; harf genişlikleri ve satır "
                     "kırılmaları kaynaktan farklı olabilir.");
        }
        if (!codepage_seen_)
            note(Severity::Warning,
                 "Dosya kod sayfası bildirmiyor ($DWGCODEPAGE yok); 2007 öncesi bir dosyada "
                 "Türkçe harfler yanlış çıkabilir.");
        if (skipped_ != 0) {
            report_.diagnostics.skipped        = skipped_;
            report_.diagnostics.skipped_reason = first_skip_reason_;
        }
        report_.diagnostics.paper_space_skipped = paper_space_;
        report_.blocks                          = blocks_read_;
        for (const auto& [folded, info] : layers_) {
            (void)folded;
            if (info.used != 0) report_.layer_names.emplace_back(info.name, info.used);
        }
        std::sort(report_.layer_names.begin(), report_.layer_names.end());
        report_.layers = report_.layer_names.size();
        return core::ok();
    }

private:
    struct LayerInfo
    {
        std::string name;
        bool frozen{false}, locked{false}, off{false};
        int color{7}, color24{-1}, weight{-3};
        LayerId slot{core::kNoLayer};
        bool made{false};
        std::size_t used{0};
    };

    static std::string trim_number(double v)
    {
        char buf[64];
        (void)std::snprintf(buf, sizeof(buf), "%g", v);
        return buf;
    }

    void note(Severity level, std::string text)
    {
        report_.diagnostics.note(level, std::move(text));
    }

    // ---- the per-entity protocol --------------------------------------------

    /// Everything that happens before an entity is looked at: the stop check, the
    /// paper-space and layer filters, the census. False means "do not emit".
    bool begin(const DRW_Entity& e, const char* type, const Inherit& in)
    {
        if (failed_ || cancelled_) return false;
        // The first entity and every 4096th ask for the stop (io.md R15): the
        // first, so a cancel that arrived before the read began is honoured on a
        // three-entity file too.
        ++seen_;
        if ((seen_ == 1 || (seen_ % kStopStride) == 0) && stop_.stop_requested()) {
            cancelled_ = true;
            return false;
        }
        if (e.space == DRW::PaperSpace) {
            ++paper_space_;
            return false;
        }
        // The first entity of the ENTITIES section: every definition is known,
        // so the nested references kept back can be placed (see PendingInsert).
        if (in_block_ == core::kNoBlock && !pending_inserts_.empty()) flush_pending_inserts();
        const std::string& lname = e.layer == "0" && !in.layer.empty() ? in.layer : e.layer;
        // A member is read whatever layer it is on: the filter is about what is
        // drawn, and a member is drawn through its reference's layer.
        if (!options_.only.empty() && in_block_ == core::kNoBlock) {
            bool wanted = false;
            for (const std::string& pick : options_.only)
                if (core::turkish_iequals(pick, lname)) wanted = true;
            if (!wanted) return false;
        }
        (void)type;
        return true;
    }

    template<class E, class Fn> void defer_or_emit(const E& data, Fn fn)
    {
        // Inside an anonymous block nothing is read; inside a named one the
        // entity becomes a MEMBER of the definition (`place`), in the
        // definition's own coordinates — no transform is ever applied here,
        // the reference applies it when it draws.
        if (skipping_block_) return;
        fn(data, Xform{}, Inherit{}, 0);
    }

    template<class E> void defer_skip(const E& data, const char* type, const char* why)
    {
        defer_or_emit(data, [this, type, why](const E& e, const Xform&, const Inherit& in, int) {
            if (!begin(e, type, in)) return;
            skip(type, why);
        });
    }

    void skip(const char* type, const char* why)
    {
        report_.diagnostics.tally(type, 0, 1, 0);
        ++skipped_;
        if (first_skip_reason_.empty()) first_skip_reason_ = std::string(type) + ": " + why;
    }

    /// After a successful add: the census, the style, the handle, the Z, the XDATA.
    template<class E>
    void finish(const E& e, const char* type, const core::Result<command::EntityId>& made,
                std::initializer_list<double> zs, bool degraded = false)
    {
        if (!made) {
            fail(err(made.error().code, std::string(type) + " okunamadı: " + made.error().message));
            return;
        }
        finish_entity(e, type, made.value(), std::vector<double>(zs), degraded);
    }

    template<class E>
    void finish_entity(const E& e, const char* type, command::EntityId id,
                       const std::vector<double>& zs, bool degraded)
    {
        report_.diagnostics.tally(type, 1, 0, degraded ? 1 : 0);
        if (in_block_ == core::kNoBlock) ++report_.entities;
        // The layer's own count, by the name the entity resolved to.
        if (const auto at = layers_.find(core::turkish_fold_key(e.layer)); at != layers_.end())
            ++at->second.used;

        style_of(e, id);
        if (e.handle != DRW::NoHandle) {
            char hex[24];
            (void)std::snprintf(hex, sizeof(hex), "%X", static_cast<unsigned>(e.handle));
            set_text_attr("kaynak_kimlik", "kaynak kimliği", id, hex);
        }
        height_of(zs, id);
        if (!e.extData.empty()) {
            // FOREIGN means another program's. This program's own application
            // group (KENTOSCAD) is decoded into columns and regenerated on every
            // write, so it is left out of the kept bytes — otherwise a drawing
            // that went out and came back would carry itself twice.
            std::vector<std::shared_ptr<DRW_Variant>> foreign;
            bool ours = false;
            for (const auto& v : e.extData) {
                if (!v) continue;
                if (v->code() == 1001)
                    ours = v->type() == DRW_Variant::STRING && v->content.s != nullptr &&
                           *v->content.s == "KENTOSCAD";
                if (!ours) foreign.push_back(v);
            }
            const auto bytes = dxf::encode_xdata(foreign);
            if (!bytes.empty()) {
                if (auto st = tx_.attach_foreign(id, core::kForeignDxfXdata, bytes); st)
                    ++xdata_kept_;
            }
            own_attributes(e.extData, id);
        }
    }

    void fail(core::Error e)
    {
        if (failed_) return;
        failed_ = true;
        error_  = std::move(e);
    }

    // ---- layers and styles ---------------------------------------------------

    /// Counts an entity whose plane is not the drawing plane: the library has
    /// put its points where the plan view shows them, and the loss of the tilt
    /// is said once for the file.
    void note_tilt(const DRW_Coord& normal)
    {
        const bool flat = std::abs(normal.x) < 1e-12 && std::abs(normal.y) < 1e-12;
        if (!flat) ++tilted_;
    }

    LayerId layer_for(const DRW_Entity& e, const Inherit& in)
    {
        const std::string& name = e.layer == "0" && !in.layer.empty() ? in.layer : e.layer;
        return layer_slot(name);
    }

    LayerId layer_slot(const std::string& name)
    {
        const std::string key = core::turkish_fold_key(name);
        auto at               = layers_.find(key);
        if (at == layers_.end()) {
            LayerInfo info;
            info.name = name;
            at        = layers_.emplace(key, std::move(info)).first;
        }
        LayerInfo& info = at->second;
        if (!info.made) {
            info.made = true;
            info.slot = tx_.ensure_layer(info.name);
            if (info.slot == core::kNoLayer) {
                fail(err(ErrorCode::ValidationFailed,
                         "'" + info.name + "' katmanı oluşturulamadı."));
                return core::kNoLayer;
            }
            core::Appearance a{};
            const std::uint32_t rgb = info.color24 >= 0 ? static_cast<std::uint32_t>(info.color24)
                                                        : dxf::aci_ink(info.color);
            a.rgba                  = 0xFF000000u | rgb;
            a.src_colour            = core::Source::Explicit;
            if (info.weight >= 0) {
                a.width_um  = dxf::lineweight_um_from_dxf(info.weight);
                a.src_width = core::Source::Explicit;
            }
            if (auto st = tx_.set_layer_appearance(info.slot, a); !st) fail(st.error());
            if (info.frozen || info.off)
                if (auto st = tx_.set_layer_visible(info.slot, false); !st) fail(st.error());
            if (info.locked) lock_later_.push_back(info.slot);
        }
        return info.slot;
    }

    void style_of(const DRW_Entity& e, command::EntityId id)
    {
        int color   = e.color;
        int color24 = e.color24;
        if (color == DRW::ColorByBlock) {
            if (in_block_ != core::kNoBlock) {
                // A member that takes its colour from the block it is placed by:
                // the source is kept, and the reference resolves it when it draws
                // (block_reference.hpp).
                core::Appearance a{};
                a.src_colour = core::Source::ByBlock;
                if (auto st = tx_.set_entity_style(id, tx_.intern_style(a)); !st) fail(st.error());
                if (!e.visible)
                    if (auto st = tx_.set_entity_hidden(id, true); !st) fail(st.error());
                return;
            }
            ++byblock_seen_;
            color = DRW::ColorByLayer;
        }
        if (!e.lineType.empty() && !core::turkish_iequals(e.lineType, "BYLAYER") &&
            !core::turkish_iequals(e.lineType, "BYBLOCK") &&
            !core::turkish_iequals(e.lineType, "CONTINUOUS"))
            ++linetypes_dropped_;
        const int weight      = DRW_LW_Conv::lineWidth2dxfInt(e.lWeight);
        const bool own_colour = color != DRW::ColorByLayer;
        const bool own_weight = weight >= 0;
        if (!own_colour && !own_weight && e.visible) return;

        core::Appearance a{};
        if (own_colour) {
            const std::uint32_t rgb =
                color24 >= 0 ? static_cast<std::uint32_t>(color24) : dxf::aci_ink(std::abs(color));
            a.rgba       = 0xFF000000u | rgb;
            a.src_colour = core::Source::Explicit;
        }
        if (own_weight) {
            a.width_um  = dxf::lineweight_um_from_dxf(weight);
            a.src_width = core::Source::Explicit;
        }
        if (own_colour || own_weight)
            if (auto st = tx_.set_entity_style(id, tx_.intern_style(a)); !st) fail(st.error());
        if (!e.visible)
            if (auto st = tx_.set_entity_hidden(id, true); !st) fail(st.error());
    }

    // ---- attributes ---------------------------------------------------------

    core::AttrId column(const char* cid, const char* label, core::AttrType type)
    {
        const core::AttrId found = tx_.document().attributes().find(cid);
        if (found != core::kNoAttr) return found;
        core::AttrSpec spec;
        spec.id      = cid;
        spec.name_tr = label;
        spec.type    = type;
        auto made    = tx_.declare_attribute(std::move(spec));
        if (!made) {
            fail(made.error());
            return core::kNoAttr;
        }
        return made.value();
    }

    void set_text_attr(const char* cid, const char* label, command::EntityId id,
                       const std::string& value)
    {
        const core::AttrId col = column(cid, label, core::AttrType::Text);
        if (col == core::kNoAttr) return;
        core::AttrValue v;
        v.type    = core::AttrType::Text;
        v.present = true;
        v.text    = value;
        if (auto st = tx_.set_attribute(col, id, v); !st) fail(st.error());
    }

    /// Counts a caption whose style asks for a face other than the bundled IBM
    /// Plex every caption is drawn in (TODOS C-12). A style the file never
    /// declared is no face at all, and says nothing.
    void count_face(const std::string& style)
    {
        const auto at = style_faces_.find(
            core::turkish_fold_key(style.empty() ? std::string("STANDARD") : style));
        if (at == style_faces_.end()) return;
        const std::string& face = at->second;
        if (face.size() >= 7 && core::turkish_key_equals(face.substr(0, 7), "ibmplex")) return;
        ++foreign_faces_[face];
    }

    /// The value of this program's own note `key` on `e` — a `key=value` string
    /// in its KENTOSCAD XDATA group, which the writer keeps for what a DXF record
    /// cannot say. Empty when the file does not carry it.
    static std::optional<std::string> own_note(const DRW_Entity& e, std::string_view key)
    {
        bool ours = false;
        for (const auto& v : e.extData) {
            if (!v) continue;
            if (v->code() == 1001) {
                ours = v->type() == DRW_Variant::STRING && v->content.s != nullptr &&
                       *v->content.s == "KENTOSCAD";
                continue;
            }
            if (!ours || v->code() != 1000 || v->type() != DRW_Variant::STRING ||
                v->content.s == nullptr)
                continue;
            const std::string& text = *v->content.s;
            if (text.size() > key.size() && text.starts_with(key) && text[key.size()] == '=')
                return text.substr(key.size() + 1);
        }
        return std::nullopt;
    }

    /// This program's own attributes, written by its exporter under the KENTOSCAD
    /// application name as `id=value` strings, come back into the columns the
    /// drawing already declares — the same type, parsed the way the column's type
    /// says. A column the drawing does not have stays in the foreign bytes.
    void own_attributes(const std::vector<std::shared_ptr<DRW_Variant>>& items,
                        command::EntityId id)
    {
        bool ours = false;
        for (const auto& v : items) {
            if (!v) continue;
            if (v->code() == 1001) {
                ours = v->type() == DRW_Variant::STRING && v->content.s != nullptr &&
                       *v->content.s == "KENTOSCAD";
                continue;
            }
            if (!ours || v->code() != 1000 || v->type() != DRW_Variant::STRING ||
                v->content.s == nullptr)
                continue;
            const std::string& text = *v->content.s;
            const auto eq           = text.find('=');
            if (eq == std::string::npos) continue;
            std::string cid       = text.substr(0, eq);
            const std::string raw = text.substr(eq + 1);
            if (cid == "anahtar") continue;         // the source's key is its history, not a cell
            if (cid.starts_with("olcu.")) continue; // a dimension's own notes (`own_note`)

            // `id#T=value`: T is the column's type, so a drawing that never had
            // the column gets it declared with the type the writer meant. The
            // older `id=value` form lands only in a column that already exists.
            std::optional<core::AttrType> typed;
            if (const auto hash = cid.find('#'); hash != std::string::npos) {
                int t         = -1;
                const char* b = cid.data() + hash + 1;
                const char* e = cid.data() + cid.size();
                if (const auto r = std::from_chars(b, e, t);
                    r.ec == std::errc{} && r.ptr == e && t >= 0 &&
                    t <= static_cast<int>(core::AttrType::Date))
                    typed = static_cast<core::AttrType>(t);
                cid.erase(hash);
            }
            core::AttrId col = tx_.document().attributes().find(cid);
            if (col == core::kNoAttr && typed.has_value() && *typed != core::AttrType::CodeRef) {
                core::AttrSpec spec;
                spec.id      = cid;
                spec.name_tr = cid;
                spec.type    = *typed;
                auto made    = tx_.declare_attribute(std::move(spec));
                if (!made) continue;
                col = made.value();
            }
            if (col == core::kNoAttr) continue;
            const core::AttrColumn* held = tx_.document().attributes().column(col);
            if (held == nullptr) continue;
            if (typed.has_value() && held->spec().type != *typed) continue;
            core::AttrValue value;
            value.type    = held->spec().type;
            value.present = true;
            switch (value.type) {
            case core::AttrType::Text:
            case core::AttrType::CodeRef: value.text = raw; break;
            default: {
                char* end         = nullptr;
                const long long n = std::strtoll(raw.c_str(), &end, 10);
                if (end == raw.c_str() || *end != '\0') continue; // not a number: leave it
                value.number = n;
                break;
            }
            }
            if (auto st = tx_.set_attribute(col, id, value); !st) fail(st.error());
        }
    }

    void height_of(const std::vector<double>& zs, command::EntityId id)
    {
        if (zs.empty()) return;
        const double first = zs.front();
        for (const double z : zs)
            if (std::abs(z - first) > 1e-9) {
                ++z_varies_;
                return;
            }
        const Mm kot = to_mm_len(first);
        if (kot == 0) return;
        const core::AttrId col = column("kot", "kot", core::AttrType::Length);
        if (col == core::kNoAttr) return;
        core::AttrValue v;
        v.type    = core::AttrType::Length;
        v.present = true;
        v.number  = kot;
        if (auto st = tx_.set_attribute(col, id, v); !st) fail(st.error());
        ++kot_written_;
    }

    // ---- units --------------------------------------------------------------

    /// A file value to millimetres by THE rounding, and what the rounding took
    /// counted (TODOS F-03): a drawing made in millimetres can carry a 0,3 mm
    /// gap or a 2,5 mm text height, and neither survives the storage unit.
    Mm to_mm_len(double v) noexcept
    {
        const double exact = core::drawing_units_to_mm_exact(v, unit_);
        report_.diagnostics.rounded(exact);
        return core::mm_round(exact);
    }

    Point2 to_mm(const dxf::Pt& p) noexcept { return Point2{to_mm_len(p.x), to_mm_len(p.y)}; }

    // ---- curves -------------------------------------------------------------

    template<class E>
    void emit_arc(const E& e, const Xform& x, const Inherit& in, double sta_rad, double end_rad,
                  const char* type)
    {
        // libdxfrw has already applied the extrusion: a mirrored normal (0,0,−1)
        // moved the centre and mirrored and swapped the angles; the sweep here is
        // the drawn one. Only an INSERT's own mirror is left to honour.
        note_tilt(e.extPoint);
        const double cx = e.basePoint.x, cy = e.basePoint.y, cz = e.basePoint.z;
        std::int64_t s0 = dxf::udeg_from_radians(sta_rad), s1 = dxf::udeg_from_radians(end_rad);
        const bool swap_ends = x.mirrored();

        if (x.uniform()) {
            const dxf::Pt c = x.apply(cx, cy);
            const Point2 cm = to_mm(c);
            const Mm r      = to_mm_len(e.radious * x.uniform_scale());
            if (r <= 0) {
                skip(type, "yarıçap bir milimetrenin altında");
                return;
            }
            const std::int64_t rot = x.rotation_udeg();
            Point2 from            = dxf::point_on_circle(cm, r, s0 + rot);
            Point2 to              = dxf::point_on_circle(cm, r, s1 + rot);
            if (swap_ends) std::swap(from, to);
            auto made = place_arc(layer_for(e, in), cm, r, from, to);
            finish(e, type, made, {cz});
            return;
        }
        // Stretched by a non-uniform INSERT: the arc is an elliptic arc; stroked.
        std::vector<Point2> pts;
        const dxf::Pt c  = x.apply(cx, cy);
        const dxf::Pt ma = x.apply(cx + e.radious, cy);
        const dxf::Pt mi = x.apply(cx, cy + e.radious);
        const Point2 cm  = to_mm(c);
        const Point2 major_vec{to_mm(ma).x - cm.x, to_mm(ma).y - cm.y};
        const Point2 minor_vec{to_mm(mi).x - cm.x, to_mm(mi).y - cm.y};
        if (swap_ends) std::swap(s0, s1);
        dxf::ellipse_arc_points(cm, major_vec, minor_vec, s0, s1, pts);
        ++tilted_;
        auto made = place_polyline(layer_for(e, in), pts);
        finish(e, type, made, {cz}, true);
    }

    template<class E>
    void emit_polyline(const E& e, const Xform& x, const Inherit& in,
                       const std::vector<dxf::Pt>& verts, const std::vector<double>& bulges,
                       bool closed, const char* type, std::vector<double> zs,
                       bool varying_width = false)
    {
        bool degraded = false;
        if (verts.size() < 2) {
            skip(type, "ikiden az köşe");
            return;
        }
        // Vertices in millimetres, repeats dropped; each kept edge remembers the
        // bulge of the file edge it came from.
        std::vector<Point2> pts;
        std::vector<double> edge_bulge;
        pts.reserve(verts.size());
        const std::size_t n = verts.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Point2 a = to_mm(x.apply(verts[i].x, verts[i].y));
            if (!pts.empty() && pts.back() == a) {
                // The repeated vertex's bulge belongs to the edge that leaves it.
                if (!edge_bulge.empty()) edge_bulge.back() = i < bulges.size() ? bulges[i] : 0.0;
                continue;
            }
            pts.push_back(a);
            edge_bulge.push_back(i < bulges.size() ? bulges[i] : 0.0);
        }
        if (closed && pts.size() > 1 && pts.front() == pts.back()) {
            pts.pop_back();
            edge_bulge.pop_back();
        }
        if (pts.size() < 2) {
            skip(type, "bütün köşeler aynı noktada");
            return;
        }

        // AN EDGE THAT BENDS MAKES THE WHOLE THING AN ARC POLYLINE: every bulge
        // becomes an arc's centre, radius and direction (core/arc_polyline.hpp),
        // kept exactly, drawn by the arc routine. Nothing is stroked.
        core::ArcPolyline ap;
        const std::size_t kept = pts.size();
        const std::size_t segs = closed ? kept : kept - 1;
        for (std::size_t i = 0; i < segs; ++i) {
            double bulge = edge_bulge[i];
            if (bulge == 0.0) continue;
            if (x.mirrored()) bulge = -bulge;
            const Point2 a = pts[i];
            const Point2 b = pts[(i + 1) % kept];
            core::ArcPolyline::Arc arc;
            if (!core::arc_from_bulge(a, b, bulge, arc.centre, arc.radius, arc.ccw)) continue;
            arc.segment = static_cast<std::uint32_t>(i);
            ap.arcs.push_back(arc);
        }

        core::Result<command::EntityId> made = err(ErrorCode::Internal, "");
        const LayerId layer                  = layer_for(e, in);
        // A width that varies along the line is not kept by any kind; a constant
        // one rides on an arc polyline and is dropped from a plain one.
        if (varying_width || (ap.arcs.empty() && width_of(e) != 0.0)) {
            ++widths_dropped_;
            degraded = true;
        }
        if (!ap.arcs.empty()) {
            if (const double w = width_of(e); w > 0.0) ap.constant_width = to_mm_len(w);
            const core::RingRole role =
                closed && pts.size() >= 3 ? core::RingRole::Exterior : core::RingRole::Open;
            const core::RingGeometry::RingInput ring{pts, role, 0};
            made = tx_.add_kind(layer, core::kArcPolylineKind,
                                std::span<const core::RingGeometry::RingInput>(&ring, 1),
                                core::encode_arc_polyline(ap), in_block_);
        } else if (closed && pts.size() >= 3) {
            const core::RingGeometry::RingInput ring{pts, core::RingRole::Exterior, 0};
            made = place_area(layer, std::span<const core::RingGeometry::RingInput>(&ring, 1));
        } else {
            made = place_polyline(layer, pts);
        }
        if (!made) {
            fail(err(made.error().code, std::string(type) + " okunamadı: " + made.error().message));
            return;
        }
        finish_entity(e, type, made.value(), zs, degraded);
    }

    /// The constant width a polyline entity declares, or 0.
    static double width_of(const DRW_LWPolyline& e) { return e.width; }

    static double width_of(const DRW_Polyline& e) { return e.defstawidth; }

    static double width_of(const DRW_Spline&) { return 0.0; }

    /// A SPLINE as `core.spline`: control points, degree, knots and weights as the
    /// file states them (core/spline.hpp). A fit-point-only spline has no control
    /// points to keep, so the fit points stand in for them and the loss is said.
    void emit_spline(const DRW_Spline& e, const Xform& x, const Inherit& in)
    {
        std::vector<Point2> controls;
        for (const auto& c : e.controllist)
            if (c) controls.push_back(to_mm(x.apply(c->x, c->y)));
        std::vector<Point2> fits;
        for (const auto& f : e.fitlist)
            if (f) fits.push_back(to_mm(x.apply(f->x, f->y)));

        core::SplineDef def;
        def.degree   = static_cast<std::uint8_t>(std::clamp(e.degree, 1, 15));
        def.closed   = has_bit(e.flags, 1);
        def.periodic = has_bit(e.flags, 2);
        def.rational = has_bit(e.flags, 4);
        def.planar   = has_bit(e.flags, 8);
        def.linear   = has_bit(e.flags, 16);

        bool degraded = false;
        if (controls.size() < static_cast<std::size_t>(def.degree) + 1) {
            if (fits.size() < 2) {
                skip("SPLINE", "kontrol noktası dereceye yetmiyor ve uydurma noktası yok");
                return;
            }
            controls = fits;
            fits.clear();
            if (controls.size() < static_cast<std::size_t>(def.degree) + 1)
                def.degree = static_cast<std::uint8_t>(controls.size() - 1);
            ++spline_fit_only_;
            degraded = true;
        } else {
            for (const double k : e.knotslist)
                def.knots_nano.push_back(static_cast<std::int64_t>(std::llround(k * 1e9)));
            if (def.knots_nano.size() != controls.size() + def.degree + 1) def.knots_nano.clear();
            for (std::size_t i = 1; i < def.knots_nano.size(); ++i)
                if (def.knots_nano[i] < def.knots_nano[i - 1]) {
                    def.knots_nano.clear();
                    break;
                }
            if (e.weightlist.size() == controls.size()) {
                bool positive = true;
                for (const double w : e.weightlist)
                    if (!(w > 0.0)) positive = false;
                if (positive)
                    for (const double w : e.weightlist)
                        def.weights_nano.push_back(
                            static_cast<std::int64_t>(std::llround(w * 1e9)));
            }
        }
        def.has_fit = !fits.empty();

        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{controls, core::RingRole::Open, 0});
        if (def.has_fit)
            rings.push_back(core::RingGeometry::RingInput{fits, core::RingRole::Open, 0});
        auto made = tx_.add_kind(layer_for(e, in), core::kSplineKind, rings,
                                 core::encode_spline(def), in_block_);
        std::vector<double> zs;
        for (const auto& c : e.controllist)
            if (c) zs.push_back(c->z);
        finish_entity_result(e, "SPLINE", made, zs, degraded);
    }

    /// The figures of a DIMSTYLE by name, or the ISO-25 figures when the file
    /// does not define it (counted once per dimension).
    DimStyleFigures figures_of(const std::string& style)
    {
        if (const auto at = dimstyles_.find(core::turkish_fold_key(style)); at != dimstyles_.end())
            return at->second;
        ++dim_style_missing_;
        return DimStyleFigures{};
    }

    /// A DIMENSION as `core.dimension`: its definition points in the kind's
    /// order, the style's figures brought to millimetres, the measurement from
    /// the points and the text the file typed over it, if any.
    void emit_dimension(const DRW_Dimension& e, core::DimensionType type,
                        std::initializer_list<DRW_Coord> raw, double rotation_deg)
    {
        if (skipping_block_) return;
        if (!begin(e, "DIMENSION", Inherit{})) return;
        std::vector<Point2> defs;
        for (const DRW_Coord& c : raw)
            defs.push_back(to_mm(dxf::Pt{c.x, c.y}));

        // THIS PROGRAM'S OWN NOTES (the writer's reserved `olcu.` keys). An
        // arc-length dimension went out as the angular record over its centre,
        // start and end, and comes back as what it was.
        if (type == core::DimensionType::Angular && defs.size() == 5 &&
            own_note(e, "olcu.tur") == "yay") {
            type = core::DimensionType::ArcLength;
            defs = {defs[0], defs[1], defs[3], defs[4]};
        }

        const DimStyleFigures fig = figures_of(e.getStyle());
        core::DimensionDef def;
        def.type                = type;
        def.rotation_udeg       = dxf::udeg_from_degrees(rotation_deg);
        def.user_text_position  = has_bit(e.type, 128);
        def.ordinate_x          = has_bit(e.type, 64);
        def.arrow_size          = std::max<Mm>(0, to_mm_len(fig.arrow));
        def.extension_beyond    = std::max<Mm>(0, to_mm_len(fig.extension_beyond));
        def.extension_offset    = std::max<Mm>(0, to_mm_len(fig.extension_offset));
        def.text_gap            = std::max<Mm>(0, to_mm_len(fig.text_gap));
        def.precision           = static_cast<std::uint8_t>(fig.precision);
        def.decimal_separator   = fig.separator;
        def.style               = e.getStyle().empty() ? std::string("STANDARD") : e.getStyle();
        const std::string typed = e.getText();
        if (!typed.empty() && typed != "<>") def.override_text = dxf::expand_text_codes(typed);
        if (type == core::DimensionType::Ordinate) {
            def.measurement = defs.size() >= 2 ? (def.ordinate_x ? std::abs(defs[1].x - defs[0].x)
                                                                 : std::abs(defs[1].y - defs[0].y))
                                               : 0;
        } else {
            def.measurement = core::dimension_measure(type, defs, def.rotation_udeg);
        }

        // A FIGURE IN A UNIT OF ITS OWN went out whole, for other readers, with
        // its unit and the pattern it was written from beside it. The pattern
        // comes back only while group 1 is still what it gives: a caption
        // another program has since retyped is a typed caption.
        if (const auto word = own_note(e, "olcu.birim"); word) {
            if (const auto code = core::dimension_unit_code(def, *word); code) {
                def.unit = *code;
                if (const auto pattern = own_note(e, "olcu.yazi"); pattern) {
                    core::DimensionDef measured = def;
                    measured.override_text =
                        *pattern == "<>" ? std::string() : dxf::expand_text_codes(*pattern);
                    if (core::dimension_text(measured, unit_) == core::dimension_text(def, unit_))
                        def.override_text = measured.override_text;
                }
            }
        }

        // The text: centred on the file's text point, along the dimension line.
        const Point2 centre = to_mm(dxf::Pt{e.getTextPoint().x, e.getTextPoint().y});
        Mm height           = to_mm_len(fig.text_height);
        if (height <= 0) height = core::mm_from_metres(2.5);
        const std::string text = core::dimension_text(def, unit_);
        const Mm advance       = std::max<Mm>(1, core::text_width(text, height));
        std::int64_t dir       = 0;
        if (defs.size() >= 2 &&
            (type == core::DimensionType::Aligned || type == core::DimensionType::Radial ||
             type == core::DimensionType::Diametric))
            dir = core::atan2_udeg(defs[1].y - defs[0].y, defs[1].x - defs[0].x);
        else if (type == core::DimensionType::Linear)
            dir = def.rotation_udeg;
        if (dir > core::kUDegFullCircle / 4 && dir <= 3 * core::kUDegFullCircle / 4)
            dir = (dir + core::kUDegFullCircle / 2) % core::kUDegFullCircle;
        const Point2 baseline[2]{centre, dxf::point_on_circle(centre, advance, dir)};

        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{std::span<const Point2>(baseline, 2),
                                                      core::RingRole::Open, 0});
        rings.push_back(core::RingGeometry::RingInput{defs, core::RingRole::Open, 0});
        auto made = tx_.add_kind(layer_for(e, Inherit{}), core::kDimensionKind, rings,
                                 core::encode_dimension(def), in_block_);
        if (!made) {
            fail(err(made.error().code, "DIMENSION okunamadı: " + made.error().message));
            return;
        }
        if (auto st = tx_.set_text(made.value(), text, height, core::TextAnchor::MiddleCentre);
            !st) {
            fail(err(st.error().code, "DIMENSION yazısı yazılamadı: " + st.error().message));
            return;
        }
        finish_entity(e, "DIMENSION", made.value(), {}, false);
    }

    template<class E>
    void finish_entity_result(const E& e, const char* type,
                              const core::Result<command::EntityId>& made,
                              const std::vector<double>& zs, bool degraded)
    {
        if (!made) {
            fail(err(made.error().code, std::string(type) + " okunamadı: " + made.error().message));
            return;
        }
        finish_entity(e, type, made.value(), zs, degraded);
    }

    // ---- placing: every entity goes through the general mutator so a member
    // of a block definition and an entity of the drawing are made the same way.

    core::Result<command::EntityId> place_polyline(LayerId layer, std::span<const Point2> pts)
    {
        const core::RingGeometry::RingInput ring{pts, core::RingRole::Open, 0};
        return tx_.add_kind(layer, core::kPolylineKind,
                            std::span<const core::RingGeometry::RingInput>(&ring, 1), {},
                            in_block_);
    }

    core::Result<command::EntityId> place_area(LayerId layer,
                                               std::span<const core::RingGeometry::RingInput> rings)
    {
        return tx_.add_kind(layer, core::kPolylineKind, rings, {}, in_block_);
    }

    core::Result<command::EntityId> place_point(LayerId layer, Point2 at)
    {
        const Point2 pts[1]{at};
        const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 1),
                                                 core::RingRole::Open, 0};
        return tx_.add_kind(layer, core::kPointKind,
                            std::span<const core::RingGeometry::RingInput>(&ring, 1), {},
                            in_block_);
    }

    core::Result<command::EntityId> place_circle(LayerId layer, Point2 centre, Mm radius)
    {
        const Point2 pts[2]{centre, Point2{centre.x + radius, centre.y}};
        const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 2),
                                                 core::RingRole::Open, 0};
        return tx_.add_kind(layer, core::kCircleKind,
                            std::span<const core::RingGeometry::RingInput>(&ring, 1), {},
                            in_block_);
    }

    core::Result<command::EntityId> place_arc(LayerId layer, Point2 centre, Mm radius, Point2 from,
                                              Point2 to)
    {
        const Point2 pts[4]{centre, Point2{centre.x + radius, centre.y}, from, to};
        const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 4),
                                                 core::RingRole::Open, 0};
        return tx_.add_kind(layer, core::kArcKind,
                            std::span<const core::RingGeometry::RingInput>(&ring, 1), {},
                            in_block_);
    }

    core::Result<command::EntityId> place_ellipse(LayerId layer, Point2 centre, Point2 major,
                                                  Point2 minor,
                                                  std::span<const std::uint8_t> payload)
    {
        const Point2 pts[3]{centre, major, minor};
        const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 3),
                                                 core::RingRole::Open, 0};
        return tx_.add_kind(layer, core::kEllipseKind,
                            std::span<const core::RingGeometry::RingInput>(&ring, 1), payload,
                            in_block_);
    }

    /// An INSERT as `core.block_reference`, placed on the drawing or — when
    /// `in_block` names a definition — as a member of it.
    void place_insert(const DRW_Insert& e, core::BlockId in_block)
    {
        const auto at = blocks_.find(core::turkish_fold_key(e.name));
        if (at == blocks_.end()) {
            skip("INSERT", ("'" + e.name + "' bloğu dosyada tanımlı değil").c_str());
            return;
        }
        const Ocs ocs = Ocs::from_normal(e.extPoint);
        double ix = e.basePoint.x, iy = e.basePoint.y, iz = e.basePoint.z;
        ocs.to_wcs(ix, iy, iz);

        core::BlockReference ref;
        ref.block         = at->second;
        ref.sx            = ratio_of(e.xscale);
        ref.sy            = ratio_of(e.yscale);
        ref.rotation_udeg = dxf::udeg_from_radians(e.angle);
        if (ocs.mirror_only()) {
            // A mirror in x after the placement is a mirrored x scale and the
            // opposite turn: M·R(ρ)·S = R(−ρ)·S(−sx, sy).
            ref.sx.num        = -ref.sx.num;
            ref.rotation_udeg = -ref.rotation_udeg;
        }
        ref.rotation_udeg %= core::kUDegFullCircle;
        if (ref.rotation_udeg < 0) ref.rotation_udeg += core::kUDegFullCircle;
        ref.columns        = static_cast<std::uint16_t>(std::clamp(e.colcount, 1, 65535));
        ref.rows           = static_cast<std::uint16_t>(std::clamp(e.rowcount, 1, 65535));
        ref.column_spacing = to_mm_len(e.colspace);
        ref.row_spacing    = to_mm_len(e.rowspace);
        if (ref.sx.num == 0 || ref.sy.num == 0) {
            skip("INSERT", "sıfır ölçekli blok referansı");
            return;
        }
        const Point2 insertion = to_mm(dxf::Pt{ix, iy});
        ref.bounds             = core::block_reference_bounds(tx_.document(), insertion, ref);

        const Point2 pts[1]{insertion};
        const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 1),
                                                 core::RingRole::Open, 0};
        const core::BlockId saved = in_block_;
        in_block_                 = in_block;
        auto made                 = tx_.add_kind(layer_slot(e.layer), core::kBlockReferenceKind,
                                                 std::span<const core::RingGeometry::RingInput>(&ring, 1),
                                                 core::encode_block_reference(ref), in_block);
        if (made && in_block == core::kNoBlock) ++refs_read_;
        finish(e, "INSERT", made, {iz});
        in_block_ = saved;
    }

    void flush_pending_inserts()
    {
        std::vector<PendingInsert> pending = std::move(pending_inserts_);
        pending_inserts_.clear();
        for (const PendingInsert& p : pending) {
            if (failed_ || cancelled_) break;
            place_insert(*p.data, p.in_block);
        }
    }

    static core::Ratio ratio_of(double v)
    {
        const auto num       = static_cast<std::int64_t>(std::llround(v * 1000000.0));
        std::int64_t den     = 1000000;
        const std::int64_t g = std::gcd(num < 0 ? -num : num, den);
        return g > 1 ? core::Ratio{num / g, den / g} : core::Ratio{num, den};
    }

    template<class E>
    void emit_quad(const E& e, const Xform& x, const Inherit& in, const char* type)
    {
        // A SOLID's corners come 1-2-4-3 (the bow-tie order of the format).
        const dxf::Pt raw[4]{{e.basePoint.x, e.basePoint.y},
                             {e.secPoint.x, e.secPoint.y},
                             {e.fourPoint.x, e.fourPoint.y},
                             {e.thirdPoint.x, e.thirdPoint.y}};
        std::vector<Point2> pts;
        for (const dxf::Pt& p : raw) {
            const Point2 m = to_mm(x.apply(p.x, p.y));
            if (pts.empty() || pts.back() != m) pts.push_back(m);
        }
        if (pts.size() > 1 && pts.front() == pts.back()) pts.pop_back();
        if (pts.size() < 3) {
            skip(type, "üçten az ayrı köşe");
            return;
        }
        const core::RingGeometry::RingInput ring{pts, core::RingRole::Exterior, 0};
        auto made =
            place_area(layer_for(e, in), std::span<const core::RingGeometry::RingInput>(&ring, 1));
        finish(e, type, made, {e.basePoint.z});
    }

    template<class E>
    void emit_text(const E& e, const Xform& x, const Inherit& in, const DRW_Coord& at,
                   std::string words, double height_units, double angle_deg,
                   core::TextAnchor anchor, const char* type, core::TextLines lines, Mm width)
    {
        if (words.empty()) {
            skip(type, "boş yazı");
            return;
        }
        const Ocs ocs = Ocs::from_normal(e.extPoint);
        double px = at.x, py = at.y, pz = at.z;
        ocs.to_wcs(px, py, pz);
        const Point2 where = to_mm(x.apply(px, py));
        Mm height          = to_mm_len(height_units * x.uniform_scale());
        if (height <= 0) height = core::mm_from_metres(1.0);
        std::int64_t angle = dxf::udeg_from_degrees(angle_deg) + x.rotation_udeg();
        if (ocs.mirror_only()) angle = core::kUDegFullCircle / 2 - angle;

        // The baseline: from the anchor along the text direction, as long as the
        // caption is wide — the same rule `METİN` uses — or, for a text that
        // wraps, as long as the width its lines break to.
        const Mm advance =
            std::max<Mm>(1, lines.wrap && width > 0 ? width : core::text_width(words, height));
        const Point2 end = dxf::point_on_circle(where, advance, angle);
        const Point2 baseline[2]{where, end};
        auto made = place_polyline(layer_for(e, in), std::span<const Point2>(baseline, 2));
        if (!made) {
            fail(err(made.error().code, std::string(type) + " okunamadı: " + made.error().message));
            return;
        }
        if (auto st = tx_.set_text(made.value(), std::move(words), height, anchor, lines); !st) {
            fail(err(st.error().code,
                     std::string(type) + " yazısı yazılamadı: " + st.error().message));
            return;
        }
        finish_entity(e, type, made.value(), {pz}, false);
    }

    void emit_hatch(const DRW_Hatch& e, const Xform& x, const Inherit& in, std::size_t ordinal)
    {
        struct Loop
        {
            std::vector<Point2> pts;
            double area2{0.0}; // twice the signed area, for outer/inner ordering
        };

        std::vector<Loop> loops;
        bool stroked = false;
        for (const auto& lp : e.looplist) {
            if (!lp) continue;
            Loop loop;
            if (has_bit(lp->type, 2)) {
                // A polyline loop: one LWPOLYLINE with optional bulges.
                for (const auto& obj : lp->objlist) {
                    const auto* pl = dynamic_cast<const DRW_LWPolyline*>(obj.get());
                    if (pl == nullptr) continue;
                    std::vector<dxf::Pt> verts;
                    std::vector<double> bulges;
                    for (const auto& v : pl->vertlist)
                        if (v) {
                            verts.push_back(dxf::Pt{v->x, v->y});
                            bulges.push_back(v->bulge);
                        }
                    ring_points(x, verts, bulges, true, loop.pts, stroked);
                }
            } else {
                // Edge loop: lines, arcs, elliptic arcs and splines, in order.
                for (const auto& obj : lp->objlist) {
                    if (!obj) continue;
                    if (const auto* ln = dynamic_cast<const DRW_Line*>(obj.get())) {
                        push_unique(loop.pts, to_mm(x.apply(ln->basePoint.x, ln->basePoint.y)));
                        push_unique(loop.pts, to_mm(x.apply(ln->secPoint.x, ln->secPoint.y)));
                    } else if (const auto* ar = dynamic_cast<const DRW_Arc*>(obj.get())) {
                        const Point2 c  = to_mm(x.apply(ar->basePoint.x, ar->basePoint.y));
                        const Mm r      = to_mm_len(ar->radious * x.uniform_scale());
                        std::int64_t s0 = dxf::udeg_from_radians(ar->staangle) + x.rotation_udeg();
                        std::int64_t s1 = dxf::udeg_from_radians(ar->endangle) + x.rotation_udeg();
                        if (ar->isccw == 0) std::swap(s0, s1);
                        std::vector<Mm> xs, ys;
                        core::arc_outline(c, r, dxf::point_on_circle(c, r, s0),
                                          dxf::point_on_circle(c, r, s1), xs, ys);
                        if (ar->isccw == 0) {
                            std::reverse(xs.begin(), xs.end());
                            std::reverse(ys.begin(), ys.end());
                        }
                        for (std::size_t k = 0; k < xs.size(); ++k)
                            push_unique(loop.pts, Point2{xs[k], ys[k]});
                        stroked = true;
                    } else if (const auto* el = dynamic_cast<const DRW_Ellipse*>(obj.get())) {
                        const Point2 c  = to_mm(x.apply(el->basePoint.x, el->basePoint.y));
                        const Point2 ma = to_mm(x.apply(el->basePoint.x + el->secPoint.x,
                                                        el->basePoint.y + el->secPoint.y));
                        const Point2 mi =
                            to_mm(x.apply(el->basePoint.x - el->secPoint.y * el->ratio,
                                          el->basePoint.y + el->secPoint.x * el->ratio));
                        std::vector<Point2> arcpts;
                        std::int64_t s0 = dxf::udeg_from_radians(el->staparam),
                                     s1 = dxf::udeg_from_radians(el->endparam);
                        if (el->isccw == 0) std::swap(s0, s1);
                        dxf::ellipse_arc_points(c, Point2{ma.x - c.x, ma.y - c.y},
                                                Point2{mi.x - c.x, mi.y - c.y}, s0, s1, arcpts);
                        if (el->isccw == 0) std::reverse(arcpts.begin(), arcpts.end());
                        for (const Point2& p : arcpts)
                            push_unique(loop.pts, p);
                        stroked = true;
                    } else if (const auto* sp = dynamic_cast<const DRW_Spline*>(obj.get())) {
                        std::vector<dxf::Pt> controls;
                        for (const auto& cp : sp->controllist)
                            if (cp) controls.push_back(dxf::Pt{cp->x, cp->y});
                        std::vector<dxf::Pt> pts;
                        std::vector<double> weights = sp->weightlist;
                        if (!weights.empty() && weights.size() != controls.size()) weights.clear();
                        if (dxf::nurbs_points(sp->degree, sp->knotslist, controls, weights, 16,
                                              pts))
                            for (const dxf::Pt& p : pts)
                                push_unique(loop.pts, to_mm(x.apply(p.x, p.y)));
                        stroked = true;
                    }
                }
            }
            if (loop.pts.size() > 1 && loop.pts.front() == loop.pts.back()) loop.pts.pop_back();
            if (loop.pts.size() < 3) continue;
            // Twice the signed area, translated to the first vertex (core.md R3).
            double a2 = 0.0;
            for (std::size_t i = 0; i < loop.pts.size(); ++i) {
                const Point2& p = loop.pts[i];
                const Point2& q = loop.pts[(i + 1) % loop.pts.size()];
                a2 += static_cast<double>(p.x - loop.pts[0].x) *
                          static_cast<double>(q.y - loop.pts[0].y) -
                      static_cast<double>(q.x - loop.pts[0].x) *
                          static_cast<double>(p.y - loop.pts[0].y);
            }
            loop.area2 = a2;
            loops.push_back(std::move(loop));
        }
        if (loops.empty()) {
            skip("HATCH", "sınır döngüsü çözülemedi");
            return;
        }
        // The largest loop is the face; every other loop is an island in it.
        std::sort(loops.begin(), loops.end(), [](const Loop& a, const Loop& b) {
            return std::abs(a.area2) > std::abs(b.area2);
        });
        std::vector<core::RingGeometry::RingInput> rings;
        rings.reserve(loops.size());
        for (std::size_t i = 0; i < loops.size(); ++i)
            rings.push_back(core::RingGeometry::RingInput{
                loops[i].pts, i == 0 ? core::RingRole::Exterior : core::RingRole::Interior, 0});

        // THE HATCH AS A HATCH (core/hatch.hpp): the loops are its rings, the
        // pattern its payload — name, angle and the scale in this file's unit
        // (dxf_common.hpp), the line families from the pattern catalogue by
        // name; the file's own definition lines, when it has them, replace
        // those once the whole file has been read (`read_pattern_lines`).
        core::HatchDef def;
        def.solid        = e.solid != 0;
        def.double_lines = e.doubleflag != 0;
        def.associative  = e.associative != 0;
        def.style        = static_cast<std::uint16_t>(std::clamp(e.hstyle, 0, 2));
        def.pattern_type = static_cast<std::uint16_t>(std::clamp(e.hpattern, 0, 2));
        def.name         = e.name.empty() && def.solid ? std::string("SOLID") : e.name;
        if (def.name.empty() && def.pattern_type != 0) def.name = "ANSI31";
        def.angle_udeg = dxf::udeg_from_degrees(e.angle);
        def.origin     = to_mm(dxf::Pt{e.basePoint.x, e.basePoint.y});
        bool unknown   = false;
        if (!def.solid) {
            dxf::apply_dxf_pattern_scale(def, e.scale, unit_);
            if (def.pattern_type != 0) {
                if (const command::HatchPattern* p = patterns().find(def.name); p != nullptr)
                    def.families = p->families;
                else
                    unknown = true;
            }
        }

        auto made = tx_.add_kind(layer_for(e, in), core::kHatchKind, rings, core::encode_hatch(def),
                                 in_block_);
        if (!made) {
            fail(err(made.error().code, std::string("HATCH okunamadı: ") + made.error().message));
            return;
        }
        const command::EntityId id = made.value();

        // Drawn through its style (model.md R14): the ink is the entity's colour
        // resolved here, the pattern's families become its fill layers.
        int color   = e.color;
        int color24 = e.color24;
        if (color == DRW::ColorByBlock) color = DRW::ColorByLayer;
        std::uint32_t rgb = 0x000000;
        if (color24 >= 0)
            rgb = static_cast<std::uint32_t>(color24);
        else if (color != DRW::ColorByLayer)
            rgb = dxf::aci_ink(std::abs(color));
        else if (const auto at = layers_.find(core::turkish_fold_key(e.layer)); at != layers_.end())
            rgb = at->second.color24 >= 0 ? static_cast<std::uint32_t>(at->second.color24)
                                          : dxf::aci_ink(at->second.color);
        const core::StyleId style =
            tx_.intern_symbol(command::hatch_symbol(def, 0xFF000000u | rgb));
        if (auto st = tx_.set_entity_style(id, style); !st) fail(st.error());
        finish_entity(e, "HATCH", id, {e.basePoint.z}, stroked);
        if (!def.solid && e.deflines > 0)
            patterned_.push_back(Patterned{id, static_cast<std::uint32_t>(e.handle), ordinal,
                                           std::move(def), 0xFF000000u | rgb, unknown});
        else if (unknown)
            ++hatch_unknown_;
    }

public:
    /// The definition lines of the hatches that announced some, read from the
    /// file itself once libdxfrw is done with it: each patterned hatch then
    /// draws with the lines the file draws it with — spacing, angle and origin
    /// — and a pattern the catalogue does not know draws at all.
    /// THE MULTILEADERS, which libdxfrw has no class for (TODOS C-12): read by
    /// GDAL's DXF driver, and only when the file holds one — a byte search
    /// first, because a second pass over a large drawing costs time the import
    /// budget does not have. Each becomes this program's own leader, one per
    /// leader line, and its words a caption tied to the landing when they
    /// stand where that tie puts them.
    core::Status read_multileaders(const std::string& path, bool utf8)
    {
        bool present = false;
        if (auto mapped = MappedFile::open(path); mapped) {
            const auto bytes = mapped.value().bytes();
            present = std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())
                          .find("MULTILEADER") != std::string_view::npos;
        }
        if (!present) return core::ok();
        if (!dxf_multileaders_supported()) {
            note(Severity::Skipped,
                 "Dosyada MULTILEADER var; bu yapı onları okuyamıyor (GDAL'sız derlenmiş), "
                 "atlandılar.");
            return core::ok();
        }
        auto read = read_dxf_multileaders(path, utf8);
        if (!read) {
            note(Severity::Warning, "MULTILEADER'lar okunamadı: " + read.error().message);
            return core::ok();
        }

        std::size_t made  = 0;
        std::size_t tied  = 0;
        std::size_t loose = 0;
        const auto close  = [](Point2 a, Point2 b) {
            return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1;
        };
        for (const DxfMultiLeader& ml : read.value()) {
            const LayerId layer = layer_slot(ml.layer.empty() ? std::string("0") : ml.layer);
            if (layer == core::kNoLayer) continue;
            std::vector<std::vector<Point2>> runs;
            for (const std::vector<DxfXY>& line : ml.lines) {
                std::vector<Point2> run;
                for (const DxfXY& p : line)
                    push_unique(run, to_mm(dxf::Pt{p.x, p.y}));
                if (run.size() >= 2) runs.push_back(std::move(run));
            }
            // THE LANDING IS ITS OWN RUN, starting where the leader lines end;
            // each leader line is drawn on into it.
            std::vector<bool> landing(runs.size(), false);
            for (std::size_t i = 0; i < runs.size(); ++i)
                for (std::size_t j = 0; j < runs.size(); ++j)
                    if (i != j && runs[i].size() == 2 && close(runs[i].front(), runs[j].back()))
                        landing[i] = true;
            command::EntityId first = core::kNoEntity;
            std::vector<Point2> first_points;
            for (std::size_t i = 0; i < runs.size(); ++i) {
                if (landing[i]) continue;
                std::vector<Point2> pts = runs[i];
                for (std::size_t j = 0; j < runs.size(); ++j)
                    if (landing[j] && close(runs[j].front(), pts.back())) {
                        push_unique(pts, runs[j].back());
                        break;
                    }
                // THE ARROW'S TIP: GDAL starts the line at the head's base and
                // draws the head as its own outline, whose farthest corner from
                // that base is the point the leader shows.
                core::LeaderDef def;
                for (const std::vector<DxfXY>& head : ml.arrows) {
                    std::vector<Point2> corners;
                    corners.reserve(head.size());
                    for (const DxfXY& p : head)
                        corners.push_back(to_mm(dxf::Pt{p.x, p.y}));
                    if (corners.empty()) continue;
                    const Point2 base = pts.front();
                    const auto far    = std::ranges::max_element(corners, {}, [base](Point2 c) {
                        return std::hypot(static_cast<double>(c.x - base.x),
                                             static_cast<double>(c.y - base.y));
                    });
                    const double size = std::hypot(static_cast<double>(far->x - base.x),
                                                   static_cast<double>(far->y - base.y));
                    // Its own head: the base sits between the other corners.
                    if (size <= 0.0 || size > 1e7) continue;
                    bool at_base = false;
                    for (const Point2 c : corners)
                        at_base = at_base || std::hypot(static_cast<double>(c.x - base.x),
                                                        static_cast<double>(c.y - base.y)) < size;
                    if (!at_base) continue;
                    // The base lies on the line from the tip on, so the tip takes
                    // its place: the leader starts where the file's did.
                    pts.front()    = *far;
                    def.arrow      = true;
                    def.arrow_size = core::mm_round(size);
                    break;
                }
                const core::RingGeometry::RingInput ring{pts, core::RingRole::Open, 0};
                auto leader = tx_.add_kind(layer, core::kLeaderKind,
                                           std::span<const core::RingGeometry::RingInput>(&ring, 1),
                                           core::encode_leader(def), core::kNoBlock);
                if (!leader) return leader.error();
                ++made;
                if (first == core::kNoEntity) {
                    first        = leader.value();
                    first_points = pts;
                }
            }
            if (ml.text.empty()) continue;

            // THE WORDS, where the file put them, anchored as it anchored them.
            const Mm height =
                ml.text_height > 0.0 ? to_mm_len(ml.text_height) : core::mm_from_metres(2.5);
            const int column = (ml.text_anchor - 1) % 3; // left, centre, right
            int row          = 0;                        // bottom
            if (ml.text_anchor >= 4) row = 1;
            if (ml.text_anchor >= 7) row = 2;
            core::TextAnchor anchor = core::text_anchor_at(column, row);
            Point2 at               = to_mm(dxf::Pt{ml.text_at.x, ml.text_at.y});

            // TIED TO THE LANDING when they stand exactly where the tie puts
            // them: one line, not turned, its middle on the landing and its
            // near edge off the side the last segment points.
            std::optional<core::Attachment> tie;
            if (first != core::kNoEntity && first_points.size() >= 2 &&
                ml.text.find('\n') == std::string::npos && std::abs(ml.text_angle) < 1e-9 &&
                row >= 1) {
                const Point2 end   = first_points.back();
                const Point2 from  = first_points[first_points.size() - 2];
                const bool right   = end.x >= from.x;
                const Point2 level = row == 2 ? Point2{at.x, at.y - (height / 2)} : at;
                const Mm gap       = right ? level.x - end.x : end.x - level.x;
                if (std::abs(level.y - end.y) <= 1 && gap >= 0 && column == (right ? 0 : 2)) {
                    core::Attachment a;
                    a.source = tx_.document().key_of(first);
                    a.anchor = core::AttachAnchor::Landing;
                    a.derive = core::AttachDerive::Keep;
                    a.gap    = gap;
                    if (const auto place =
                            core::attach_place(first_points, false, a, height, false);
                        place && place->anchor) {
                        at     = place->centre;
                        anchor = *place->anchor;
                        tie    = a;
                    }
                }
            }
            const std::int64_t turn = dxf::udeg_from_degrees(ml.text_angle);
            const Mm advance        = std::max<Mm>(1, core::text_width(ml.text, height));
            const Point2 baseline[2]{at, dxf::point_on_circle(at, advance, turn)};
            auto caption = place_polyline(layer, baseline);
            if (!caption) return caption.error();
            if (auto st = tx_.set_text(caption.value(), ml.text, height, anchor); !st) return st;
            if (tie) {
                if (auto st = tx_.set_attachment(caption.value(), *tie); !st) return st;
                ++tied;
            } else {
                ++loose;
            }
        }
        if (made != 0)
            note(Severity::Info,
                 std::to_string(read.value().size()) + " MULTILEADER " + std::to_string(made) +
                     " kılavuz çizgi olarak okundu (GDAL ile)" +
                     (tied != 0 ? "; " + std::to_string(tied) + " yazı kılavuzun ucuna bağlandı"
                                : std::string()) +
                     (loose != 0 ? "; " + std::to_string(loose) +
                                       " yazı bağlanmadı — çok satırlı, dönük ya da ucundan "
                                       "ayrı; yerinde duruyor"
                                 : std::string()) +
                     ".");
        return core::ok();
    }

    core::Status read_pattern_lines(const std::string& path)
    {
        if (patterned_.empty()) return core::ok();
        std::vector<HatchRecordLines> records;
        if (auto mapped = MappedFile::open(path); mapped) {
            const auto bytes = mapped.value().bytes();
            records          = scan_hatch_lines(
                std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        }
        std::map<std::uint32_t, std::size_t> by_handle;
        for (std::size_t i = 0; i < records.size(); ++i)
            if (records[i].handle != 0) by_handle.emplace(records[i].handle, i);
        // By order only when the file and the library agree on how many there
        // are: a record the library dropped would shift every one after it.
        const bool by_order = records.size() == hatches_seen_;

        std::uint64_t from_file = 0;
        for (Patterned& p : patterned_) {
            const HatchRecordLines* record = nullptr;
            if (const auto at = by_handle.find(p.handle); p.handle != 0 && at != by_handle.end())
                record = &records[at->second];
            else if (by_order && p.ordinal < records.size())
                record = &records[p.ordinal];
            const std::vector<core::HatchDef::Family> known = p.def.families;
            if (record == nullptr ||
                !dxf::families_from_lines(p.def, record->lines, unit_, known)) {
                if (p.unknown) ++hatch_unknown_;
                continue;
            }
            ++from_file;
            if (auto st = tx_.set_kind_payload(p.id, core::encode_hatch(p.def)); !st) return st;
            const core::StyleId style = tx_.intern_symbol(command::hatch_symbol(p.def, p.ink));
            if (auto st = tx_.set_entity_style(p.id, style); !st) return st;
        }
        if (from_file != 0)
            note(Severity::Info, std::to_string(from_file) +
                                     " taramanın deseni dosyadaki kendi çizgileriyle okundu "
                                     "(aralık, açı ve başlangıç dosyadaki gibi).");
        return core::ok();
    }

private:
    /// The pattern catalogue, loaded on first use from its default place.
    const command::HatchPatternCatalog& patterns()
    {
        if (!patterns_loaded_) {
            patterns_loaded_ = true;
            const std::string path =
                command::resolve_catalog_path(command::kDefaultHatchPatternPath);
            if (!path.empty())
                if (auto loaded = command::load_hatch_patterns(path); loaded)
                    patterns_ = std::move(loaded.value());
        }
        return patterns_;
    }

    static void push_unique(std::vector<Point2>& pts, Point2 p)
    {
        if (pts.empty() || pts.back() != p) pts.push_back(p);
    }

    void ring_points(const Xform& x, const std::vector<dxf::Pt>& verts,
                     const std::vector<double>& bulges, bool closed, std::vector<Point2>& out,
                     bool& stroked)
    {
        const std::size_t n = verts.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Point2 a = to_mm(x.apply(verts[i].x, verts[i].y));
            push_unique(out, a);
            if (!closed && i + 1 == n) break;
            const double bulge = i < bulges.size() ? bulges[i] : 0.0;
            if (bulge == 0.0) continue;
            const Point2 b = to_mm(x.apply(verts[(i + 1) % n].x, verts[(i + 1) % n].y));
            Point2 centre{};
            Mm radius = 0;
            bool ccw  = false;
            if (!dxf::arc_from_bulge(a, b, x.mirrored() ? -bulge : bulge, centre, radius, ccw))
                continue;
            stroked = true;
            std::vector<Mm> xs, ys;
            core::arc_outline(centre, radius, ccw ? a : b, ccw ? b : a, xs, ys);
            if (!ccw) {
                std::reverse(xs.begin(), xs.end());
                std::reverse(ys.begin(), ys.end());
            }
            for (std::size_t k = 1; k + 1 < xs.size(); ++k)
                push_unique(out, Point2{xs[k], ys[k]});
        }
    }

    command::Transaction& tx_;
    const ImportOptions& options_;
    std::stop_token stop_;
    core::DrawingUnit unit_;
    DxfReport report_;

    std::map<std::string, LayerInfo> layers_;
    std::vector<LayerId> lock_later_;
    std::map<std::string, core::BlockId> blocks_; ///< folded name → definition
    std::map<std::string, DimStyleFigures> dimstyles_;
    std::vector<PendingInsert> pending_inserts_;
    core::BlockId in_block_{core::kNoBlock}; ///< the definition being read, or none
    bool skipping_block_{false};             ///< inside an anonymous block
    command::HatchPatternCatalog patterns_;
    bool patterns_loaded_{false};

    /// A hatch whose file record announced definition lines, until they are read.
    struct Patterned
    {
        command::EntityId id;
        std::uint32_t handle{0}; ///< group 5, 0 when the file gives none
        std::size_t ordinal{0};  ///< which HATCH of the file it is
        core::HatchDef def;      ///< as far as the library read it
        std::uint32_t ink{0};    ///< the colour its symbol draws in
        bool unknown{false};     ///< the catalogue does not know its name
    };

    std::vector<Patterned> patterned_;
    std::size_t hatches_seen_{0};

    std::uint64_t seen_{0}, skipped_{0}, paper_space_{0}, ltypes_{0};
    std::vector<std::string> xrefs_; ///< blocks the file marks as external references
    std::uint64_t blocks_read_{0}, refs_read_{0}, spline_fit_only_{0}, hatch_unknown_{0},
        dim_style_missing_{0}, widths_dropped_{0}, z_varies_{0}, kot_written_{0},
        text_align_approx_{0}, byblock_seen_{0}, tilted_{0}, xdata_kept_{0}, linetypes_dropped_{0};
    /// Which face each text style asks for, by folded style name (`addTextStyle`).
    std::map<std::string, std::string> style_faces_;
    /// How many captions asked for each face this program does not draw with.
    std::map<std::string, std::size_t> foreign_faces_;
    std::string first_skip_reason_;
    bool codepage_seen_{false};
    bool cancelled_{false};
    bool failed_{false};
    core::Error error_{};
};

} // namespace

command::Task<core::Result<DxfReport>> import_dxf(command::Transaction& tx, std::string path,
                                                  ImportOptions options, std::stop_token stop)
{
    // io.md P14: no network path, no archive path. Refused before the library
    // ever sees the string — the same rule the GDAL path applies.
    if (path.rfind("/vsi", 0) == 0)
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' sanal dosya sistemi yolu. KentOSCad bir veri dosyasının ağdan ya da "
                          "arşivin içinden okunmasına izin vermez; dosyayı diske alıp yeniden "
                          "deneyin.");
    if (stop.stop_requested())
        co_return err(ErrorCode::Cancelled, "İçe aktarma durduruldu; çizim değişmedi.");

    DxfSink sink(tx, options, stop);

    // The coordinate system: the `.prj` beside the file, or the drawing's own,
    // said out loud (io.md R20) — the same words the GDAL path uses.
    {
        auto prj = prj_sidecar_crs(path);
        if (prj) {
            sink.set_crs(prj.value());
            // The `.prj` names a system in metres (`prj_sidecar_crs` refuses any
            // other) and the numbers are read in the PROJECT'S unit. When the
            // two disagree, a GIS program trusting the `.prj` reads these same
            // numbers a thousand times larger — said once, out loud, as the
            // GDAL path says it (vector.cpp).
            if (options.drawing_unit != core::DrawingUnit::Metre)
                sink.report().diagnostics.note(
                    Severity::Warning,
                    "Yanındaki .prj metre sayan bir sistem bildiriyor (" + prj.value() +
                        ") ama çizim " + core::drawing_unit_name(options.drawing_unit) +
                        " olarak okundu (AYAR çizim_birimi). Bir CBS programı bu dosyanın "
                        "sayılarını metre okur; dosya metre ise AYAR çizim_birimi metre ile "
                        "yeniden aktarın.");
        } else if (prj.error().code == ErrorCode::NotFound ||
                   prj.error().code == ErrorCode::Unsupported) {
            if (options.project_crs.empty())
                co_return err(ErrorCode::ValidationFailed,
                              "'" + path +
                                  "' koordinat sistemi bildirmiyor (DXF taşıyamaz) ve çizimin "
                                  "kendi sistemi de yok. Yanına aynı adlı bir .prj dosyası koyun "
                                  "ya da AYAR koordinat_sistemi ile çizimin sistemini kurun.");
            sink.set_crs(options.project_crs);
            // See the same note in `vector.cpp`: an assumed coordinate system is
            // a `Warning` by `Severity`'s own definition, and io.md R20's
            // "never a silent assumption" is about being HEARD, not only about
            // being recorded.
            sink.report().diagnostics.note(
                Severity::Warning,
                "Dosya koordinat sistemi bildirmiyor (DXF taşıyamaz). Çizimin kendi "
                "sistemi varsayıldı: " +
                    options.project_crs +
                    ". Yanlışsa GERİAL ile geri alın, AYAR koordinat_sistemi ile "
                    "doğrusunu kurun ve yeniden aktarın.");
        } else {
            co_return prj.error();
        }
    }

    dxfRW reader(path.c_str());
    const bool ok = reader.read(&sink, /*ext=*/true);
    if (sink.cancelled())
        co_return err(ErrorCode::Cancelled, "İçe aktarma durduruldu; çizim değişmedi.");
    if (const core::Error* failed = sink.failure(); failed != nullptr) co_return *failed;
    if (!ok)
        co_return err(ErrorCode::ParseError,
                      "'" + path + "' DXF olarak okunamadı (libdxfrw hata kodu " +
                          std::to_string(static_cast<int>(reader.getError())) +
                          "). Dosya bozuk ya da bu bir DXF değil.");

    if (const auto st = sink.read_pattern_lines(path); !st) co_return st.error();
    if (const auto st = sink.read_multileaders(path, reader.getVersion() >= DRW::AC1021); !st)
        co_return st.error();
    if (auto st = sink.conclude(); !st) co_return st.error();
    sink.report().version = dxf::acad_name(reader.getVersion());
    co_return std::move(sink.report());
}

#endif // KENTOS_HAVE_DXFRW

} // namespace kentos::io
