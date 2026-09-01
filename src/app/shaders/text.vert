// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the SDF text pipeline (`.claude/render.md` R8).
//
// One instance per GLYPH, not per string: every caption on a cadastral sheet
// comes out of one buffer and one draw call, which is what R7's draw-call budget
// needs when a sheet carries thousands of parcel numbers.
//
// THE ROTATION IS PER INSTANCE and comes as a direction vector, not an angle. A
// caption's rotation is the direction of its own baseline — the document stores
// no angle, so there is none that can disagree with the geometry — and a rotation
// applied here costs nothing while a CPU-side rotation would be four multiplies
// per glyph per frame.
#version 440

// Per-vertex: the corner of the unit quad, 0..1 in both axes.
layout(location = 0) in vec2 corner;

// Per-instance.
layout(location = 1) in vec4 local;  // u0, v0, u1, v1 — pixels along and across
                                     // the baseline, v growing DOWNWARD
layout(location = 2) in vec4 place;  // origin.xy in widget pixels, then the
                                     // baseline direction as (cos, sin)
layout(location = 3) in vec4 uv;     // u0, v0, u1, v1 in the atlas
layout(location = 4) in vec4 ink;    // RGBA, already normalised

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vInk;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 colour;
    vec4 params;
} ubuf;

void main()
{
    vec2 at = mix(local.xy, local.zw, corner);

    // Rotate into widget space. `place.zw` is (cos, sin) of the baseline, and the
    // perpendicular is (-sin, cos) — which points DOWN, because widget y does.
    vec2 pos = place.xy + vec2(at.x * place.z - at.y * place.w,
                               at.x * place.w + at.y * place.z);

    vTexCoord = mix(uv.xy, uv.zw, corner);
    vInk      = ink;

    gl_Position = ubuf.mvp * vec4(pos, 0.0, 1.0);
}
