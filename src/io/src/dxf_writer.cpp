// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: writing a DXF through libdxfrw.
//
// The library drives the file's sections and calls back here for the tables,
// the blocks and the entities; this file answers from the document, kind by
// kind: a circle is a CIRCLE, an arc an ARC, an ellipse an ELLIPSE, a caption a
// TEXT — never a polygon standing in for a curve (the GDAL path's lasting
// limitation, io.md R13). The DRW_* types stay inside this .cpp (io.md R2).
#include "kentos_cad/io/dxf.hpp"
#include "kentos_cad/io/staging.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/spline.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"

#include "dxf_common.hpp"
#include "dxf_units.hpp"
#include "prj_sidecar.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
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

command::Task<core::Result<DxfReport>> export_dxf(const core::Document& doc, std::string path,
                                                  ExportOptions options, DxfVersion version,
                                                  command::JobControl control)
{
    (void)doc;
    (void)options;
    (void)version;
    (void)control;
    co_return err(ErrorCode::Unsupported,
                  "'" + path +
                      "' libdxfrw ile yazılamaz: bu yapı KENTOS_WITH_DXFRW=OFF ile "
                      "derlendi. " +
                      dxf_backend_status());
}

#else

namespace {

/// A block or layer name as a DXF may carry it. A `|` in a symbol name means
/// "belongs to an external reference" to AutoCAD, which then looks for the
/// reference and finds none; the names an external reference's layers and
/// blocks carry here (`ALTLIK|YOL`, TODOS C-14) are therefore written the way
/// AutoCAD itself writes a bound reference's, `ALTLIK$0$YOL`.
std::string dxf_symbol(const std::string& name)
{
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        if (c == '|')
            out += "$0$";
        else
            out += c;
    }
    return out;
}

using core::Mm;
using core::Point2;

constexpr int kStopStride = 4096;

/// The source of everything libdxfrw writes.
/// Groups the library cannot write, to go into one record right after its own
/// group `after`: a hatch's pattern lines after 78, an MTEXT's direction after
/// 44 — spliced in once the file is closed (`splice_xdata`).
struct GroupInsert
{
    unsigned handle{0};                              ///< the record, by the handle it was given
    int after{0};                                    ///< the group they follow
    std::vector<std::pair<int, std::string>> groups; ///< code and value, in order
    /// When not empty, the group they follow must also carry this value — a
    /// subclass marker (`100 AcDbDimension`) is one of several 100s in a record.
    std::string after_value{};
};

/// A number as a DXF value: shortest round-trip form.
std::string number_text(double d);

class DxfSource final : public DRW_Interface
{
public:
    DxfSource(const core::Document& doc, dxfRW& out, core::DrawingUnit unit, DRW::Version version,
              command::JobControl control)
        : doc_(doc), out_(out), unit_(unit), version_(version), control_(std::move(control))
    {}

    DxfReport& report() noexcept { return report_; }

    bool cancelled() const noexcept { return cancelled_; }

    // ---- the reader half of the interface: unused on a write ----------------
    void addHeader(const DRW_Header*) override {}

    void addLType(const DRW_LType&) override {}

    void addLayer(const DRW_Layer&) override {}

    void addDimStyle(const DRW_Dimstyle&) override {}

    void addVport(const DRW_Vport&) override {}

    void addTextStyle(const DRW_Textstyle&) override {}

    void addAppId(const DRW_AppId&) override {}

    void addBlock(const DRW_Block&) override {}

    void setBlock(const int) override {}

    void endBlock() override {}

    void addPoint(const DRW_Point&) override {}

    void addLine(const DRW_Line&) override {}

    void addRay(const DRW_Ray&) override {}

    void addXline(const DRW_Xline&) override {}

    void addArc(const DRW_Arc&) override {}

    void addCircle(const DRW_Circle&) override {}

    void addEllipse(const DRW_Ellipse&) override {}

    void addLWPolyline(const DRW_LWPolyline&) override {}

    void addPolyline(const DRW_Polyline&) override {}

    void addSpline(const DRW_Spline*) override {}

    void addKnot(const DRW_Entity&) override {}

    void addInsert(const DRW_Insert&) override {}

    void addTrace(const DRW_Trace&) override {}

    void add3dFace(const DRW_3Dface&) override {}

    void addSolid(const DRW_Solid&) override {}

    void addMText(const DRW_MText&) override {}

    void addText(const DRW_Text&) override {}

    void addDimAlign(const DRW_DimAligned*) override {}

    void addDimLinear(const DRW_DimLinear*) override {}

    void addDimRadial(const DRW_DimRadial*) override {}

    void addDimDiametric(const DRW_DimDiametric*) override {}

    void addDimAngular(const DRW_DimAngular*) override {}

    void addDimAngular3P(const DRW_DimAngular3p*) override {}

    void addDimOrdinate(const DRW_DimOrdinate*) override {}

    void addLeader(const DRW_Leader*) override {}

    void addHatch(const DRW_Hatch*) override {}

    void addViewport(const DRW_Viewport&) override {}

    void addImage(const DRW_Image*) override {}

    void linkImage(const DRW_ImageDef*) override {}

    void addComment(const char*) override {}

    void addPlotSettings(const DRW_PlotSettings*) override {}

    // ---- the write half -----------------------------------------------------

    void writeHeader(DRW_Header& data) override
    {
        data.addInt("$INSUNITS", insunits_code(unit_), 70);
        data.addInt("$MEASUREMENT", 1, 70);
        data.addDouble("$LTSCALE", 1.0, 40);
        const core::Box2 box = doc_.extent();
        if (!box.empty()) {
            data.addCoord("$EXTMIN", DRW_Coord(units(box.min_x), units(box.min_y), 0.0), 10);
            data.addCoord("$EXTMAX", DRW_Coord(units(box.max_x), units(box.max_y), 0.0), 10);
        }
        // A file older than 2007 is not UTF-8; Turkish text needs the code page
        // said, or every Turkish letter comes out as a question mark in AutoCAD.
        if (version_ < DRW::AC1021) data.addStr("$DWGCODEPAGE", "ANSI_1254", 3);
    }

    void writeLTypes() override {} // ByBlock, ByLayer and Continuous are the library's

    void writeBlockRecords() override
    {
        for (const core::BlockDef& def : doc_.blocks().all())
            out_.writeBlockRecord(dxf_symbol(def.name));
        name_pictures();
        for (const auto& [e, name] : pictures_)
            out_.writeBlockRecord(name);
    }

    void writeBlocks() override
    {
        // Every definition, with its members written inside it exactly as an
        // entity of the drawing is written — the same `write_entity`, so a
        // circle in a block is a CIRCLE and a caption a TEXT (model.md R45).
        for (const core::BlockDef& def : doc_.blocks().all()) {
            DRW_Block blk;
            blk.name      = dxf_symbol(def.name);
            blk.basePoint = DRW_Coord(units(def.base.x), units(def.base.y), 0.0);
            out_.writeBlock(&blk);
            for (const core::EntityKey key : def.members) {
                const core::EntityId m = doc_.slot_of(key);
                if (m == core::kNoEntity || !doc_.entities().alive(m)) continue;
                write_entity(m);
            }
            ++report_.blocks;
        }
        // Every DIMENSION's own picture, the anonymous block its group 2 names.
        name_pictures();
        for (const auto& [e, name] : pictures_) {
            DRW_Block blk;
            blk.name      = name;
            blk.flags     = 1; // anonymous
            blk.basePoint = DRW_Coord(0.0, 0.0, 0.0);
            out_.writeBlock(&blk);
            write_picture(e);
        }
    }

    void writeObjects() override {}

    void writeVports() override
    {
        DRW_Vport v;
        v.name = "*Active";
        out_.writeVport(&v);
    }

    void writeDimstyles() override
    {
        // STANDARD always, then every style a dimension names, with the figures
        // that dimension carries brought back to drawing units — so a reader
        // that trusts the table draws the arrows the size this drawing drew them.
        const core::EntityTable& ents = doc_.entities();
        DRW_Dimstyle standard;
        standard.name = "STANDARD";
        out_.writeDimstyle(&standard);

        // In order of first use: the first dimension of each style gives its
        // figures, the first ANGLE of it the angle unit — a style's table row
        // is one row, whichever of its dimensions comes first.
        struct Use
        {
            std::string name;
            core::EntityId first{core::kNoEntity};
            core::EntityId angle{core::kNoEntity};
        };

        std::vector<Use> uses;
        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if (!ents.alive(e) || ents.kind[e] != core::kDimensionKind) continue;
            auto def = core::dimension_of(doc_.geometry(), ents.slot[e]);
            if (!def || core::turkish_key_equals(def.value().style, "STANDARD")) continue;
            auto use = std::ranges::find_if(uses, [&def](const Use& u) {
                return core::turkish_key_equals(u.name, def.value().style);
            });
            if (use == uses.end()) {
                uses.push_back(Use{def.value().style, e, core::kNoEntity});
                use = std::prev(uses.end());
            }
            if (use->angle == core::kNoEntity && core::dimension_is_angle(def.value()))
                use->angle = e;
        }
        for (const Use& use : uses) {
            auto def = core::dimension_of(doc_.geometry(), ents.slot[use.first]);
            if (!def) continue;
            DRW_Dimstyle d;
            d.name     = use.name;
            d.dimscale = 1.0;
            d.dimasz   = units(def.value().arrow_size);
            d.dimexe   = units(def.value().extension_beyond);
            d.dimexo   = units(def.value().extension_offset);
            d.dimgap   = units(def.value().text_gap);
            d.dimtxt   = doc_.texts().has(ents.slot[use.first])
                             ? units(doc_.texts().height(ents.slot[use.first]))
                             : d.dimasz;
            d.dimdec   = def.value().precision;
            d.dimdsep  = static_cast<unsigned char>(def.value().decimal_separator);
            // THE SHEET'S OWN RULES, for a reader that regenerates a dimension
            // rather than drawing its picture (TODOS C-17): the figure above
            // its line and along it (ISO 129-1), and the head this style draws
            // — the default closed one filled, an architectural tick as the
            // oblique stroke DIMTSZ asks for, an open head by its block name.
            d.dimtad = 1;
            d.dimtih = 0;
            d.dimtoh = 0;
            switch (def.value().arrow) {
            case core::ArrowStyle::Tick: d.dimtsz = d.dimasz; break;
            case core::ArrowStyle::Open: d.dimblk = "_OPEN"; break;
            case core::ArrowStyle::Closed: break;
            }
            if (use.angle != core::kNoEntity) {
                if (auto angle = core::dimension_of(doc_.geometry(), ents.slot[use.angle])) {
                    // DIMAUNIT: 0 decimal degrees, 2 grad, 3 radians.
                    switch (core::dimension_angle_unit(angle.value())) {
                    case core::AngleUnit::Grad: d.dimaunit = 2; break;
                    case core::AngleUnit::Radian: d.dimaunit = 3; break;
                    case core::AngleUnit::Degree: d.dimaunit = 0; break;
                    }
                    d.dimadec = angle.value().precision;
                }
            }
            out_.writeDimstyle(&d);
        }
    }

    void writeTextstyles() override
    {
        // THE FACE THE CAPTIONS WERE SET IN (TODOS C-12): every caption of
        // this program is drawn in the bundled IBM Plex Sans, so that is what
        // the style names — a reader that has it draws the sheet's own
        // letters, and one that does not substitutes knowingly rather than
        // being told `txt`, which the captions never were.
        DRW_Textstyle t;
        t.name = "STANDARD";
        t.font = "IBMPlexSans-Regular.ttf";
        out_.writeTextstyle(&t);
    }

    void writeAppId() override
    {
        DRW_AppId a;
        a.name = "KENTOSCAD";
        out_.writeAppId(&a);
    }

    void writeLayers() override
    {
        // Every layer, so the structure a user built comes back — including an
        // empty one, which is a decision they made and a DXF can carry.
        for (const core::Layer& l : doc_.layers()) {
            DRW_Layer out;
            out.name                = dxf_symbol(l.name);
            const std::uint32_t rgb = l.appearance.rgba & 0x00FFFFFFu;
            out.color               = dxf::aci_for_ink(rgb);
            out.color24             = static_cast<int>(rgb);
            if (!l.visible) out.color = -out.color; // negative colour = layer off
            out.lWeight  = l.appearance.width_um > 0
                               ? DRW_LW_Conv::dxfInt2lineWidth(
                                    dxf::dxf_lineweight_from_um(l.appearance.width_um))
                               : DRW_LW_Conv::widthDefault;
            out.lineType = "CONTINUOUS";
            out.flags    = l.locked ? 4 : 0;
            out.plotF    = l.plottable;
            out_.writeLayer(&out);
            ++report_.layers;
        }
        if (doc_.dashes().size() > 1)
            report_.diagnostics.note(Severity::Info,
                                     "Çizgi tipleri bu sürümde DXF'e yazılmadı; her katman "
                                     "CONTINUOUS.");
    }

    void writeEntities() override
    {
        const core::EntityTable& ents = doc_.entities();
        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if ((e % kStopStride) == 0) {
                if (control_.cancelled()) {
                    cancelled_ = true;
                    return;
                }
                control_.at(e, ents.size());
            }
            // Hidden entities and block members are not on the drawing (members
            // are written inside their BLOCK); unknown kinds have no DXF shape
            // this build can name.
            if (!ents.visible(e)) continue;
            if (!doc_.kind_known(e)) {
                ++unknown_;
                continue;
            }
            write_entity(e);
        }
        control_.at(1, 1); // every object written; what follows is the close
        if (unknown_ != 0)
            report_.diagnostics.note(
                Severity::Skipped, std::to_string(unknown_) +
                                       " nesne bu yapının tanımadığı türdeydi ve DXF'e yazılmadı.");
        // A CLIP IS NOT CARRIED: AutoCAD keeps one in a SPATIAL_FILTER object
        // libdxfrw cannot write, so the INSERT goes out whole and says so.
        if (clipped_ != 0)
            report_.diagnostics.note(
                Severity::Degraded,
                std::to_string(clipped_) +
                    " blok referansının BLOKKIRP sınırı DXF'e taşınmadı; bu referanslar DXF'te "
                    "kırpılmadan, bütün görünür.");
    }

private:
    /// One entity as the DXF entity its kind is. A dimension's caption is part
    /// of the DIMENSION; any other captioned slot is a TEXT.
    void write_entity(core::EntityId e)
    {
        const core::EntityTable& ents = doc_.entities();
        const core::RingGeometry& geo = doc_.geometry();
        const std::uint32_t slot      = ents.slot[e];
        const core::KindId kind       = ents.kind[e];
        {
            if (doc_.texts().has(slot) && kind != core::kDimensionKind) {
                write_text(e, slot);
                return;
            }
            if (kind == core::kCircleKind) {
                DRW_Circle c;
                common(c, e);
                const Point2 centre = core::circle_centre_of(geo, slot);
                c.basePoint         = DRW_Coord(units(centre.x), units(centre.y), 0.0);
                c.radious           = units(core::circle_radius_of(geo, slot));
                out_.writeCircle(&c);
                remember_xdata(c);
            } else if (kind == core::kArcKind) {
                DRW_Arc a;
                common(a, e);
                const Point2 centre = core::arc_centre_of(geo, slot);
                const Point2 from   = core::arc_start_of(geo, slot);
                const Point2 to     = core::arc_end_of(geo, slot);
                a.basePoint         = DRW_Coord(units(centre.x), units(centre.y), 0.0);
                a.radious           = units(core::arc_radius_of(geo, slot));
                a.staangle =
                    dxf::radians_from_udeg(core::atan2_udeg(from.y - centre.y, from.x - centre.x));
                a.endangle =
                    dxf::radians_from_udeg(core::atan2_udeg(to.y - centre.y, to.x - centre.x));
                out_.writeArc(&a);
                remember_xdata(a);
            } else if (kind == core::kEllipseKind) {
                DRW_Ellipse el;
                common(el, e);
                const Point2 centre = core::ellipse_centre_of(geo, slot);
                const Point2 major  = core::ellipse_major_of(geo, slot);
                const Point2 minor  = core::ellipse_minor_of(geo, slot);
                el.basePoint        = DRW_Coord(units(centre.x), units(centre.y), 0.0);
                el.secPoint = DRW_Coord(units(major.x - centre.x), units(major.y - centre.y), 0.0);
                const double la = std::hypot(static_cast<double>(major.x - centre.x),
                                             static_cast<double>(major.y - centre.y));
                const double lb = std::hypot(static_cast<double>(minor.x - centre.x),
                                             static_cast<double>(minor.y - centre.y));
                el.ratio        = la > 0.0 ? lb / la : 1.0;
                el.staparam     = 0.0;
                el.endparam     = 2.0 * core::kPi;
                if (const auto arc = core::ellipse_arc_of(geo, slot); arc.has_value()) {
                    el.staparam = dxf::radians_from_udeg(arc->start_udeg);
                    el.endparam = dxf::radians_from_udeg(arc->end_udeg);
                }
                out_.writeEllipse(&el);
                remember_xdata(el);
            } else if (kind == core::kPointKind) {
                DRW_Point p;
                common(p, e);
                const Point2 at = core::point_position_of(geo, slot);
                p.basePoint     = DRW_Coord(units(at.x), units(at.y), 0.0);
                out_.writePoint(&p);
                remember_xdata(p);
            } else if (kind == core::kArcPolylineKind) {
                write_arc_polyline(e, slot);
            } else if (kind == core::kSplineKind) {
                write_spline(e, slot);
            } else if (kind == core::kHatchKind) {
                write_hatch(e, slot);
            } else if (kind == core::kBlockReferenceKind) {
                write_insert(e, slot);
            } else if (kind == core::kDimensionKind) {
                write_dimension(e, slot);
            } else if (kind == core::kLeaderKind) {
                write_leader(e, slot);
            } else {
                // A polyline or a face: one LWPOLYLINE per ring, closed when the
                // ring is. A hole is its own closed polyline on the same layer.
                const core::RingSpan span = geo.rings_of(slot);
                for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                    DRW_LWPolyline pl;
                    common(pl, e);
                    pl.flags      = geo.ring_role[r] != core::RingRole::Open ? 1 : 0;
                    const auto xs = geo.ring_xs(r);
                    const auto ys = geo.ring_ys(r);
                    for (std::size_t v = 0; v < xs.size(); ++v) {
                        DRW_Vertex2D vert;
                        vert.x = units(xs[v]);
                        vert.y = units(ys[v]);
                        pl.addVertex(vert);
                    }
                    pl.vertexnum = static_cast<int>(xs.size());
                    out_.writeLWPolyline(&pl);
                    remember_xdata(pl);
                }
            }
            ++report_.entities;
        }
    }

    void write_arc_polyline(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        const core::RingSpan span     = geo.rings_of(slot);
        if (span.count == 0) return;
        auto def = core::arc_polyline_of(geo, slot);
        DRW_LWPolyline pl;
        common(pl, e);
        const bool closed   = geo.ring_role[span.first] != core::RingRole::Open;
        pl.flags            = closed ? 1 : 0;
        const auto xs       = geo.ring_xs(span.first);
        const auto ys       = geo.ring_ys(span.first);
        const std::size_t n = xs.size();
        if (def && def.value().constant_width > 0) pl.width = units(def.value().constant_width);
        std::size_t next_arc = 0;
        for (std::size_t v = 0; v < n; ++v) {
            DRW_Vertex2D vert;
            vert.x = units(xs[v]);
            vert.y = units(ys[v]);
            // The bulge of the edge leaving this vertex, when it bends.
            if (def) {
                const auto& arcs = def.value().arcs;
                while (next_arc < arcs.size() && arcs[next_arc].segment < v)
                    ++next_arc;
                if (next_arc < arcs.size() && arcs[next_arc].segment == v) {
                    const core::ArcPolyline::Arc& a = arcs[next_arc];
                    const Point2 from{xs[v], ys[v]};
                    const Point2 to{xs[(v + 1) % n], ys[(v + 1) % n]};
                    vert.bulge = core::bulge_from_arc(from, to, a.centre, a.radius, a.ccw);
                }
            }
            pl.addVertex(vert);
        }
        pl.vertexnum = static_cast<int>(n);
        out_.writeLWPolyline(&pl);
        remember_xdata(pl);
    }

    void write_spline(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        const core::RingSpan span     = geo.rings_of(slot);
        if (span.count == 0) return;
        auto def = core::spline_of(geo, slot);
        if (!def) return;
        const core::SplineDef& d = def.value();
        DRW_Spline sp;
        common(sp, e);
        sp.normalVec = DRW_Coord(0.0, 0.0, 1.0);
        sp.degree    = d.degree;
        sp.flags =
            static_cast<int>((d.closed ? 1U : 0U) | (d.periodic ? 2U : 0U) |
                             (d.rational ? 4U : 0U) | (d.planar ? 8U : 0U) | (d.linear ? 16U : 0U));
        const auto cx = geo.ring_xs(span.first);
        const auto cy = geo.ring_ys(span.first);
        for (std::size_t i = 0; i < cx.size(); ++i)
            sp.controllist.push_back(std::make_shared<DRW_Coord>(units(cx[i]), units(cy[i]), 0.0));
        std::vector<std::int64_t> knots = d.knots_nano;
        if (knots.empty()) knots = core::uniform_clamped_knots(cx.size(), d.degree);
        for (const std::int64_t k : knots)
            sp.knotslist.push_back(static_cast<double>(k) / static_cast<double>(core::kNano));
        for (const std::int64_t w : d.weights_nano)
            sp.weightlist.push_back(static_cast<double>(w) / static_cast<double>(core::kNano));
        if (d.has_fit && span.count > 1) {
            const auto fx = geo.ring_xs(span.first + 1);
            const auto fy = geo.ring_ys(span.first + 1);
            for (std::size_t i = 0; i < fx.size(); ++i)
                sp.fitlist.push_back(std::make_shared<DRW_Coord>(units(fx[i]), units(fy[i]), 0.0));
        }
        sp.nknots   = static_cast<int>(sp.knotslist.size());
        sp.ncontrol = static_cast<int>(sp.controllist.size());
        sp.nfit     = static_cast<int>(sp.fitlist.size());
        out_.writeSpline(&sp);
        remember_xdata(sp);
    }

    void write_hatch(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        auto def                      = core::hatch_of(geo, slot);
        if (!def) return;
        const core::HatchDef& d = def.value();
        DRW_Hatch h;
        common(h, e);
        h.name        = d.name;
        h.solid       = d.solid ? 1 : 0;
        h.associative = d.associative ? 1 : 0;
        h.hstyle      = d.style;
        h.hpattern    = d.pattern_type;
        h.doubleflag  = d.double_lines ? 1 : 0;
        h.angle       = dxf::degrees_from_udeg(d.angle_udeg);
        // THE PATTERN AS IT IS DRAWN (dxf_common.hpp): the scale against the
        // metric pattern file in this file's unit, and the lines themselves —
        // which the library cannot write, so they are spliced in after group
        // 78 once the file is closed.
        h.scale                                = dxf::dxf_pattern_scale(d, unit_);
        std::vector<dxf::PatternLine> patterns = dxf::pattern_lines(d, unit_);
        h.deflines                             = static_cast<int>(patterns.size());
        h.extPoint                             = DRW_Coord(0.0, 0.0, 1.0);
        // Every ring as an edge loop of lines: the library writes edge loops
        // and not polyline loops.
        const core::RingSpan span = geo.rings_of(slot);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            auto loop           = std::make_shared<DRW_HatchLoop>(0);
            const auto xs       = geo.ring_xs(r);
            const auto ys       = geo.ring_ys(r);
            const std::size_t n = xs.size();
            for (std::size_t v = 0; v < n; ++v) {
                auto line       = std::make_shared<DRW_Line>();
                line->basePoint = DRW_Coord(units(xs[v]), units(ys[v]), 0.0);
                line->secPoint  = DRW_Coord(units(xs[(v + 1) % n]), units(ys[(v + 1) % n]), 0.0);
                loop->objlist.push_back(line);
            }
            loop->update();
            h.appendLoop(loop);
        }
        out_.writeHatch(&h);
        remember_xdata(h);
        if (!patterns.empty()) {
            GroupInsert lines{h.handle, 78, {}};
            for (const dxf::PatternLine& l : patterns) {
                lines.groups.emplace_back(53, number_text(l.angle_deg));
                lines.groups.emplace_back(43, number_text(l.base_x));
                lines.groups.emplace_back(44, number_text(l.base_y));
                lines.groups.emplace_back(45, number_text(l.offset_x));
                lines.groups.emplace_back(46, number_text(l.offset_y));
                lines.groups.emplace_back(79, std::to_string(l.dashes.size()));
                for (const double dash : l.dashes)
                    lines.groups.emplace_back(49, number_text(dash));
            }
            pending_inserts_.push_back(std::move(lines));
        }
    }

    void write_insert(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        auto def                      = core::block_reference_of(geo, slot);
        if (!def || def.value().block >= doc_.blocks().size()) return;
        const core::BlockReference& r = def.value();
        if (!r.clip.empty()) ++clipped_;
        DRW_Insert ins;
        common(ins, e);
        ins.name        = dxf_symbol(doc_.blocks().at(r.block).name);
        const Point2 at = core::block_reference_insertion(geo, slot);
        ins.basePoint   = DRW_Coord(units(at.x), units(at.y), 0.0);
        ins.xscale      = static_cast<double>(r.sx.num) / static_cast<double>(r.sx.den);
        ins.yscale      = static_cast<double>(r.sy.num) / static_cast<double>(r.sy.den);
        ins.zscale      = 1.0;
        ins.angle       = dxf::radians_from_udeg(r.rotation_udeg);
        ins.colcount    = r.columns;
        ins.rowcount    = r.rows;
        ins.colspace    = units(r.column_spacing);
        ins.rowspace    = units(r.row_spacing);
        out_.writeInsert(&ins);
        remember_xdata(ins);
    }

    void write_dimension(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        auto def                      = core::dimension_of(geo, slot);
        if (!def) return;
        const core::DimensionDef& d = def.value();
        const core::RingSpan span   = geo.rings_of(slot);
        if (span.count < 2) return;
        std::vector<DRW_Coord> defs;
        {
            const auto xs = geo.ring_xs(span.first + 1);
            const auto ys = geo.ring_ys(span.first + 1);
            for (std::size_t i = 0; i < xs.size(); ++i)
                defs.emplace_back(units(xs[i]), units(ys[i]), 0.0);
        }
        if (defs.size() < core::dimension_point_count(d.type)) return;
        DRW_Coord text_at(0.0, 0.0, 0.0);
        {
            const auto xs = geo.ring_xs(span.first);
            const auto ys = geo.ring_ys(span.first);
            if (!xs.empty()) text_at = DRW_Coord(units(xs[0]), units(ys[0]), 0.0);
        }
        // GROUP 1 KEEPS A MEASURED FIGURE MEASURED (TODOS C-10). A caption typed
        // by hand goes out as typed; one this program decorated — a prefix, a
        // tolerance, a template — goes out as `<>` in its place, so the reader
        // measures the figure itself and a round trip keeps it measured. Only a
        // dimension with a unit of its own is written out whole: a reader's
        // `<>` would come back in the file's unit, and an angle in the reader's
        // degrees on a sheet measured in grad.
        const bool manual   = core::dimension_text_is_manual(d);
        std::string caption = d.override_text;
        std::string pattern = "<>"; // the caption with `<>` where the figure goes
        if (!manual) {
            const bool decorated = !d.prefix.empty() || !d.suffix.empty() ||
                                   d.tolerance != core::DimTolerance::None ||
                                   !d.override_text.empty();
            if (decorated) {
                std::string tol = core::dimension_tolerance_text(d, unit_);
                if (tol.starts_with("±")) tol = "%%p" + tol.substr(std::string_view("±").size());
                const std::string figure = d.tolerance == core::DimTolerance::Limits
                                               ? std::string(doc_.texts().text(slot))
                                               : d.prefix + "<>" + tol + d.suffix;
                if (d.override_text.empty()) {
                    pattern = figure;
                } else {
                    pattern.clear();
                    const std::string_view typed = d.override_text;
                    std::size_t from             = 0;
                    for (std::size_t at = typed.find("<>"); at != std::string_view::npos;
                         at             = typed.find("<>", from)) {
                        pattern += typed.substr(from, at - from);
                        pattern += figure;
                        from = at + 2;
                    }
                    pattern += typed.substr(from);
                }
            }
            if (d.unit != 0) {
                caption = std::string(doc_.texts().text(slot));
            } else if (decorated) {
                caption = pattern;
            }
        }

        // WHAT A DIMENSION RECORD CANNOT SAY, for this program's own reader, in
        // its XDATA group under reserved `olcu.` keys: that an angular record is
        // an arc-length dimension (below), and the unit of a figure written out
        // whole, with the pattern it was written from — so a round trip gets a
        // MEASURED figure back rather than a typed one that happens to match.
        // Another program ignores the group and reads what group 1 says.
        std::vector<std::string> notes;
        if (d.type == core::DimensionType::ArcLength) notes.emplace_back("olcu.tur=yay");
        if (d.unit != 0) {
            notes.push_back(std::string("olcu.birim=") + core::dimension_unit_word(d));
            if (!manual) notes.push_back("olcu.yazi=" + pattern);
        }
        const auto fill = [&](DRW_Dimension& out, int type_bits) {
            common(out, e);
            out.type =
                static_cast<int>(static_cast<unsigned>(type_bits) |
                                 (d.user_text_position ? 128U : 0U) | (d.ordinate_x ? 64U : 0U));
            out.setTextPoint(text_at);
            out.setStyle(d.style);
            out.setText(caption);
            out.setExtrusion(DRW_Coord(0.0, 0.0, 1.0));
            out.setAlign(5);
            for (const std::string& note : notes)
                own_item(out, note);
        };
        switch (d.type) {
        case core::DimensionType::Linear: {
            DRW_DimLinear out;
            fill(out, 0);
            out.setDef1Point(defs[0]);
            out.setDef2Point(defs[1]);
            out.setDimPoint(defs[2]);
            out.setAngle(dxf::degrees_from_udeg(d.rotation_udeg));
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::Aligned: {
            DRW_DimAligned out;
            fill(out, 1);
            out.setDef1Point(defs[0]);
            out.setDef2Point(defs[1]);
            out.setDimPoint(defs[2]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::Radial: {
            DRW_DimRadial out;
            fill(out, 4);
            out.setCenterPoint(defs[0]);
            out.setDiameterPoint(defs[1]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::Diametric: {
            DRW_DimDiametric out;
            fill(out, 3);
            out.setDiameter1Point(defs[0]);
            out.setDiameter2Point(defs[1]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::Angular: {
            DRW_DimAngular out;
            fill(out, 2);
            out.setFirstLine1(defs[0]);
            out.setFirstLine2(defs[1]);
            out.setSecondLine1(defs[2]);
            out.setSecondLine2(defs[3]);
            out.setDimPoint(defs[4]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::Angular3P: {
            DRW_DimAngular3p out;
            fill(out, 5);
            out.SetVertexPoint(defs[0]);
            out.setFirstLine(defs[1]);
            out.setSecondLine(defs[2]);
            out.setDimPoint(defs[3]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::Ordinate: {
            DRW_DimOrdinate out;
            fill(out, 6);
            out.setOriginPoint(defs[0]);
            out.setFirstLine(defs[1]);
            out.setSecondLine(defs[2]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        case core::DimensionType::ArcLength: {
            // DXF HAS NO ARC-LENGTH DIMENSION BEFORE R2007's `DIMARC`, and
            // libdxfrw does not write one. Exported as an ANGULAR dimension over
            // the same three points, which is the shape a reader can make sense
            // of: the arc, its two radii and a figure. `olcu.tur=yay` in this
            // program's XDATA group says what it was, so a round trip through
            // this program gets the arc length back; another program sees an
            // angle and says so rather than seeing a chord and believing it.
            DRW_DimAngular out;
            fill(out, 2);
            out.setFirstLine1(defs[0]);
            out.setFirstLine2(defs[1]);
            out.setSecondLine1(defs[0]);
            out.setSecondLine2(defs[2]);
            out.setDimPoint(defs.size() > 3 ? defs[3] : defs[2]);
            out_.writeDimension(&out);
            remember_xdata(out);
            name_picture(e, out);
            break;
        }
        }
    }

    /// Every DIMENSION that will be written — on the drawing or inside a block
    /// definition — gets the name of its picture, `*D1` onward, before the
    /// tables are written: the block record table comes first in the file and
    /// libdxfrw finds a block by its record.
    void name_pictures()
    {
        if (pictures_named_) return;
        pictures_named_               = true;
        const core::EntityTable& ents = doc_.entities();
        const auto name               = [this](core::EntityId e) {
            if (!doc_.entities().alive(e) || doc_.entities().kind[e] != core::kDimensionKind)
                return;
            pictures_.emplace(e, "*D" + std::to_string(pictures_.size() + 1));
        };
        for (const core::BlockDef& def : doc_.blocks().all())
            for (const core::EntityKey key : def.members)
                if (const core::EntityId m = doc_.slot_of(key); m != core::kNoEntity) name(m);
        for (core::EntityId e = 0; e < ents.size(); ++e)
            if (ents.visible(e) && doc_.kind_known(e)) name(e);
    }

    /// A DIMENSION'S PICTURE, the anonymous block its group 2 names (TODOS
    /// C-17): the lines, arcs and heads `dimension_outline` draws — a closed
    /// head a SOLID, filled as the sheet prints it — and the caption a TEXT,
    /// on layer 0 and BYBLOCK so they take the dimension's own layer and ink.
    /// A reader that draws a DIMENSION from its block, which is every reader
    /// that does not regenerate it, shows exactly what this program showed.
    void write_picture(core::EntityId e)
    {
        const core::RingGeometry& geo = doc_.geometry();
        const std::uint32_t slot      = doc_.entities().slot[e];
        core::EmitBuffer runs;
        core::dimension_outline(geo, slot, runs);
        const auto by_block = [](DRW_Entity& x) {
            x.layer    = "0";
            x.color    = 0; // BYBLOCK
            x.lineType = "BYBLOCK";
            x.lWeight  = DRW_LW_Conv::widthByBlock;
        };
        const auto at = [&](std::uint32_t v) {
            return DRW_Coord(units(runs.xs[v]), units(runs.ys[v]), 0.0);
        };
        for (std::size_t r = 0; r < runs.run_total(); ++r) {
            const std::uint32_t first = runs.run_start[r];
            const std::uint32_t count = runs.run_count[r];
            if (count < 2) continue;
            if (runs.run_solid[r] != 0 && count == 3) {
                DRW_Solid head;
                by_block(head);
                head.basePoint  = at(first);
                head.secPoint   = at(first + 1);
                head.thirdPoint = at(first + 2);
                head.fourPoint  = at(first + 2);
                out_.writeSolid(&head);
                continue;
            }
            if (count == 2 && runs.run_closed[r] == 0) {
                DRW_Line l;
                by_block(l);
                l.basePoint = at(first);
                l.secPoint  = at(first + 1);
                out_.writeLine(&l);
                continue;
            }
            DRW_LWPolyline pl;
            by_block(pl);
            pl.flags = runs.run_closed[r] != 0 ? 1 : 0;
            for (std::uint32_t v = first; v < first + count; ++v) {
                DRW_Vertex2D vert;
                vert.x = units(runs.xs[v]);
                vert.y = units(runs.ys[v]);
                pl.addVertex(vert);
            }
            pl.vertexnum = static_cast<int>(count);
            out_.writeLWPolyline(&pl);
        }
        const core::RingSpan span = geo.rings_of(slot);
        if (span.count == 0 || !doc_.texts().has(slot)) return;
        const auto xs = geo.ring_xs(span.first);
        const auto ys = geo.ring_ys(span.first);
        if (xs.size() < 2) return;
        DRW_Text t;
        by_block(t);
        t.basePoint = DRW_Coord(units(xs[0]), units(ys[0]), 0.0);
        t.secPoint  = t.basePoint;
        t.height    = units(doc_.texts().height(slot));
        t.text      = std::string(doc_.texts().text(slot));
        t.angle     = dxf::degrees_from_udeg(core::atan2_udeg(ys[1] - ys[0], xs[1] - xs[0]));
        t.alignH    = DRW_Text::HCenter;
        t.alignV    = DRW_Text::VMiddle;
        out_.writeText(&t);
    }

    /// Names a DIMENSION's picture in its group 2, which libdxfrw does not
    /// write: spliced in right after the record's `AcDbDimension` marker,
    /// where the group belongs (after the layer in a file without markers).
    void name_picture(core::EntityId e, const DRW_Entity& written)
    {
        const auto at = pictures_.find(e);
        if (at == pictures_.end()) return;
        if (version_ > DRW::AC1009)
            pending_inserts_.push_back(
                GroupInsert{written.handle, 100, {{2, at->second}}, "AcDbDimension"});
        else
            pending_inserts_.push_back(GroupInsert{written.handle, 8, {{2, at->second}}});
    }

    void write_leader(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        auto def                      = core::leader_of(geo, slot);
        if (!def) return;
        const core::RingSpan span = geo.rings_of(slot);
        if (span.count == 0) return;
        DRW_Leader l;
        common(l, e);
        l.style       = "STANDARD";
        l.arrow       = def.value().arrow ? 1 : 0;
        l.leadertype  = def.value().spline ? 1 : 0;
        l.flag        = 3;
        l.hookline    = 0;
        l.hookflag    = 0;
        l.textheight  = 0.0;
        l.textwidth   = 0.0;
        const auto xs = geo.ring_xs(span.first);
        const auto ys = geo.ring_ys(span.first);
        for (std::size_t i = 0; i < xs.size(); ++i)
            l.vertexlist.push_back(std::make_shared<DRW_Coord>(units(xs[i]), units(ys[i]), 0.0));
        l.vertnum = static_cast<int>(xs.size());
        out_.writeLeader(&l);
        remember_xdata(l);
    }

private:
    double units(Mm mm) const noexcept { return core::drawing_units_from_mm(mm, unit_); }

    /// Appends `item` to this program's own XDATA group, opening the group when
    /// the entity's last one is not it: the attributes `common` writes and the
    /// notes a kind adds share one group.
    static void own_item(DRW_Entity& out, const std::string& item)
    {
        bool open = false;
        for (const auto& v : out.extData)
            if (v && v->code() == 1001)
                open = v->type() == DRW_Variant::STRING && v->content.s != nullptr &&
                       *v->content.s == "KENTOSCAD";
        if (!open) {
            auto app = std::make_shared<DRW_Variant>();
            app->addString(1001, "KENTOSCAD");
            out.extData.push_back(app);
        }
        auto v = std::make_shared<DRW_Variant>();
        v->addString(1000, item.size() > 255 ? item.substr(0, 255) : item);
        out.extData.push_back(v);
    }

    /// libdxfrw writes an entity's XDATA for tables only, never for entities, and
    /// assigns the handle itself as it writes. So the groups are remembered by
    /// that handle and spliced into the file afterwards (`splice_xdata`).
    void remember_xdata(const DRW_Entity& written)
    {
        if (written.extData.empty()) return;
        pending_xdata_.emplace_back(written.handle, written.extData);
    }

public:
    /// The XDATA groups the library could not write, keyed by the handle it gave
    /// each entity. Consumed once by `export_dxf` after the file is closed.
    std::vector<std::pair<unsigned, std::vector<std::shared_ptr<DRW_Variant>>>>& pending_xdata()
    {
        return pending_xdata_;
    }

    /// The groups the library could not write inside a record, by the handle it
    /// gave each one; spliced in with the XDATA.
    std::vector<GroupInsert>& pending_inserts() { return pending_inserts_; }

private:
    /// Layer, colour, weight and extended data — what every entity carries.
    void common(DRW_Entity& out, core::EntityId e)
    {
        const core::EntityTable& ents = doc_.entities();
        if (const core::Layer* l = doc_.layer(ents.layer[e]); l != nullptr)
            out.layer = dxf_symbol(l->name);

        const core::StyleId style = ents.style[e];
        if (style != core::kByLayerStyle && style < doc_.styles().size()) {
            const core::Appearance& a = doc_.styles().at(style);
            if (a.src_colour == core::Source::Explicit) {
                const std::uint32_t rgb = a.rgba & 0x00FFFFFFu;
                out.color               = dxf::aci_for_ink(rgb);
                out.color24             = static_cast<int>(rgb);
            }
            if (a.src_width == core::Source::Explicit && a.width_um > 0)
                out.lWeight =
                    DRW_LW_Conv::dxfInt2lineWidth(dxf::dxf_lineweight_from_um(a.width_um));
        }

        // Foreign data goes back as the XDATA it came from; this program's own
        // attributes and the entity's key travel under the KENTOSCAD application
        // name, one `id=value` string each, so a GIS attribute survives a trip
        // through DXF and another program can read it.
        const std::uint32_t slot = ents.slot[e];
        auto foreign             = doc_.foreign().bytes(slot, core::kForeignDxfXdata);
        if (!foreign.empty()) out.extData = dxf::decode_xdata(foreign);

        std::vector<std::string> own;
        const core::AttrTable& attrs = doc_.attributes();
        for (std::size_t c = 0; c < attrs.columns(); ++c) {
            const core::AttrColumn* col = attrs.column(static_cast<core::AttrId>(c));
            if (col == nullptr) continue;
            auto cell = attrs.get(static_cast<core::AttrId>(c), slot);
            if (!cell || !cell.value().present) continue;
            const core::AttrValue& v = cell.value();
            std::string text;
            switch (v.type) {
            case core::AttrType::Text:
            case core::AttrType::CodeRef: text = v.text; break;
            default: text = std::to_string(v.number); break;
            }
            // `id#T=value`, T the column's type, so the reader can declare the
            // column when the drawing it lands in does not have it yet.
            own.push_back(col->spec().id + "#" + std::to_string(static_cast<int>(v.type)) + "=" +
                          text);
        }
        if (!own.empty()) {
            auto app = std::make_shared<DRW_Variant>();
            app->addString(1001, "KENTOSCAD");
            out.extData.push_back(app);
            auto key = std::make_shared<DRW_Variant>();
            key->addString(1000, "anahtar=" + std::to_string(core::raw(ents.key[e])));
            out.extData.push_back(key);
            for (const std::string& s : own) {
                auto item = std::make_shared<DRW_Variant>();
                item->addString(1000, s.size() > 255 ? s.substr(0, 255) : s);
                out.extData.push_back(item);
            }
            ++with_attributes_;
        }
    }

    void write_text(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        const core::RingSpan span     = geo.rings_of(slot);
        if (span.count == 0) return;
        const auto xs = geo.ring_xs(span.first);
        const auto ys = geo.ring_ys(span.first);
        if (xs.empty()) return;

        // MORE THAN ONE LINE, OR LINES LAID OUT, IS AN MTEXT (TODOS C-12): a
        // TEXT holds one line and has no pitch, and a newline written into its
        // group 1 is a file broken at that byte.
        const std::string content(doc_.texts().text(slot));
        if (const core::TextLines lines = doc_.texts().lines(slot);
            content.find('\n') != std::string::npos || lines != core::TextLines{}) {
            write_mtext(e, slot, content, lines);
            return;
        }

        DRW_Text t;
        common(t, e);
        const Point2 at{xs.front(), ys.front()};
        t.basePoint = DRW_Coord(units(at.x), units(at.y), 0.0);
        t.height    = units(doc_.texts().height(slot));
        t.text      = content;
        if (xs.size() >= 2) {
            const Point2 end{xs.back(), ys.back()};
            t.angle = dxf::degrees_from_udeg(core::atan2_udeg(end.y - at.y, end.x - at.x));
        }
        // The anchor's column and row (text_store.hpp) are TEXT's own two groups:
        // 72 left, centre, right; 73 baseline, middle, top. Anything but
        // baseline-left is placed by its alignment point, group 11 — the anchor.
        const core::TextAnchor anchor = doc_.texts().anchor(slot);
        constexpr std::array<DRW_Text::HAlign, 3> kColumn{DRW_Text::HLeft, DRW_Text::HCenter,
                                                          DRW_Text::HRight};
        constexpr std::array<DRW_Text::VAlign, 3> kRow{DRW_Text::VBaseLine, DRW_Text::VMiddle,
                                                       DRW_Text::VTop};
        t.alignH = kColumn[static_cast<std::size_t>(core::text_anchor_column(anchor))];
        t.alignV = kRow[static_cast<std::size_t>(core::text_anchor_row(anchor))];
        if (anchor != core::TextAnchor::BaselineLeft) t.secPoint = t.basePoint;
        out_.writeText(&t);
        remember_xdata(t);
        ++report_.entities;
    }

    /// A caption as an MTEXT: the anchor its attachment point, the lines its
    /// paragraphs, the spacing its line spacing, and — for a text that wraps —
    /// the width its lines break to. Read back by `addMText` to the same text.
    void write_mtext(core::EntityId e, std::uint32_t slot, const std::string& content,
                     core::TextLines lines)
    {
        const core::RingGeometry& geo = doc_.geometry();
        const core::RingSpan span     = geo.rings_of(slot);
        const auto xs                 = geo.ring_xs(span.first);
        const auto ys                 = geo.ring_ys(span.first);
        const Point2 at{xs.front(), ys.front()};
        const Point2 end{xs.back(), ys.back()};
        const auto dx    = static_cast<double>(end.x - at.x);
        const auto dy    = static_cast<double>(end.y - at.y);
        const double len = std::sqrt(dx * dx + dy * dy);

        DRW_MText m;
        common(m, e);
        m.basePoint = DRW_Coord(units(at.x), units(at.y), 0.0);
        m.height    = units(doc_.texts().height(slot));
        // 41, the reference width: zero breaks nothing but the text's own breaks.
        m.widthscale = lines.wrap ? units(static_cast<Mm>(std::llround(len))) : 0.0;
        // 71, the attachment point: rows top 1–3, middle 4–6, bottom 7–9.
        const core::TextAnchor anchor = doc_.texts().anchor(slot);
        constexpr std::array<int, 3> kRowStart{7, 4,
                                               1}; // by `text_anchor_row`: baseline, middle, top
        m.textgen = kRowStart[static_cast<std::size_t>(core::text_anchor_row(anchor))] +
                    core::text_anchor_column(anchor);
        // THE LIBRARY WRITES TWO MTEXT GROUPS FROM TEXT'S FIELDS: 72 from
        // `alignH` and 73 from `alignV`. For an MTEXT they are the drawing
        // direction (1, left to right) and the line spacing style (2, exactly —
        // so a tall letter does not push the next line down).
        m.alignH   = static_cast<DRW_Text::HAlign>(1);
        m.alignV   = static_cast<DRW_Text::VAlign>(2);
        m.interlin = static_cast<double>(lines.spacing) / 1000.0;
        m.text     = dxf::escape_mtext(content);
        // The rotation goes as the X-axis direction (11/21/31), after group 44
        // — the last the library writes — so it prevails over group 50, which
        // stays zero: the file's own rule is that the later of the two wins.
        m.angle = 0.0;
        out_.writeMText(&m);
        remember_xdata(m);
        const double ux = len > 0.0 ? dx / len : 1.0;
        const double uy = len > 0.0 ? dy / len : 0.0;
        pending_inserts_.push_back(
            GroupInsert{m.handle, 44, {{11, number_text(ux)}, {21, number_text(uy)}, {31, "0"}}});
        ++report_.entities;
    }

    const core::Document& doc_;
    dxfRW& out_;
    core::DrawingUnit unit_;
    DRW::Version version_;
    command::JobControl control_;
    DxfReport report_;
    std::uint64_t unknown_{0};
    /// Block references whose clip the DXF does not carry (`write_insert`).
    std::uint64_t clipped_{0};
    std::uint64_t with_attributes_{0};
    bool cancelled_{false};
    std::vector<std::pair<unsigned, std::vector<std::shared_ptr<DRW_Variant>>>> pending_xdata_;
    std::vector<GroupInsert> pending_inserts_;
    /// Each DIMENSION's picture block, by entity (`name_pictures`).
    std::map<core::EntityId, std::string> pictures_;
    bool pictures_named_{false};
};

/// One XDATA group as DXF text: the code right-aligned in three columns, the
/// value on its own line, the way every writer since R12 lays them out.
void append_group(std::string& out, int code, const std::string& value)
{
    char head[8];
    (void)std::snprintf(head, sizeof(head), "%3d", code);
    out += head;
    out += '\n';
    out += value;
    out += '\n';
}

std::string number_text(double d)
{
    char buf[64];
    (void)std::snprintf(buf, sizeof(buf), "%.15g", d);
    return buf;
}

/// Splices what the library cannot write into the written file, by the handle
/// group `5` of each record: the remembered XDATA just before the next `0`
/// group — the end of that entity's record — and a hatch's definition lines
/// just after its group 78, which announces how many follow. One streaming
/// pass over the ASCII file, written beside it and renamed over it, so a
/// failure leaves the original.
core::Status splice_xdata(
    const std::string& path,
    const std::vector<std::pair<unsigned, std::vector<std::shared_ptr<DRW_Variant>>>>& pending,
    const std::vector<GroupInsert>& inserts)
{
    if (pending.empty() && inserts.empty()) return core::ok();
    const auto hex_of = [](unsigned handle) {
        char hex[24];
        (void)std::snprintf(hex, sizeof(hex), "%X", handle);
        return std::string(hex);
    };
    std::map<std::string, const std::vector<std::shared_ptr<DRW_Variant>>*> by_handle;
    for (const auto& [handle, items] : pending)
        by_handle[hex_of(handle)] = &items;
    std::map<std::string, std::vector<const GroupInsert*>> inserts_by_handle;
    for (const GroupInsert& ins : inserts)
        inserts_by_handle[hex_of(ins.handle)].push_back(&ins);

    std::ifstream in(path, std::ios::binary);
    if (!in) return err(ErrorCode::IoFailure, "'" + path + "' XDATA için geri okunamadı.");
    const std::string tmp = path + ".xdata.tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return err(ErrorCode::IoFailure, "'" + tmp + "' yazılamadı.");

    const auto trim = [](std::string s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' '))
            s.pop_back();
        std::size_t i = 0;
        while (i < s.size() && s[i] == ' ')
            ++i;
        return s.substr(i);
    };

    std::string code_line, value_line;
    bool in_entities                                         = false;
    const std::vector<std::shared_ptr<DRW_Variant>>* current = nullptr;
    // Blocks carry hatches and captions too, so inserts are looked for in every
    // section. What is left of the current record's, until it is written.
    std::vector<const GroupInsert*> due;
    const auto flush = [&] {
        if (current == nullptr) return;
        std::string block;
        for (const auto& v : *current) {
            if (!v) continue;
            switch (v->type()) {
            case DRW_Variant::STRING:
                append_group(block, v->code(), v->content.s ? *v->content.s : std::string());
                break;
            case DRW_Variant::INTEGER:
                append_group(block, v->code(), std::to_string(v->content.i));
                break;
            case DRW_Variant::DOUBLE:
                append_group(block, v->code(), number_text(v->content.d));
                break;
            case DRW_Variant::COORD:
                if (v->content.v) {
                    append_group(block, v->code(), number_text(v->content.v->x));
                    append_group(block, v->code() + 10, number_text(v->content.v->y));
                    append_group(block, v->code() + 20, number_text(v->content.v->z));
                }
                break;
            default: break;
            }
        }
        out << block;
        current = nullptr;
    };

    while (std::getline(in, code_line)) {
        if (!std::getline(in, value_line)) {
            out << code_line << '\n';
            break;
        }
        const std::string code  = trim(code_line);
        const std::string value = trim(value_line);
        if (code == "0") {
            flush();
            due.clear();
            if (value == "SECTION") in_entities = false;
        } else if (code == "2" && !in_entities && value == "ENTITIES") {
            in_entities = true;
        } else if (code == "5") {
            if (in_entities && current == nullptr)
                if (const auto at = by_handle.find(value); at != by_handle.end())
                    current = at->second;
            if (const auto at = inserts_by_handle.find(value); at != inserts_by_handle.end())
                due = at->second;
        }
        out << code_line << '\n' << value_line << '\n';
        for (auto it = due.begin(); it != due.end();) {
            if (std::to_string((*it)->after) != code ||
                (!(*it)->after_value.empty() && (*it)->after_value != value)) {
                ++it;
                continue;
            }
            std::string block;
            for (const auto& [group, text] : (*it)->groups)
                append_group(block, group, text);
            out << block;
            it = due.erase(it);
        }
    }
    flush();
    out.close();
    in.close();
    if (!out) return err(ErrorCode::IoFailure, "'" + tmp + "' yazılamadı; disk dolu olabilir.");
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec)
        return err(ErrorCode::IoFailure,
                   "'" + path + "' XDATA ile yeniden yazılamadı: " + ec.message());
    return core::ok();
}

} // namespace

command::Task<core::Result<DxfReport>> export_dxf(const core::Document& doc, std::string path,
                                                  ExportOptions options, DxfVersion version,
                                                  command::JobControl control)
{
    if (options.crs.empty())
        co_return err(ErrorCode::ValidationFailed,
                      "Çizimin koordinat sistemi yok; koordinat sistemi olmayan bir dışa aktarım "
                      "eksik veridir. AYAR koordinat_sistemi ile kurun.");

    const DRW::Version ver = dxf::drw_version_for_year(dxf_version_year(version));
    // WRITTEN BESIDE THE TARGET, MOVED ONTO IT WHEN WHOLE (io/staging.hpp): a
    // cancel or a failure leaves the file that was there. A cancel used to
    // remove the target itself — the half-written file, and with it the good
    // one it had replaced.
    Staging staged(path);
    const std::string written_to = staged.path().string();
    dxfRW writer(written_to.c_str());
    DxfSource source(doc, writer, options.unit, ver, control);
    const bool ok = writer.write(&source, ver, /*binary=*/false);
    if (source.cancelled())
        co_return err(ErrorCode::Cancelled,
                      "Dışa aktarma durduruldu; dosya yazılmadı, '" + path + "' olduğu gibi.");
    if (!ok)
        co_return err(ErrorCode::IoFailure,
                      "'" + path + "' yazılamadı (libdxfrw hata kodu " +
                          std::to_string(static_cast<int>(writer.getError())) +
                          "). Dizin izinlerini ve diski denetleyin; varsa eski dosya olduğu gibi.");

    // The XDATA and the pattern lines the library cannot write, spliced in
    // after the fact — into the staged file, which moves once it is whole.
    if (const auto st = splice_xdata(written_to, source.pending_xdata(), source.pending_inserts());
        !st)
        co_return st.error();

    // The coordinate system beside the file, the only place DXF lets it go —
    // WHEN THE NUMBERS ARE IN IT. A `.prj` names a system that counts metres, and
    // a GIS program that finds one reads the DXF's numbers as metres: beside a
    // drawing written in millimetres it would place every parcel a thousand
    // times too far out (TODOS F-03). Such a file gets no `.prj`, and the report
    // says why and how to get one.
    if (options.unit != core::DrawingUnit::Metre) {
        source.report().diagnostics.note(Severity::Warning, dxf_prj_withheld(options.unit));
    } else if (auto prj = write_prj_sidecar(written_to, options.crs); !prj) {
        if (prj.error().code == ErrorCode::Unsupported)
            source.report().diagnostics.note(Severity::Warning,
                                             ".prj yan dosyası bu yapıda yazılamadı (GDAL kapalı); "
                                             "koordinat sistemi dosyanın yanında değil.");
        else
            co_return prj.error();
    }

    if (auto st = place_staged(staged); !st) co_return st.error();

    // A `.prj` LEFT FROM AN EARLIER METRE EXPORT goes with the file it
    // described, and that is said.
    if (options.unit != core::DrawingUnit::Metre)
        if (const std::string removed = remove_stale_prj(path); !removed.empty())
            source.report().diagnostics.note(Severity::Info, removed);

    DxfReport report = std::move(source.report());
    report.version   = dxf::acad_name(ver);
    report.crs       = options.crs;
    co_return report;
}

#endif // KENTOS_HAVE_DXFRW

} // namespace kentos::io
