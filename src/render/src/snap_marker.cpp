// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/snap_marker.hpp"

#include "kentos_cad/core/snap.hpp"

namespace kentos::render {
namespace {

MarkerRun run_of(std::initializer_list<ScreenPointF> points, bool closed)
{
    return MarkerRun{std::vector<ScreenPointF>(points), closed};
}

} // namespace

Marker snap_marker(std::uint32_t mode, float x, float y, float h)
{
    using namespace kentos::core;

    Marker m;
    switch (mode) {
    case SnapEndpoint: // square
        m.runs.push_back(
            run_of({{x - h, y - h}, {x + h, y - h}, {x + h, y + h}, {x - h, y + h}}, true));
        break;

    case SnapMidpoint: // triangle
        m.runs.push_back(run_of({{x, y - h}, {x + h, y + h}, {x - h, y + h}}, true));
        break;

    case SnapCenter: m.ring = h; break;

    case SnapIntersection: // cross
        m.runs.push_back(run_of({{x - h, y - h}, {x + h, y + h}}, false));
        m.runs.push_back(run_of({{x - h, y + h}, {x + h, y - h}}, false));
        break;

    case SnapPerpendicular: // the right-angle mark
        m.runs.push_back(run_of({{x - h, y - h}, {x - h, y + h}, {x + h, y + h}}, false));
        m.runs.push_back(run_of({{x, y + h}, {x, y}, {x - h, y}}, false));
        break;

    case SnapNearest: // bowtie
        m.runs.push_back(
            run_of({{x - h, y - h}, {x + h, y - h}, {x - h, y + h}, {x + h, y + h}}, true));
        break;

    case SnapGrid: // lattice cell with its centre marked
        m.runs.push_back(run_of({{x - h, y}, {x + h, y}}, false));
        m.runs.push_back(run_of({{x, y - h}, {x, y + h}}, false));
        m.runs.push_back(
            run_of({{x - h, y - h}, {x + h, y - h}, {x + h, y + h}, {x - h, y + h}}, true));
        break;

    case SnapNode: // a filled ring: the surveyed monument itself
        m.ring = h * 0.55F;
        m.runs.push_back(run_of({{x - h, y}, {x - h * 0.55F, y}}, false));
        m.runs.push_back(run_of({{x + h * 0.55F, y}, {x + h, y}}, false));
        m.runs.push_back(run_of({{x, y - h}, {x, y - h * 0.55F}}, false));
        m.runs.push_back(run_of({{x, y + h * 0.55F}, {x, y + h}}, false));
        break;

    case SnapInsertion: // a square with its centre marked: where a thing was PUT
        m.runs.push_back(
            run_of({{x - h, y - h}, {x + h, y - h}, {x + h, y + h}, {x - h, y + h}}, true));
        m.runs.push_back(run_of({{x - h * 0.4F, y}, {x + h * 0.4F, y}}, false));
        m.runs.push_back(run_of({{x, y - h * 0.4F}, {x, y + h * 0.4F}}, false));
        break;

    case SnapPolar:
    case SnapOrtho: // diamond: the point is on a locked direction
        m.runs.push_back(run_of({{x, y - h}, {x + h, y}, {x, y + h}, {x - h, y}}, true));
        break;

    // The constructed points get OPEN glyphs — a shape with a gap in it — so a
    // point this engine built is never mistaken at a glance for a corner the
    // drawing actually contains.
    case SnapExtension: // an arrow continuing to the right
        m.runs.push_back(run_of({{x - h, y}, {x + h, y}}, false));
        m.runs.push_back(run_of({{x, y - h * 0.6F}, {x + h, y}, {x, y + h * 0.6F}}, false));
        break;

    case SnapParallel: // the two strokes of the parallel sign
        m.runs.push_back(run_of({{x - h * 0.3F, y - h}, {x - h, y + h}}, false));
        m.runs.push_back(run_of({{x + h, y - h}, {x + h * 0.3F, y + h}}, false));
        break;

    case SnapApparent: // a cross with an open corner
        m.runs.push_back(run_of({{x - h, y - h}, {x + h, y + h}}, false));
        m.runs.push_back(run_of({{x - h, y + h}, {x, y}}, false));
        break;

        // ---- the seven that drew NOTHING until this file existed -----------------

    case SnapCentroid: // a ring with a cross through it: the balance point
        m.ring = h * 0.7F;
        m.runs.push_back(run_of({{x - h, y}, {x + h, y}}, false));
        m.runs.push_back(run_of({{x, y - h}, {x, y + h}}, false));
        break;

    case SnapNormal: // a right-angle mark standing on a surface stroke
        // The surface, and the perpendicular standing on it. Read as "this point
        // is square to that edge", which is the whole of what the lock did and
        // the one thing the user could not see it doing.
        m.runs.push_back(run_of({{x - h, y + h}, {x + h, y + h}}, false));
        m.runs.push_back(run_of({{x, y + h}, {x, y - h}}, false));
        m.runs.push_back(run_of(
            {{x - h * 0.45F, y + h}, {x - h * 0.45F, y + h * 0.45F}, {x, y + h * 0.45F}}, false));
        break;

    case SnapQuadrant: // a quarter arc between two axis ticks
        m.runs.push_back(run_of({{x, y - h}, {x, y - h * 0.4F}}, false));
        m.runs.push_back(run_of({{x + h * 0.4F, y}, {x + h, y}}, false));
        m.runs.push_back(
            run_of({{x, y - h * 0.4F}, {x + h * 0.28F, y - h * 0.28F}, {x + h * 0.4F, y}}, false));
        break;

    case SnapTangent: // a stroke laid along a curve, touching it at one point
        m.runs.push_back(run_of({{x - h, y - h * 0.55F}, {x + h, y - h * 0.55F}}, false));
        m.runs.push_back(run_of({{x - h * 0.8F, y + h},
                                 {x - h * 0.3F, y - h * 0.5F},
                                 {x + h * 0.3F, y - h * 0.5F},
                                 {x + h * 0.8F, y + h}},
                                false));
        break;

    case SnapGuide: // a long dash through the point: a drafting line
        m.runs.push_back(run_of({{x - h, y - h}, {x + h, y + h}}, false));
        m.runs.push_back(
            run_of({{x - h * 0.35F, y + h * 0.35F}, {x + h * 0.35F, y - h * 0.35F}}, false));
        break;

    case SnapTracking: // a small open square with two trails leaving it
        m.runs.push_back(run_of({{x - h * 0.5F, y - h * 0.5F},
                                 {x + h * 0.5F, y - h * 0.5F},
                                 {x + h * 0.5F, y + h * 0.5F},
                                 {x - h * 0.5F, y + h * 0.5F}},
                                true));
        m.runs.push_back(run_of({{x + h * 0.5F, y}, {x + h, y}}, false));
        m.runs.push_back(run_of({{x, y + h * 0.5F}, {x, y + h}}, false));
        break;

    case SnapStep: // a short ruled run: the point was rounded onto the step
        m.runs.push_back(run_of({{x - h, y}, {x + h, y}}, false));
        m.runs.push_back(run_of({{x - h, y - h * 0.5F}, {x - h, y + h * 0.5F}}, false));
        m.runs.push_back(run_of({{x, y - h * 0.5F}, {x, y + h * 0.5F}}, false));
        m.runs.push_back(run_of({{x + h, y - h * 0.5F}, {x + h, y + h * 0.5F}}, false));
        break;

    default:
        // A BIT THIS FILE DOES NOT KNOW still gets a mark. The engine moved the
        // point, so something has to say so; a small open ring is the honest
        // "an aid fired and it is not one of the named ones".
        m.ring = h * 0.5F;
        break;
    }
    return m;
}

} // namespace kentos::render
