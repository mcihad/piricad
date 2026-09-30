// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: objects cut off from the drawing's majority (`KAPSAMDENETİM`).
//
// A drawing whose extent is the whole country is nearly always a drawing with a
// handful of objects in the wrong place: a coordinate lost and fallen to 0,0, a
// block imported in another TM zone (the `Suşehri` plan's TM36 against TM39 was
// 250 km), northing and easting swapped. KAPSAM then frames an empty country,
// and Netcad's answer is Shift+Limit Bul, which moves the culprits to a layer
// of its own choosing (wiki.netcad.com.tr 217385147). This finds them and
// changes nothing; what to do with them is the user's decision.
//
// NO LIBRARY FOR IT, because the question is not a clustering one: it is one
// sort by distance and one walk for the first empty band, and the rule is the
// point — it has to be one a surveyor can predict (Article 2.7, stated here).
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstddef>
#include <vector>

namespace kentos::core {

/// The drawing the check reads; core/document.hpp.
class Document;

/// ONE OBJECT CUT OFF from the drawing's majority.
struct DetachedObject
{
    EntityId entity{kNoEntity}; ///< its slot in the document
    Point2 centre{};            ///< the centre of its extent
    double distance_mm{0};      ///< from the majority's centre
};

/// WHAT THE CHECK FOUND (`find_detached`).
struct DetachedReport
{
    std::size_t checked{0}; ///< objects with an extent, the ones the rule saw
    Point2 centre{};        ///< the majority's centre: the median of the objects' centres
    double radius_mm{0};    ///< how far the majority reaches from it
    std::vector<DetachedObject> detached; ///< nearest first
};

/// The objects that lie beyond an EMPTY BAND at least `factor` times as wide as
/// the majority's radius.
///
/// THE RULE. The centre is the median of the objects' centres, coordinate by
/// coordinate — it stays inside the majority however far the stragglers are.
/// Sorted by distance from it, the nearer half is always the majority. Walking
/// outwards, the majority takes each next object in unless the gap to it is at
/// least `factor` times the majority's radius so far (never less than 100 m, so
/// a parcel and a reference point beside it are not a majority and a straggler);
/// at the first such gap, everything beyond it is detached. A drawing that thins
/// out gradually — a dense centre and sparse outskirts — has no such gap and
/// reports nothing, which is what a distance threshold could not promise.
///
/// Fewer than three objects have no majority, and nothing is detached. Exact on
/// every platform: the medians are integer, and the distances are sums of
/// squares and square roots, which IEEE 754 rounds correctly (§7.3).
DetachedReport find_detached(const Document& doc, int factor);

} // namespace kentos::core
