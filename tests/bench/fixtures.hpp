// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — synthetic benchmark fixtures.
//
// A 5M-parcel cadastral layer is far too large to commit, so it is generated
// deterministically: the same code produces the same document on every machine,
// which is what makes a cross-platform comparison meaningful (kentoscad.md §7.3).
#pragma once

#include "kentos_cad/core/document.hpp"

#include <array>
#include <cstdint>

namespace bench {

/// A regular parcel grid in TUREF/TM30, 30th 3-degree zone, near real Turkish
/// coordinates so the numbers exercise the same magnitudes the product will see.
inline void build_cadastral_grid(kentos::core::Document& doc, std::size_t parcels,
                                 std::size_t columns    = 2500,
                                 kentos::core::Mm side = 20000 /* 20 m */)
{
    using namespace kentos::core;

    const LayerId layer = doc.ensure_layer("PARSEL");

    Op op;
    std::array<Point2, 5> ring{};

    for (std::size_t i = 0; i < parcels; ++i) {
        const Mm x = 485000000 + static_cast<Mm>(i % columns) * side;
        const Mm y = 4310000000 + static_cast<Mm>(i / columns) * side;

        ring[0] = {x, y};
        ring[1] = {x + side, y};
        ring[2] = {x + side, y + side};
        ring[3] = {x, y + side};
        ring[4] = {x, y};

        (void)doc.add_polyline(layer, ring, op);
    }
}

/// The five-million-parcel document, built once per process and shared by every
/// scenario that needs it. Building it costs half a second and a gigabyte, so a
/// second copy would measure the allocator rather than the code under test.
inline kentos::core::Document& cadastral_5m()
{
    static kentos::core::Document document = [] {
        kentos::core::Document doc;
        build_cadastral_grid(doc, 5'000'000);
        // Force the lazily-built spatial index HERE, where it belongs to fixture
        // construction. Left to the first query, it lands inside the first timed
        // iteration and is reported as the cost of drawing one frame.
        (void)doc.spatial_index();
        return doc;
    }();
    return document;
}

} // namespace bench
