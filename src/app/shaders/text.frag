// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the SDF text pipeline's fragment stage.
//
// THE MEDIAN IS THE WHOLE TRICK. A single-channel distance field rounds every
// corner it has, because one distance cannot describe two edges meeting. Three
// channels carry three edge colourings and their MEDIAN reconstructs the corner —
// which is what keeps the serif of a `T` and the cedilla of a `Ş` sharp at the
// zoom a plan sheet is read at.
#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vInk;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D atlas;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 colour;
    vec4 params;   // y: the distance range in ATLAS pixels
} ubuf;

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msd = texture(atlas, vTexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    // How many SCREEN pixels the field's range covers here. Derived per fragment
    // from the texture-coordinate derivative rather than passed in, so one shader
    // is right for a 8 px ruler division and a 200 px title without either of them
    // telling it which it is.
    vec2 unitRange     = vec2(ubuf.params.y) / vec2(textureSize(atlas, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(vTexCoord);
    float pxRange      = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    float coverage = clamp(pxRange * (sd - 0.5) + 0.5, 0.0, 1.0);

    fragColor = vec4(vInk.rgb, vInk.a * coverage);
}
