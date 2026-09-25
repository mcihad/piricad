// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core (private to curve_path.cpp): a piece of a path as the exact
// curve it is, and the numerical questions an ellipse or a spline needs
// answered (TODOS C-01).
//
// EVERY SUM IN A LOCAL FRAME. A piece is evaluated in metres about an origin
// the caller chooses (core.md R3): seven-digit TUREF coordinates squared in a
// double lose the millimetres the result must keep.
//
// NOTHING BUT +, −, ×, ÷ AND √, so three platforms agree (§7.3): the ellipse's
// sine and cosine come from `sin_cos_rad`, the spline from de Boor's convex
// combinations in a fixed order, the quadrature from fixed nodes, and every
// iteration from fixed start points and fixed stopping rules. Why this is
// written here and not taken from a library is said in curve_path.hpp.
#pragma once

#include "kentos_cad/core/curve_path.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace kentos::core::curve {

/// A vector in metres.
struct Vec
{
    double x{0.0}; ///< east
    double y{0.0}; ///< north
};

/// One piece as the function of t ∈ [0, 1] it is: t = 0 at `from`, 1 at `to`
/// — an arc's and an ellipse's fraction of their sweep, a spline's of its
/// knot domain.
class Eval
{
public:
    /// The piece, evaluated about `origin`.
    Eval(const PathPiece& piece, Point2 origin);

    /// The point at `t`, in metres about the origin.
    Vec point(double t) const;

    /// dP/dt at `t`: metres per whole piece.
    Vec tangent(double t) const;

    /// The point at `t` in the drawing, rounded to the millimetre.
    Point2 world(double t) const;

    /// The frame's origin.
    Point2 origin() const noexcept { return origin_; }

private:
    /// A spline's homogeneous point and its derivative in u, at `u`.
    void spline_at(double u, double& x, double& y, double& w, double& dx, double& dy,
                   double& dw) const;

    PathPiece::Kind kind_;
    Point2 origin_;

    Vec a_; ///< Segment: its start
    Vec b_; ///< Segment: its end

    Vec c_;             ///< Arc, Ellipse: the centre
    Vec u_;             ///< Arc, Ellipse: the first axis, from the centre
    Vec v_;             ///< Arc, Ellipse: the second axis, from the centre
    double start_{0.0}; ///< Arc, Ellipse: the parameter at t = 0, radians
    double sweep_{0.0}; ///< Arc, Ellipse: its signed sweep, radians

    int degree_{0};             ///< Spline: its degree
    std::vector<double> knots_; ///< Spline: its knots
    std::vector<double> hx_;    ///< Spline: homogeneous controls w·x, metres
    std::vector<double> hy_;    ///< Spline: w·y
    std::vector<double> hw_;    ///< Spline: w
    std::vector<double> qx_;    ///< Spline: the derivative's homogeneous controls
    std::vector<double> qy_;    ///< Spline: likewise, north
    std::vector<double> qw_;    ///< Spline: likewise, weights
    double u0_{0.0};            ///< Spline: its domain's start
    double u1_{0.0};            ///< Spline: its domain's end
};

/// The parameters and points a piece is sampled at, ends included: a segment
/// at its two ends, an arc or an ellipse at 256 steps a turn, a spline at 32
/// a knot span.
void samples(const PathPiece& piece, const Eval& eval, std::vector<double>& ts,
             std::vector<Vec>& pts);

/// The parameter of `piece` nearest `probe`.
double nearest_t(const PathPiece& piece, Point2 probe);

/// The length of `piece` between `t0` and `t1`, in metres.
double length(const PathPiece& piece, double t0, double t1);

/// The parameter `metres` along `piece` from its start, clamped to it.
double t_at_length(const PathPiece& piece, double metres);

/// The piece's share of twice the area a closed path encloses — ∫(x·y′ − y·x′)
/// dt over it — in square metres about `origin`, by the fixed rule `length`
/// uses. Summed over a closed path's pieces it is twice the signed area,
/// counter-clockwise positive (`path_area`).
double twice_area(const PathPiece& piece, Point2 origin);

/// A spline's control points and definition.
struct SplineParts
{
    std::vector<Point2> controls; ///< its control points
    SplineDef def;                ///< its degree, knots and weights
};

/// The spline cut at knot value `u` (nano units, strictly inside its
/// domain): the curve before it and the curve after, each clamped at the cut
/// and meeting there exactly.
std::pair<SplineParts, SplineParts> split_spline(const SplineParts& whole, std::int64_t u);

/// The same curve with both its ends clamped: the first control point its
/// start and the last its end — what a spline must be to be joined end to
/// end with another.
SplineParts clamped(const SplineParts& whole);

/// The spline walked the other way: its controls, knots and weights reversed.
SplineParts reversed_spline(const SplineParts& whole);

/// One place two pieces meet.
struct Hit
{
    double s{0.0};        ///< on the first piece
    double t{0.0};        ///< on the second
    Point2 point{};       ///< the point, rounded to the millimetre
    bool touching{false}; ///< they meet without crossing
};

/// A stretch two pieces share, on the first.
struct Span
{
    double s0{0.0}; ///< where it begins
    double s1{0.0}; ///< where it ends
};

/// Everything two pieces have in common.
struct Meets
{
    std::vector<Hit> hits;      ///< ordered along the first piece
    std::vector<Span> overlaps; ///< shared stretches
    bool unresolved{false};     ///< a candidate the refinement could not settle
};

/// Every meet of two pieces, at least one of them an ellipse or a spline — the
/// chords' candidates refined by Newton's method on the exact curves.
Meets meets(const PathPiece& p, const PathPiece& q);

/// The box the piece's curve occupies, in the drawing.
Box2 bounds(const PathPiece& piece);

} // namespace kentos::core::curve
