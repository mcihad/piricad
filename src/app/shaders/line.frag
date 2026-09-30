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
