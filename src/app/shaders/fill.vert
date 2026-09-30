// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the flat-colour pipeline: polygon fills and the stencil cover quad.
//
// Vertices arrive in WIDGET PIXELS, y down, already origin-offset by the scene
// builder (render.md R2): a TUREF easting is seven digits and never reaches a
// float here, because what reaches here is a pixel.
#version 440

layout(location = 0) in vec2 position;

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
    gl_Position = ubuf.mvp * vec4(position, 0.0, 1.0);
}
