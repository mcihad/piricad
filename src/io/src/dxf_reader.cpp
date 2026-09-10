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
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/spline.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/io/vector.hpp"

#include "dxf_common.hpp"
#include "dxf_units.hpp"
#include "prj_sidecar.hpp"

#include <algorithm>
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
    DRW_Insert data;
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
        info.frozen                                = (data.flags & 1) != 0;
        info.locked                                = (data.flags & 4) != 0;
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

    void addTextStyle(const DRW_Textstyle&) override {}

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
                          emit_polyline(e, x, in, verts, bulges, (e.flags & 1) != 0, "LWPOLYLINE",
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
            if ((e.flags & (16 | 64)) != 0) {
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
            emit_polyline(e, x, in, verts, bulges, (e.flags & 1) != 0, "POLYLINE", zs, varying);
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
                pending_inserts_.push_back(PendingInsert{e, in_block_});
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
            // MTEXT attachment point (code 71): 1 TL 2 TC 3 TR 4 ML 5 MC 6 MR 7 BL 8 BC 9 BR.
            core::TextAnchor anchor = core::TextAnchor::BaselineLeft;
            const int attach        = e.textgen;
            if (attach == 5)
                anchor = core::TextAnchor::MiddleCentre;
            else if (attach == 2 || attach == 8)
                anchor = core::TextAnchor::BaselineCentre;
            else if (attach == 3 || attach == 6 || attach == 9)
                anchor = core::TextAnchor::BaselineRight;
            if (attach != 1 && attach != 5 && attach != 7 && attach != 2 && attach != 8 &&
                attach != 3 && attach != 9 && attach != 4 && attach != 6)
                ++text_align_approx_;
            if (attach == 1 || attach == 2 || attach == 3 || attach == 4 || attach == 6)
                ++text_align_approx_; // top and middle rows have no anchor of their own yet
            // Code 11 of an MTEXT is its X-axis direction; when the file gave one
            // it stands in for the angle.
            double angle_deg = e.angle;
            if (e.secPoint.x != 0.0 || e.secPoint.y != 0.0)
                angle_deg = std::atan2(e.secPoint.y, e.secPoint.x) * 180.0 / core::kPi;
            emit_text(e, x, in, e.basePoint, dxf::strip_mtext(e.text), e.height, angle_deg, anchor,
                      "MTEXT");
        });
    }

    void addText(const DRW_Text& data) override
    {
        defer_or_emit(data, [this](const DRW_Text& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "TEXT", in)) return;
            core::TextAnchor anchor = core::TextAnchor::BaselineLeft;
            const bool aligned_point =
                e.alignH != DRW_Text::HLeft || e.alignV != DRW_Text::VBaseLine;
            if (e.alignV == DRW_Text::VMiddle &&
                (e.alignH == DRW_Text::HCenter || e.alignH == DRW_Text::HMiddle))
                anchor = core::TextAnchor::MiddleCentre;
            else if (e.alignH == DRW_Text::HCenter || e.alignH == DRW_Text::HMiddle)
                anchor = core::TextAnchor::BaselineCentre;
            else if (e.alignH == DRW_Text::HRight)
                anchor = core::TextAnchor::BaselineRight;
            if (e.alignV == DRW_Text::VTop || e.alignV == DRW_Text::VBottom ||
                (e.alignV == DRW_Text::VMiddle && anchor != core::TextAnchor::MiddleCentre) ||
                e.alignH == DRW_Text::HAligned || e.alignH == DRW_Text::HFit)
                ++text_align_approx_;
            emit_text(e, x, in, aligned_point ? e.secPoint : e.basePoint,
                      dxf::expand_text_codes(e.text), e.height, e.angle, anchor, "TEXT");
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
        defer_or_emit(*data, [this](const DRW_Hatch& e, const Xform& x, const Inherit& in, int) {
            if (!begin(e, "HATCH", in)) return;
            emit_hatch(e, x, in);
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
            if (cid == "anahtar") continue; // the source's key is its history, not a cell

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

    Point2 to_mm(const dxf::Pt& p) const noexcept
    {
        return Point2{core::mm_from_drawing_units(p.x, unit_),
                      core::mm_from_drawing_units(p.y, unit_)};
    }

    Mm to_mm_len(double v) const noexcept { return core::mm_from_drawing_units(v, unit_); }

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
        def.closed   = (e.flags & 1) != 0;
        def.periodic = (e.flags & 2) != 0;
        def.rational = (e.flags & 4) != 0;
        def.planar   = (e.flags & 8) != 0;
        def.linear   = (e.flags & 16) != 0;

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

        const DimStyleFigures fig = figures_of(e.getStyle());
        core::DimensionDef def;
        def.type                = type;
        def.rotation_udeg       = dxf::udeg_from_degrees(rotation_deg);
        def.user_text_position  = (e.type & 128) != 0;
        def.ordinate_x          = (e.type & 64) != 0;
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

        // The text: centred on the file's text point, along the dimension line.
        const Point2 centre = to_mm(dxf::Pt{e.getTextPoint().x, e.getTextPoint().y});
        Mm height           = to_mm_len(fig.text_height);
        if (height <= 0) height = core::mm_from_metres(2.5);
        const std::string text = core::dimension_text(def, unit_);
        std::size_t glyphs     = 0;
        for (const char c : text)
            if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) ++glyphs;
        const Mm advance = std::max<Mm>(1, (height * 6 * static_cast<Mm>(glyphs)) / 10);
        std::int64_t dir = 0;
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
            place_insert(p.data, p.in_block);
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
                   core::TextAnchor anchor, const char* type)
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
        // caption is wide — the same rule `METİN` and the GDAL path use.
        std::size_t glyphs = 0;
        for (const char c : words)
            if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) ++glyphs;
        const Mm advance = std::max<Mm>(1, (height * 6 * static_cast<Mm>(glyphs)) / 10);
        const Point2 end = dxf::point_on_circle(where, advance, angle);
        const Point2 baseline[2]{where, end};
        auto made = place_polyline(layer_for(e, in), std::span<const Point2>(baseline, 2));
        if (!made) {
            fail(err(made.error().code, std::string(type) + " okunamadı: " + made.error().message));
            return;
        }
        if (auto st = tx_.set_text(made.value(), std::move(words), height, anchor); !st) {
            fail(err(st.error().code,
                     std::string(type) + " yazısı yazılamadı: " + st.error().message));
            return;
        }
        finish_entity(e, type, made.value(), {pz}, false);
    }

    void emit_hatch(const DRW_Hatch& e, const Xform& x, const Inherit& in)
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
            if ((lp->type & 2) != 0) {
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
        // pattern its payload — name, angle, scale as the file states them, the
        // line families from the pattern catalogue by name, since the library
        // does not hand the file's own definition lines over.
        core::HatchDef def;
        def.solid        = e.solid != 0;
        def.name         = e.name.empty() ? std::string(def.solid ? "SOLID" : "ANSI31") : e.name;
        def.double_lines = e.doubleflag != 0;
        def.associative  = e.associative != 0;
        def.style        = static_cast<std::uint16_t>(std::clamp(e.hstyle, 0, 2));
        def.pattern_type = static_cast<std::uint16_t>(std::clamp(e.hpattern, 0, 2));
        def.angle_udeg   = dxf::udeg_from_degrees(e.angle);
        def.scale        = ratio_of(e.scale > 0.0 ? e.scale : 1.0);
        def.origin       = to_mm(dxf::Pt{e.basePoint.x, e.basePoint.y});
        if (!def.solid) {
            if (const command::HatchPattern* p = patterns().find(def.name); p != nullptr)
                def.families = p->families;
            else
                ++hatch_unknown_;
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
    }

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

    std::uint64_t seen_{0}, skipped_{0}, paper_space_{0}, ltypes_{0};
    std::uint64_t blocks_read_{0}, refs_read_{0}, spline_fit_only_{0}, hatch_unknown_{0},
        dim_style_missing_{0}, widths_dropped_{0}, z_varies_{0}, kot_written_{0},
        text_align_approx_{0}, byblock_seen_{0}, tilted_{0}, xdata_kept_{0}, linetypes_dropped_{0};
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
        } else if (prj.error().code == ErrorCode::NotFound ||
                   prj.error().code == ErrorCode::Unsupported) {
            if (options.project_crs.empty())
                co_return err(ErrorCode::ValidationFailed,
                              "'" + path +
                                  "' koordinat sistemi bildirmiyor (DXF taşıyamaz) ve çizimin "
                                  "kendi sistemi de yok. Yanına aynı adlı bir .prj dosyası koyun "
                                  "ya da AYAR koordinat_sistemi ile çizimin sistemini kurun.");
            sink.set_crs(options.project_crs);
            sink.report().diagnostics.note(
                Severity::Info,
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

    if (auto st = sink.conclude(); !st) co_return st.error();
    sink.report().version = dxf::acad_name(reader.getVersion());
    co_return std::move(sink.report());
}

#endif // KENTOS_HAVE_DXFRW

} // namespace kentos::io
