// SPDX-License-Identifier: GPL-3.0-or-later
// Domain budgets from piricad.md §10.1.
#include "benchmark.hpp"

#include "piricad/command/transaction.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/domain/cadastre/topology.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace {

using namespace piricad;

/// A hundred thousand parcels at TUREF/TM30 magnitudes, 316 to a row, each a
/// quadrilateral on a 20 m grid — and each with two corners pulled up to 1,2 m
/// INTO its cell. Seven distinct shapes repeat under translation; their boxes
/// meet along cell edges, but the parcels have no positive-area overlap. This
/// measures validity, neighbour filtering, redundancy and the network pass;
/// it is not a budget measurement of 100k distinct OCCT boolean operations.
/// Deterministic, like every fixture here (piricad.md §7.3).
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

/// piricad.md §10.1: topological validation of 100 000 parcels in 2 s. The
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

/// The expensive branch separately: 1000 true, differently positioned overlaps
/// in OCCT. No neighbour filtering or translation cache can avoid these calls.
/// Pure kernel values are fixtures, not edits to a Document.
void overlaps_1000(benchmark::State& state)
{
    const auto face = [](core::Mm x, core::Mm y) {
        core::CurvePath path;
        path.closed = true;
        const std::array<core::Point2, 4> ring{
            core::Point2{x, y}, {x + 10'000, y}, {x + 10'000, y + 10'000}, {x, y + 10'000}};
        for (std::size_t i = 0; i < ring.size(); ++i)
            path.pieces.push_back(
                core::PathPiece{.from = ring[i], .to = ring[(i + 1) % ring.size()]});
        return core::KernelFace{std::move(path), {}};
    };
    const auto first = face(485'000'000, 4'310'000'000);
    std::vector<core::KernelFace> neighbours;
    for (core::Mm i = 0; i < 1000; ++i)
        neighbours.push_back(
            face(485'000'000 + 1'000 + i * 7, 4'310'000'000 + 500 + (i * 7919) % 3'000));
    for (auto _ : state) {
        for (const auto& next : neighbours) {
            auto checked = core::kernel_overlap(first, next, 10);
            if (!checked || !checked.value().exceeds_tolerance || checked.value().area <= 0) {
                state.SkipWithError("OpenCASCADE gerçek örtüşmeyi denetleyemedi.");
                return;
            }
            benchmark::DoNotOptimize(checked);
        }
    }
}

/// One native coverage union of 1000 faces: 250 disconnected four-parcel
/// frames, each enclosing 100 m². No translation cache skips native booleans.
void coverage_1000(benchmark::State& state)
{
    const auto face = [](core::Mm x0, core::Mm y0, core::Mm x1, core::Mm y1) {
        core::CurvePath path;
        path.closed = true;
        const std::array<core::Point2, 4> ring{core::Point2{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        for (std::size_t i = 0; i < ring.size(); ++i)
            path.pieces.push_back(
                core::PathPiece{.from = ring[i], .to = ring[(i + 1) % ring.size()]});
        return core::KernelFace{std::move(path), {}};
    };
    std::vector<core::KernelFace> faces;
    for (core::Mm i = 0; i < 250; ++i) {
        const core::Mm x = 485'000'000 + (i % 25) * 50'000;
        const core::Mm y = 4'310'000'000 + (i / 25) * 50'000;
        faces.push_back(face(x, y, x + 30'000, y + 10'000));
        faces.push_back(face(x, y + 10'000, x + 10'000, y + 20'000));
        faces.push_back(face(x + 20'000, y + 10'000, x + 30'000, y + 20'000));
        faces.push_back(face(x, y + 20'000, x + 30'000, y + 30'000));
    }
    for (auto _ : state) {
        auto gaps = core::kernel_coverage_gaps(faces, 10);
        if (!gaps || gaps.value().size() != 250 ||
            std::ranges::any_of(gaps.value(),
                                [](const auto& gap) { return gap.area != 100'000'000; })) {
            state.SkipWithError("OpenCASCADE kapalı kapsama boşluklarını denetleyemedi.");
            return;
        }
        benchmark::DoNotOptimize(gaps);
    }
}

} // namespace

PIRICAD_BENCH(topology_validation){bench::Case{
    .id          = "domain.topoloji_100k_parsel",
    .title       = "100k parselde topolojik doğrulama",
    .budget      = 2000.0,
    .unit        = "ms",
    .repetitions = 3,
    .iterations  = 1,
    .body        = &topology_100k,
}};

PIRICAD_BENCH(topology_intersections){bench::Case{
    .id          = "domain.topoloji_1000_ortusme",
    .title       = "OpenCASCADE: 1000 gerçek örtüşme",
    .unit        = "ms",
    .repetitions = 3,
    .iterations  = 1,
    .body        = &overlaps_1000,
}};

PIRICAD_BENCH(topology_coverage){bench::Case{
    .id          = "domain.kapsama_1000_alan",
    .title       = "OpenCASCADE: 1000 alanda 250 kapalı boşluk",
    .unit        = "ms",
    .repetitions = 3,
    .iterations  = 1,
    .body        = &coverage_1000,
}};
