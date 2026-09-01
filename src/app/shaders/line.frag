// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the line pipeline's fragment stage: colour, and the dash pattern.
//
// THE DASH IS SHADER-SIDE, which `.claude/render.md` R5 asks for by name: width,
// dash pattern and cap style are instance attributes and uniforms, never
// CPU-emitted outline geometry. A published çizgi tipi is a measured thing — the
// annex prints its mark and its gap — and cutting it on the CPU would mean
// re-emitting every boundary on the sheet whenever the zoom changed.
#version 440

layout(location = 0) in float vAlong;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 colour;
    vec4 params;   // x: half width, z: dash period in px, w: dash entry count
    vec4 dash[2];  // up to eight mark/space lengths, in pixels
} ubuf;

void main()
{
    int count = int(ubuf.params.w + 0.5);

    if (count > 0 && ubuf.params.z > 0.0) {
        float t = mod(vAlong, ubuf.params.z);
        if (t < 0.0) t += ubuf.params.z;

        // MARK FIRST, then space, alternating — the order the catalogue writes
        // them in. Walking the entries rather than assuming two lets a
        // dash-dot-dot line type say what it actually is.
        float acc = 0.0;
        bool ink  = true;
        for (int i = 0; i < 8; ++i) {
            if (i >= count) break;
            float seg = ubuf.dash[i / 4][i % 4];
            if (t < acc + seg) {
                ink = (i % 2 == 0);
                break;
            }
            acc += seg;
        }
        if (!ink) discard;
    }

    fragColor = ubuf.colour;
}
