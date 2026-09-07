// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/dwg.hpp"

#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#ifdef KENTOS_HAVE_DWG
// io.md R2: the format library's headers never leave this translation unit.
// LibreDWG is C, and `dwg.h` is a 12 000-line header that defines `restrict`
// and a hundred BITCODE_* macros; letting it into a public header would put all
// of that into every translation unit that includes ours.
#include <dwg.h>
#include <dwg_api.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace kentos::io {
namespace {

using core::ErrorCode;

template<class T> core::Result<T> err(ErrorCode code, std::string message)
{
    return core::err(code, std::move(message));
}

constexpr const char* kErrNoBackend = "io.no_dwg";

} // namespace

bool dwg_backend_available()
{
#ifdef KENTOS_HAVE_DWG
    return true;
#else
    return false;
#endif
}

std::string dwg_backend_status()
{
#ifdef KENTOS_HAVE_DWG
    return {};
#else
    return "Bu yapı LibreDWG ile derlenmedi; DWG okunamaz. "
           "-DKENTOS_WITH_DWG=ON ile yeniden yapılandırın.";
#endif
}

#ifndef KENTOS_HAVE_DWG

command::Task<core::Result<DwgReport>> import_dwg(command::Transaction& tx, std::string path,
                                                  std::string project_crs,
                                                  std::vector<std::string> only,
                                                  std::stop_token stop)
{
    (void)tx;
    (void)path;
    (void)project_crs;
    (void)only;
    (void)stop;
    co_return err<DwgReport>(ErrorCode::Unsupported,
                             std::string(kErrNoBackend) + ": " + dwg_backend_status());
}

#else

namespace {

core::Point2 to_mm(double x, double y)
{
    return core::Point2{core::mm_from_metres(x), core::mm_from_metres(y)};
}

/// The layer an entity sits on, defaulting to `0` exactly as DWG does.
std::string layer_of(const Dwg_Object* obj)
{
    if (obj == nullptr || obj->supertype != DWG_SUPERTYPE_ENTITY || obj->tio.entity == nullptr)
        return "0";

    int error   = 0;
    char* named = ::dwg_ent_get_layer_name(obj->tio.entity, &error);
    if (error != 0 || named == nullptr || *named == 0) return "0";

    std::string out(named);
    // `dwg_ent_get_layer_name` returns a copy on r2007+ and a borrowed pointer
    // otherwise; the API's own tools free it either way when the version is
    // 2007 or newer. Copying first and freeing there is what upstream does.
    if (obj->parent != nullptr && obj->parent->header.version >= R_2007) ::free(named);
    return out;
}

/// A DWG version code as the file declares it, for the report.
std::string version_of(const Dwg_Data& dwg)
{
    const char* name = ::dwg_version_type(dwg.header.version);
    return name != nullptr ? std::string(name) : std::string("bilinmiyor");
}

} // namespace

command::Task<core::Result<DwgReport>> import_dwg(command::Transaction& tx, std::string path,
                                                  std::string project_crs,
                                                  std::vector<std::string> only,
                                                  std::stop_token stop)
{
    // A DWG carries no coordinate system: like DXF it is a drawing format and
    // not a geodetic one. The caller's project CRS stands, and saying so is the
    // command's job rather than this reader's.
    (void)project_crs;

    Dwg_Data dwg;
    std::memset(&dwg, 0, sizeof(dwg));

    DwgReport report;

    // Every layer name the file holds and how many entities each produced,
    // whether or not it was read: the checklist is built from this, so a layer
    // that is skipped still has to appear in it.
    std::map<std::string, std::size_t> census;

    // LibreDWG reports its own diagnosis through the return code's bits: the
    // low ones are severity and anything at or above `DWG_ERR_CRITICAL` means
    // nothing usable came back.
    const int rc = ::dwg_read_file(path.c_str(), &dwg);
    if ((rc & DWG_ERR_CRITICAL) != 0) {
        ::dwg_free(&dwg);
        co_return err<DwgReport>(ErrorCode::IoFailure,
                                 "'" + path +
                                     "' bir DWG olarak okunamadı. Dosya bozuk olabilir ya da "
                                     "bu sürümün desteklemediği bir DWG sürümü olabilir.");
    }

    report.version = version_of(dwg);
    report.objects = static_cast<std::size_t>(dwg.num_objects);
    if (rc != 0)
        report.notes.push_back("LibreDWG dosyayı uyarılarla okudu; bazı nesneler eksik olabilir.");

    // The wizard's tick boxes. Empty means every layer, which is what a bare
    // İÇEAKTAR sends; the match is Turkish-folded (CLAUDE.md 5.6).
    const auto wanted = [&only](const std::string& name) {
        if (only.empty()) return true;
        for (const std::string& pick : only)
            if (core::turkish_iequals(pick, name)) return true;
        return false;
    };

    std::map<std::string, core::LayerId> layers;
    const auto layer_for = [&](const std::string& name) -> core::LayerId {
        census.try_emplace(name, 0);
        if (!wanted(name)) return core::kNoLayer;

        const auto found = layers.find(name);
        if (found != layers.end()) return found->second;

        const core::LayerId made = tx.ensure_layer(name);
        if (made != core::kNoLayer) {
            layers.emplace(name, made);
            ++report.layers;
        }
        return made;
    };

    std::map<std::string, std::size_t> skipped;
    std::vector<core::Point2> points;

    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i) {
        // io.md R15: cancellation answered inside 100 ms, checked on a stride
        // rather than per object so the check is not the cost.
        if ((i % 4096) == 0 && stop.stop_requested()) {
            ::dwg_free(&dwg);
            co_return err<DwgReport>(ErrorCode::Cancelled,
                                     "İçe aktarma iptal edildi; çizim değişmedi.");
        }

        const Dwg_Object* obj = &dwg.object[i];
        if (obj->supertype != DWG_SUPERTYPE_ENTITY || obj->tio.entity == nullptr) continue;

        // PAPER SPACE IS NOT THE DRAWING. A DWG's layouts hold the sheet frame,
        // the title block and the viewports; importing them puts a paper border
        // through the middle of the parcels.
        if (obj->tio.entity->entmode == 1) continue;

        const std::string on       = layer_of(obj);
        const core::LayerId target = layer_for(on);
        if (target == core::kNoLayer) continue; // unticked, or unnameable

        const auto keep = [&](core::Result<core::EntityId> made) {
            if (made) {
                ++report.entities;
                ++census[on];
            }
            return made.ok();
        };

        // One run of vertices into the drawing: a FACE when the file says the run
        // is closed, a polyline otherwise. Shared, because "closed means a parcel"
        // is a property of the run and not of the entity type that carried it —
        // LWPOLYLINE and the old-style POLYLINE both arrive here.
        const auto add_run = [&](bool closed) {
            while (closed && points.size() >= 2 && points.back() == points.front())
                points.pop_back(); // the closing vertex is implied (model.md R10)

            if (closed && points.size() >= 3) {
                const core::RingGeometry::RingInput ring{points, core::RingRole::Exterior, 0};
                return keep(tx.add_area(target, {&ring, 1}));
            }
            if (points.size() >= 2) return keep(tx.add_polyline(target, points));
            return false;
        };

        switch (obj->fixedtype) {
        case DWG_TYPE_LINE: {
            const Dwg_Entity_LINE* e = obj->tio.entity->tio.LINE;
            if (e == nullptr) break;
            const std::array<core::Point2, 2> run{to_mm(e->start.x, e->start.y),
                                                  to_mm(e->end.x, e->end.y)};
            (void)keep(tx.add_polyline(target, run));
            break;
        }

        case DWG_TYPE_LWPOLYLINE: {
            const Dwg_Entity_LWPOLYLINE* e = obj->tio.entity->tio.LWPOLYLINE;
            if (e == nullptr || e->points == nullptr || e->num_points < 2) break;

            points.clear();
            points.reserve(e->num_points);
            for (BITCODE_BL v = 0; v < e->num_points; ++v)
                points.push_back(to_mm(e->points[v].x, e->points[v].y));

            // BIT 512 IS `CLOSED`, and it is the whole reason a DWG parcel is a
            // parcel. Read as an open run it would come in as a line: no fill,
            // no area, nothing for İFRAZ or TEVHİT to work on — the same defect
            // the DXF path had, arriving by a different road.
            (void)add_run((e->flag & 512) != 0);
            break;
        }

        // THE OLD-STYLE POLYLINE, and a real cadastral drawing is full of it.
        // AutoCAD wrote `POLYLINE` for a decade before `LWPOLYLINE` existed and
        // every file exported from that era still carries it — a 48 MB zoning
        // drawing measured here holds 12 013 of them against no LWPOLYLINE at
        // all, which is every parcel boundary in it. Unhandled, they fell to the
        // `default:` below and were reported as an unsupported type: the drawing
        // came in without its parcels.
        //
        // Its vertices are SEPARATE OBJECTS in the file, chained to the polyline
        // by handle and closed off by a `SEQEND`. LibreDWG walks that chain and
        // hands back a flat array, which it calloc'd — hence the `free`.
        case DWG_TYPE_POLYLINE_2D:
        case DWG_TYPE_POLYLINE_3D: {
            const bool flat = obj->fixedtype == DWG_TYPE_POLYLINE_2D;

            int error          = 0;
            const BITCODE_BL n = flat ? ::dwg_object_polyline_2d_get_numpoints(obj, &error)
                                      : ::dwg_object_polyline_3d_get_numpoints(obj, &error);
            if (error != 0 || n < 2) break;

            points.clear();
            points.reserve(n);

            if (flat) {
                dwg_point_2d* run = ::dwg_object_polyline_2d_get_points(obj, &error);
                if (error != 0 || run == nullptr) break;
                for (BITCODE_BL v = 0; v < n; ++v)
                    points.push_back(to_mm(run[v].x, run[v].y));
                ::free(run);
            } else {
                // Z IS DROPPED, deliberately. This is a plan reader: the document
                // is two-dimensional and a height belongs in a surface, not in a
                // parcel boundary (model.md R9).
                dwg_point_3d* run = ::dwg_object_polyline_3d_get_points(obj, &error);
                if (error != 0 || run == nullptr) break;
                for (BITCODE_BL v = 0; v < n; ++v)
                    points.push_back(to_mm(run[v].x, run[v].y));
                ::free(run);
            }

            // BIT 1 THIS TIME, not 512: the old polyline's flag word is its own,
            // and its closed bit is the low one.
            const bool closed = flat ? (obj->tio.entity->tio.POLYLINE_2D->flag & 1) != 0
                                     : (obj->tio.entity->tio.POLYLINE_3D->flag & 1) != 0;
            (void)add_run(closed);
            break;
        }

        // NOT LOSSES. A `VERTEX` belongs to the `POLYLINE` that owns it and a
        // `SEQEND` only marks where the run stops; both were already read, above,
        // as part of their polyline. Counting them as unsupported would have the
        // report announce 162 067 dropped objects for a file that dropped none.
        case DWG_TYPE_VERTEX_2D:
        case DWG_TYPE_VERTEX_3D:
        case DWG_TYPE_VERTEX_MESH:
        case DWG_TYPE_VERTEX_PFACE:
        case DWG_TYPE_VERTEX_PFACE_FACE:
        case DWG_TYPE_SEQEND: break;

        case DWG_TYPE_POINT: {
            const Dwg_Entity_POINT* e = obj->tio.entity->tio.POINT;
            if (e == nullptr) break;
            (void)keep(tx.add_point(target, to_mm(e->x, e->y)));
            break;
        }

        case DWG_TYPE_CIRCLE: {
            const Dwg_Entity_CIRCLE* e = obj->tio.entity->tio.CIRCLE;
            if (e == nullptr || e->radius <= 0.0) break;
            (void)keep(tx.add_circle(target, to_mm(e->center.x, e->center.y),
                                     core::mm_from_metres(e->radius)));
            break;
        }

        case DWG_TYPE_ARC: {
            const Dwg_Entity_ARC* e = obj->tio.entity->tio.ARC;
            if (e == nullptr || e->radius <= 0.0) break;

            // The ends, from the angles DWG stores in radians. A CIRCLE is a
            // circle and an ARC is an arc: neither is tessellated on the way in,
            // which is what the DXF path cannot avoid — GDAL breaks both into
            // line segments before this program ever sees them.
            const core::Point2 centre = to_mm(e->center.x, e->center.y);
            const auto around         = [&](double angle) {
                return core::Point2{centre.x + core::mm_from_metres(e->radius * std::cos(angle)),
                                    centre.y + core::mm_from_metres(e->radius * std::sin(angle))};
            };
            (void)keep(tx.add_arc(target, centre, core::mm_from_metres(e->radius),
                                  around(e->start_angle), around(e->end_angle)));
            break;
        }

        case DWG_TYPE_TEXT: {
            const Dwg_Entity_TEXT* e = obj->tio.entity->tio.TEXT;
            if (e == nullptr || e->text_value == nullptr || *e->text_value == 0) break;

            const core::Mm height =
                e->height > 0.0 ? core::mm_from_metres(e->height) : core::mm_from_metres(1.0);
            const core::Point2 at = to_mm(e->ins_pt.x, e->ins_pt.y);

            // The same 0.6 em advance the METİN command uses: it decides the
            // BOUNDING BOX and not where a glyph lands.
            core::Point2 end = at;
            end.x += (height * 6 * static_cast<core::Mm>(std::strlen(e->text_value))) / 10;

            const std::array<core::Point2, 2> baseline{at, end};
            auto made = tx.add_polyline(target, baseline);
            if (!made) break;
            if (tx.set_text(made.value(), e->text_value, height, core::TextAnchor::BaselineLeft)) {
                ++report.entities;
                ++census[on];
            }
            break;
        }

        default:
            // NAMED, NOT COUNTED AS A LOSS IN SILENCE. R14's coverage report is
            // built from exactly this map: what a real file holds that this
            // reader has no translation for.
            ++skipped[obj->name != nullptr ? obj->name : "?"];
            break;
        }
    }

    ::dwg_free(&dwg);

    // THE WIZARD'S CHECKLIST, which was never filled: `service.cpp` builds the
    // layer tick boxes out of this list, so an empty one meant a DWG offered the
    // user no layers to choose between.
    report.layer_names.assign(census.begin(), census.end());

    report.skipped.assign(skipped.begin(), skipped.end());
    std::sort(report.skipped.begin(), report.skipped.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    if (report.entities == 0)
        co_return err<DwgReport>(ErrorCode::ValidationFailed,
                                 "'" + path +
                                     "' okunabilir geometri içermiyor; çizime hiçbir şey "
                                     "eklenmedi.");

    co_return report;
}

#endif

} // namespace kentos::io
