// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the flat-colour pipeline: polygon fills and the stencil cover quad.
//
// Vertices arrive in WIDGET PIXELS, y down, already origin-offset by the scene
// builder (render.md R2): a TUREF easting is seven digits and never reaches a
// float here, because what reaches here is a pixel.
#version 440

layout(location = 0) in vec2 position;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;      // pixels -> clip, including QRhi's clip-space correction
    vec4 colour;   // premultiplied straight through to the fragment stage
    vec4 params;   // x: half line width; y,z,w: reserved
} ubuf;

void main()
{
    gl_Position = ubuf.mvp * vec4(position, 0.0, 1.0);
}
