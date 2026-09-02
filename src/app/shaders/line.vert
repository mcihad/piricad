// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — GPU line widening (render.md R5).
//
// A stroke is NOT sent as CPU-built outline geometry. One instance per segment
// carries its two endpoints; this shader expands that segment into a screen-space
// quad whose half-width is a uniform. That is what keeps a 1 px cadastral
// boundary one pixel wide at every zoom without the CPU touching a vertex.
//
// The quad is extended along the segment by half a width at each end, so two
// consecutive segments of a polyline overlap in the corner instead of leaving the
// wedge-shaped notch a plain rectangle leaves. It is the cheap join; a mitre and
// a round cap are the second slice's work.
#version 440

// Per-vertex: the corner of the unit quad.
//   x = position along the segment, 0 at p0 and 1 at p1
//   y = side, -1 left of the direction and +1 right of it
layout(location = 0) in vec2 corner;

// Per-instance: the segment, in widget pixels.
layout(location = 1) in vec2 p0;
layout(location = 2) in vec2 p1;

// Distance from the START OF THE RUN to `p0`, in pixels. A dash pattern is
// measured along the whole polyline, not along each segment: restarting it at
// every vertex is what turns a published kesik çizgi into a row of unequal
// stubs, one per corner.
layout(location = 3) in float along0;

layout(location = 0) out float vAlong;

// ONE BLOCK, DECLARED IDENTICALLY IN EVERY STAGE OF EVERY PIPELINE.
//
// GLSL links a program from two stages and refuses when the same uniform block
// name carries two different member lists. The picture pipeline pairs the text
// vertex stage with its own fragment stage, so a field added to one shader and
// not the others fails at link time with `uniform 'ubuf' declared as type 'buf'
// and type 'buf'` — which is what happened when the dash ladder arrived.
layout(std140, binding = 0) uniform buf {
    mat4 mvp;      // pixels -> clip, clip-space correction folded in
    vec4 colour;   // straight RGBA
    vec4 params;   // x: half line width · y: SDF range · z: dash period · w: dash count
    vec4 dash[2];  // up to eight mark/space lengths, in pixels
} ubuf;

void main()
{
    const float half_width = ubuf.params.x;

    vec2 along = p1 - p0;
    float len  = length(along);

    // A degenerate segment — the same point twice, which a closed run produces at
    // its seam — has no direction. Picking one keeps the quad from collapsing to
    // NaN and draws a dot of the right size, which is what a zero-length stroke
    // means on paper.
    vec2 t = len > 0.0 ? along / len : vec2(1.0, 0.0);
    vec2 n = vec2(-t.y, t.x);

    vec2 centre = mix(p0, p1, corner.x) + t * (corner.x * 2.0 - 1.0) * half_width;
    vec2 pos    = centre + n * corner.y * half_width;

    // Where this corner sits along the run, including the half-width the quad is
    // extended by, so the pattern does not jump at a join.
    vAlong = along0 + corner.x * len + (corner.x * 2.0 - 1.0) * half_width;

    gl_Position = ubuf.mvp * vec4(pos, 0.0, 1.0);
}
