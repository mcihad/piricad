// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the published-picture pipeline's fragment stage.
//
// MPYY publishes part of its symbology as PICTURES — a hatch for `orman`, a glyph
// for `cami`, a line type for `il sınırı`. They arrive as bytes inside the
// document, are decoded once by `symbol_image.hpp` and reach here as a texture.
//
// The tint carries only the symbol layer's OPACITY, not a colour: a published
// picture is the regulation's own drawing and recolouring it would be answering
// a question the annex already answered.
#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vInk;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D picture;

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
    vec4 texel = texture(picture, vTexCoord);

    // The decoder hands over STRAIGHT alpha for a keyed JPEG and premultiplied
    // for a rendered SVG; Qt's RGBA8888 upload keeps whichever it was given. The
    // blend state below expects straight, so an SVG's premultiplied colour is
    // undone here rather than in a second decode path.
    if (texel.a > 0.0) texel.rgb = min(texel.rgb / texel.a, vec3(1.0));

    fragColor = vec4(texel.rgb, texel.a * vInk.a);
}
