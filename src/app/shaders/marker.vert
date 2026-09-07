// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the instanced marker pipeline.
//
// ONE INSTANCE PER STAMP, not one triangle list per stamp. A `nokta-desen-dolgu`
// puts a glyph on every cell of a grid across a face, and a `çizgi-desen` puts one
// every few pixels along a boundary: a single mosque parcel at the drawing's full
// extent asked for two thousand of them, and the CPU was expanding the glyph's
// own geometry — a fan of triangles and a ring of segments — once per stamp, every
// frame. That is 134 784 vertices rebuilt and re-uploaded for three parcels.
//
// The glyph is uploaded ONCE, in its own local pixels, and each stamp becomes
// sixteen bytes: where it goes and which way it faces. This is the same shape the
// text pipeline has had from the beginning — one instance per glyph rather than
// one buffer per string — and the picture pipeline after it.
#version 440

// Per-vertex: the glyph's own geometry, in glyph-local pixels, origin at the
// point the marker is centred on.
layout(location = 0) in vec2 local;

// Per-instance: where the stamp sits in widget pixels, then the direction it
// faces as (cos, sin). SIXTEEN BYTES, and the colour is not among them: one draw
// carries one colour, so a glyph that is filled in one ink and stroked in another
// is two draws over the SAME instance block rather than two copies of it. A DIRECTION and not an angle, for the reason `text.vert`
// gives: the document stores no angle, so there is none that can disagree with
// the geometry, and the rotation costs nothing here.
layout(location = 1) in vec4 place;

// ONE BLOCK, DECLARED IDENTICALLY IN EVERY STAGE OF EVERY PIPELINE.
//
// GLSL links a program from two stages and refuses when the same uniform block
// name carries two different member lists. A field added to one shader and not
// the others fails at link time with `uniform 'ubuf' declared as type 'buf' and
// type 'buf'`.
layout(std140, binding = 0) uniform buf {
    mat4 mvp;      // pixels -> clip, clip-space correction folded in
    vec4 colour;   // straight RGBA
    vec4 params;   // x: half line width · y: SDF range · z: dash period · w: dash count
    vec4 dash[2];  // up to eight mark/space lengths, in pixels
} ubuf;

void main()
{
    // Rotate into widget space. The perpendicular of (cos, sin) is (-sin, cos),
    // which points DOWN because widget y does — the same convention the text and
    // picture stages use, so a glyph, a caption and a picture on one boundary all
    // face the same way.
    vec2 pos = place.xy + vec2(local.x * place.z - local.y * place.w,
                               local.x * place.w + local.y * place.z);

    gl_Position = ubuf.mvp * vec4(pos, 0.0, 1.0);
}
