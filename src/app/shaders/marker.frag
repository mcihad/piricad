// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the instanced marker pipeline's fragment stage.
//
// One draw, one colour — the uniform block's, already straight RGBA and already
// faded by the symbol layer's opacity. A glyph filled in one ink and stroked in
// another is two draws over the same instance block, which costs sixteen bytes
// once rather than twice.
#version 440

layout(location = 0) out vec4 fragColor;

// ONE BLOCK, DECLARED IDENTICALLY IN EVERY STAGE OF EVERY PIPELINE — see
// `marker.vert`.
layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 colour;
    vec4 params;
    vec4 dash[2];
} ubuf;

void main()
{
    fragColor = ubuf.colour;
}
