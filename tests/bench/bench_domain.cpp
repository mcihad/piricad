// SPDX-License-Identifier: GPL-3.0-or-later
// Domain budgets from kentoscad.md §10.1.
#include "benchmark.hpp"

#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/domain/cadastre/topology.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace {

using namespace kentos;

/// A hundred thousand parcels at TUREF/TM30 magnitudes, 316 to a row, each a
/// quadrilateral on a 20 m grid — and each with two corners pulled up to 1,2 m
/// off the grid, so a parcel's box overlaps its neighbours' and the pairwise
/// pass has real work to do on every one of them. Axis-aligned squares only
/// touch along a side, which would measure the check on its easiest case.
/// Deterministic, like every fixture here (kentoscad.md §7.3).
core::Document& parcels_100k()
{
    static core::Document document = [] {
        core::Document doc;
        command::Transaction tx(doc, "kurulum");
        const core::LayerId layer      = tx.ensure_layer("PARSEL");
        constexpr std::size_t kParcels = 100'000;
        constexpr std::size_t kColumns = 316;
        constexpr core::Mm kSide       = 20'000;
        for (std::size_t i = 0; i < kParcels; ++i) {
            const core::Mm x    = 485'000'000 + static_cast<core::Mm>(i % kColumns) * kSide;
            const core::Mm y    = 4'310'000'000 + static_cast<core::Mm>(i / kColumns) * kSide;
            const core::Mm pull = static_cast<core::Mm>((i * 7919) % 7) * 200;
            const std::array<core::Point2, 4> ring{
                core::Point2{x, y},
                core::Point2{x + kSide, y + pull},
                core::Point2{x + kSide, y + kSide},
                core::Point2{x + pull, y + kSide},
            };
            const core::RingGeometry::RingInput input{ring, core::RingRole::Exterior, 0};
            (void)tx.add_area(layer, std::span<const core::RingGeometry::RingInput>(&input, 1));
        }
        (void)tx.release();
        return doc;
    }();
    return document;
}

/// kentoscad.md §10.1: topological validation of 100 000 parcels in 2 s. The
/// whole check TOPOLOJİ runs — both passes over the parcels, the redundancy
/// finder and the line network — over the whole drawing.
void topology_100k(benchmark::State& state)
{
    const core::Document& doc = parcels_100k();
    for (auto _ : state) {
        auto found = domain::cadastre::check_topology(doc, {}, 10);
        benchmark::DoNotOptimize(found);
    }
}

} // namespace

KENTOS_BENCH(topology_validation){bench::Case{
    .id          = "domain.topoloji_100k_parsel",
    .title       = "100k parselde topolojik doğrulama",
    .budget      = 2000.0,
    .unit        = "ms",
    .repetitions = 3,
    .iterations  = 1,
    .body        = &topology_100k,
}};
