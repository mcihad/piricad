// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the line pipeline's fragment stage.
#version 440

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 colour;
    vec4 params;
} ubuf;

void main()
{
    fragColor = ubuf.colour;
}
