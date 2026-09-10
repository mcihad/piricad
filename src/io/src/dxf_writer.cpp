// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: writing a DXF through libdxfrw.
//
// The library drives the file's sections and calls back here for the tables,
// the blocks and the entities; this file answers from the document, kind by
// kind: a circle is a CIRCLE, an arc an ARC, an ellipse an ELLIPSE, a caption a
// TEXT — never a polygon standing in for a curve (the GDAL path's lasting
// limitation, io.md R13). The DRW_* types stay inside this .cpp (io.md R2).
#include "kentos_cad/io/dxf.hpp"

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
                                                  std::stop_token stop)
{
    (void)doc;
    (void)options;
    (void)version;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  "'" + path +
                      "' libdxfrw ile yazılamaz: bu yapı KENTOS_WITH_DXFRW=OFF ile "
                      "derlendi. " +
                      dxf_backend_status());
}

#else

namespace {

using core::Mm;
using core::Point2;

constexpr int kStopStride = 4096;

/// The source of everything libdxfrw writes.
class DxfSource final : public DRW_Interface
{
public:
    DxfSource(const core::Document& doc, dxfRW& out, core::DrawingUnit unit, DRW::Version version,
              std::stop_token stop)
        : doc_(doc), out_(out), unit_(unit), version_(version), stop_(std::move(stop))
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
            out_.writeBlockRecord(def.name);
    }

    void writeBlocks() override
    {
        // Every definition, with its members written inside it exactly as an
        // entity of the drawing is written — the same `write_entity`, so a
        // circle in a block is a CIRCLE and a caption a TEXT (model.md R45).
        for (const core::BlockDef& def : doc_.blocks().all()) {
            DRW_Block blk;
            blk.name      = def.name;
            blk.basePoint = DRW_Coord(units(def.base.x), units(def.base.y), 0.0);
            out_.writeBlock(&blk);
            for (const core::EntityKey key : def.members) {
                const core::EntityId m = doc_.slot_of(key);
                if (m == core::kNoEntity || !doc_.entities().alive(m)) continue;
                write_entity(m);
            }
            ++report_.blocks;
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
        std::vector<std::string> written{"STANDARD"};
        const core::EntityTable& ents = doc_.entities();
        DRW_Dimstyle standard;
        standard.name = "STANDARD";
        out_.writeDimstyle(&standard);
        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if (!ents.alive(e) || ents.kind[e] != core::kDimensionKind) continue;
            auto def = core::dimension_of(doc_.geometry(), ents.slot[e]);
            if (!def) continue;
            bool seen = false;
            for (const std::string& w : written)
                if (core::turkish_key_equals(w, def.value().style)) seen = true;
            if (seen) continue;
            written.push_back(def.value().style);
            DRW_Dimstyle d;
            d.name     = def.value().style;
            d.dimscale = 1.0;
            d.dimasz   = units(def.value().arrow_size);
            d.dimexe   = units(def.value().extension_beyond);
            d.dimexo   = units(def.value().extension_offset);
            d.dimgap   = units(def.value().text_gap);
            d.dimtxt   = doc_.texts().has(ents.slot[e]) ? units(doc_.texts().height(ents.slot[e]))
                                                        : d.dimasz;
            d.dimdec   = def.value().precision;
            d.dimdsep  = static_cast<unsigned char>(def.value().decimal_separator);
            out_.writeDimstyle(&d);
        }
    }

    void writeTextstyles() override
    {
        DRW_Textstyle t;
        t.name = "STANDARD";
        t.font = "txt";
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
            out.name                = l.name;
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
            if ((e % kStopStride) == 0 && stop_.stop_requested()) {
                cancelled_ = true;
                return;
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
        if (unknown_ != 0)
            report_.diagnostics.note(
                Severity::Skipped, std::to_string(unknown_) +
                                       " nesne bu yapının tanımadığı türdeydi ve DXF'e yazılmadı.");
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
        sp.flags     = (d.closed ? 1 : 0) | (d.periodic ? 2 : 0) | (d.rational ? 4 : 0) |
                   (d.planar ? 8 : 0) | (d.linear ? 16 : 0);
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
        h.scale       = static_cast<double>(d.scale.num) / static_cast<double>(d.scale.den);
        h.deflines    = 0;
        h.extPoint    = DRW_Coord(0.0, 0.0, 1.0);
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
    }

    void write_insert(core::EntityId e, std::uint32_t slot)
    {
        const core::RingGeometry& geo = doc_.geometry();
        auto def                      = core::block_reference_of(geo, slot);
        if (!def || def.value().block >= doc_.blocks().size()) return;
        const core::BlockReference& r = def.value();
        DRW_Insert ins;
        common(ins, e);
        ins.name        = doc_.blocks().at(r.block).name;
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
        const auto fill = [&](DRW_Dimension& out, int type_bits) {
            common(out, e);
            out.type = type_bits | (d.user_text_position ? 128 : 0) | (d.ordinate_x ? 64 : 0);
            out.setTextPoint(text_at);
            out.setStyle(d.style);
            out.setText(d.override_text);
            out.setExtrusion(DRW_Coord(0.0, 0.0, 1.0));
            out.setAlign(5);
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
            break;
        }
        case core::DimensionType::Radial: {
            DRW_DimRadial out;
            fill(out, 4);
            out.setCenterPoint(defs[0]);
            out.setDiameterPoint(defs[1]);
            out_.writeDimension(&out);
            remember_xdata(out);
            break;
        }
        case core::DimensionType::Diametric: {
            DRW_DimDiametric out;
            fill(out, 3);
            out.setDiameter1Point(defs[0]);
            out.setDiameter2Point(defs[1]);
            out_.writeDimension(&out);
            remember_xdata(out);
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
            break;
        }
        }
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

private:
    /// Layer, colour, weight and extended data — what every entity carries.
    void common(DRW_Entity& out, core::EntityId e)
    {
        const core::EntityTable& ents = doc_.entities();
        if (const core::Layer* l = doc_.layer(ents.layer[e]); l != nullptr) out.layer = l->name;

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

        DRW_Text t;
        common(t, e);
        const Point2 at{xs.front(), ys.front()};
        t.basePoint = DRW_Coord(units(at.x), units(at.y), 0.0);
        t.height    = units(doc_.texts().height(slot));
        t.text      = std::string(doc_.texts().text(slot));
        if (xs.size() >= 2) {
            const Point2 end{xs.back(), ys.back()};
            t.angle = dxf::degrees_from_udeg(core::atan2_udeg(end.y - at.y, end.x - at.x));
        }
        switch (doc_.texts().anchor(slot)) {
        case core::TextAnchor::BaselineLeft:
            t.alignH = DRW_Text::HLeft;
            t.alignV = DRW_Text::VBaseLine;
            break;
        case core::TextAnchor::BaselineCentre:
            t.alignH   = DRW_Text::HCenter;
            t.alignV   = DRW_Text::VBaseLine;
            t.secPoint = t.basePoint;
            break;
        case core::TextAnchor::BaselineRight:
            t.alignH   = DRW_Text::HRight;
            t.alignV   = DRW_Text::VBaseLine;
            t.secPoint = t.basePoint;
            break;
        case core::TextAnchor::MiddleCentre:
            t.alignH   = DRW_Text::HCenter;
            t.alignV   = DRW_Text::VMiddle;
            t.secPoint = t.basePoint;
            break;
        }
        out_.writeText(&t);
        remember_xdata(t);
        ++report_.entities;
    }

    const core::Document& doc_;
    dxfRW& out_;
    core::DrawingUnit unit_;
    DRW::Version version_;
    std::stop_token stop_;
    DxfReport report_;
    std::uint64_t unknown_{0};
    std::uint64_t with_attributes_{0};
    bool cancelled_{false};
    std::vector<std::pair<unsigned, std::vector<std::shared_ptr<DRW_Variant>>>> pending_xdata_;
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

/// Splices the remembered XDATA into the written file: for every entity whose
/// handle group `5` matches, the groups go in just before the next `0` group —
/// the end of that entity's record. One streaming pass over the ASCII file,
/// written beside it and renamed over it, so a failure leaves the original.
core::Status splice_xdata(
    const std::string& path,
    const std::vector<std::pair<unsigned, std::vector<std::shared_ptr<DRW_Variant>>>>& pending)
{
    if (pending.empty()) return core::ok();
    std::map<std::string, const std::vector<std::shared_ptr<DRW_Variant>>*> by_handle;
    for (const auto& [handle, items] : pending) {
        char hex[24];
        (void)std::snprintf(hex, sizeof(hex), "%X", handle);
        by_handle[hex] = &items;
    }

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
    const auto flush                                         = [&]() {
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
            if (value == "SECTION") in_entities = false;
        } else if (code == "2" && !in_entities && value == "ENTITIES") {
            in_entities = true;
        } else if (code == "5" && in_entities && current == nullptr) {
            if (const auto at = by_handle.find(value); at != by_handle.end()) current = at->second;
        }
        out << code_line << '\n' << value_line << '\n';
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
                                                  std::stop_token stop)
{
    if (options.crs.empty())
        co_return err(ErrorCode::ValidationFailed,
                      "Çizimin koordinat sistemi yok; koordinat sistemi olmayan bir dışa aktarım "
                      "eksik veridir. AYAR koordinat_sistemi ile kurun.");

    const DRW::Version ver = dxf::drw_version_for_year(dxf_version_year(version));
    dxfRW writer(path.c_str());
    DxfSource source(doc, writer, options.unit, ver, stop);
    const bool ok = writer.write(&source, ver, /*binary=*/false);
    if (source.cancelled()) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        co_return err(ErrorCode::Cancelled, "Dışa aktarma durduruldu; dosya yazılmadı.");
    }
    if (!ok)
        co_return err(ErrorCode::IoFailure,
                      "'" + path + "' yazılamadı (libdxfrw hata kodu " +
                          std::to_string(static_cast<int>(writer.getError())) +
                          "). Dizin izinlerini ve diski denetleyin.");

    // The XDATA the library cannot write, spliced in after the fact.
    if (auto st = splice_xdata(path, source.pending_xdata()); !st) co_return st.error();

    // The coordinate system beside the file, the only place DXF lets it go.
    auto prj = write_prj_sidecar(path, options.crs);
    if (!prj) {
        if (prj.error().code == ErrorCode::Unsupported)
            source.report().diagnostics.note(Severity::Warning,
                                             ".prj yan dosyası bu yapıda yazılamadı (GDAL kapalı); "
                                             "koordinat sistemi dosyanın yanında değil.");
        else
            co_return prj.error();
    }

    DxfReport report = std::move(source.report());
    report.version   = dxf::acad_name(ver);
    report.crs       = options.crs;
    co_return report;
}

#endif // KENTOS_HAVE_DXFRW

} // namespace kentos::io
