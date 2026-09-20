// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Snapdragon Game Super Resolution 1 (SGSR1), mobile variant:
// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions
//    and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the following disclaimer in the documentation and/or other materials provided
//    with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to
//    endorse or promote products derived from this software without specific prior written
//    permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Port of Qualcomm's single-pass SGSR1 mobile shader onto Azahar's presentation interface. The
// 12-tap Lanczos-like weighting, edge vote, adaptive sharpening, and clamps are kept exactly as
// published. Only the plumbing differs: the upstream `ViewportInfo` uniform is supplied from this
// renderer's `i_resolution` push constant, and the single `ps0` sampler becomes an indexed lookup
// into the three screen textures. `OperationMode` is fixed to the published RGBA mode, which votes
// and gathers on the green channel.

#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 color;

layout(push_constant, std140) uniform DrawInfo {
    vec4 screen_rect;
    vec4 texcoords;
    vec4 framebuffer_transform;
    vec4 i_resolution;
    vec4 o_resolution;
    int screen_id_l;
    int screen_id_r;
    int layer;
    int reverse_interlaced;
    int orientation;
    float sgsr_sharpness;
};

layout(set = 0, binding = 0) uniform sampler2D screen_textures[3];

vec4 GetScreenLod(int screen_id, vec2 coord) {
#ifdef ARRAY_DYNAMIC_INDEX
    return textureLod(screen_textures[screen_id], coord, 0.0);
#else
    switch (screen_id) {
    case 0:
        return textureLod(screen_textures[0], coord, 0.0);
    case 1:
        return textureLod(screen_textures[1], coord, 0.0);
    case 2:
        return textureLod(screen_textures[2], coord, 0.0);
    }
    return vec4(0.0);
#endif
}

// SGSR gathers the operation-mode channel, which is green for the published RGBA mode. The
// component argument of textureGather must be a constant expression, so it is written literally.
vec4 GatherScreenG(int screen_id, vec2 coord) {
#ifdef ARRAY_DYNAMIC_INDEX
    return textureGather(screen_textures[screen_id], coord, 1);
#else
    switch (screen_id) {
    case 0:
        return textureGather(screen_textures[0], coord, 1);
    case 1:
        return textureGather(screen_textures[1], coord, 1);
    case 2:
        return textureGather(screen_textures[2], coord, 1);
    }
    return vec4(0.0);
#endif
}

float fastLanczos2(float x) {
    float wA = x - 4.0;
    float wB = x * wA - wA;
    wA *= wA;
    return wB * wA;
}

vec2 weightY(float dx, float dy, float c, float std) {
    float x = ((dx * dx) + (dy * dy)) * 0.55 + clamp(abs(c) * std, 0.0, 1.0);
    float w = fastLanczos2(x);
    return vec2(w, w * c);
}

void main() {
    const int mode = 1;
    const float edgeThreshold = 8.0 / 255.0;
    // Qualcomm's reference value is 2.0; the setting scales it so the filter can be
    // softened towards plain upscaling or pushed harder.
    const float edgeSharpness = sgsr_sharpness;

    vec4 result;
    result.xyz = GetScreenLod(screen_id_l, frag_tex_coord).xyz;

    // SGSR is an upscaler. At native size or when the layout downscales the screen there is no
    // reconstruction to do, so presenting directly is both cheaper and closer to the guest image.
    // o_resolution is stored height-first and the guest screen can be rotated into the layout, so
    // compare covered area rather than a single axis.
    if (o_resolution.x * o_resolution.y <= i_resolution.x * i_resolution.y) {
        result.w = 1.0;
        color = result;
        return;
    }

    // ViewportInfo.xy is the source texel size and ViewportInfo.zw the source size in pixels.
    vec4 viewport_info = vec4(i_resolution.zw, i_resolution.xy);

    vec2 imgCoord = ((frag_tex_coord * viewport_info.zw) + vec2(-0.5, 0.5));
    vec2 imgCoordPixel = floor(imgCoord);
    vec2 coord = (imgCoordPixel * viewport_info.xy);
    vec2 pl = (imgCoord + (-imgCoordPixel));
    vec4 left = GatherScreenG(screen_id_l, coord);

    float edgeVote = abs(left.z - left.y) + abs(result[mode] - left.y) + abs(result[mode] - left.z);
    if (edgeVote > edgeThreshold) {
        coord.x += viewport_info.x;

        vec4 right = GatherScreenG(screen_id_l, coord + vec2(viewport_info.x, 0.0));
        vec4 upDown;
        upDown.xy = GatherScreenG(screen_id_l, coord + vec2(0.0, -viewport_info.y)).wz;
        upDown.zw = GatherScreenG(screen_id_l, coord + vec2(0.0, viewport_info.y)).yx;

        float mean = (left.y + left.z + right.x + right.w) * 0.25;
        left = left - vec4(mean);
        right = right - vec4(mean);
        upDown = upDown - vec4(mean);
        result.w = result[mode] - mean;

        float sum = (((((abs(left.x) + abs(left.y)) + abs(left.z)) + abs(left.w)) +
                      (((abs(right.x) + abs(right.y)) + abs(right.z)) + abs(right.w))) +
                     (((abs(upDown.x) + abs(upDown.y)) + abs(upDown.z)) + abs(upDown.w)));
        float std = 2.181818 / sum;

        vec2 aWY = weightY(pl.x, pl.y + 1.0, upDown.x, std);
        aWY += weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, std);
        aWY += weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, std);
        aWY += weightY(pl.x, pl.y - 2.0, upDown.w, std);
        aWY += weightY(pl.x + 1.0, pl.y - 1.0, left.x, std);
        aWY += weightY(pl.x, pl.y - 1.0, left.y, std);
        aWY += weightY(pl.x, pl.y, left.z, std);
        aWY += weightY(pl.x + 1.0, pl.y, left.w, std);
        aWY += weightY(pl.x - 1.0, pl.y - 1.0, right.x, std);
        aWY += weightY(pl.x - 2.0, pl.y - 1.0, right.y, std);
        aWY += weightY(pl.x - 2.0, pl.y, right.z, std);
        aWY += weightY(pl.x - 1.0, pl.y, right.w, std);

        float finalY = aWY.y / aWY.x;

        float maxY = max(max(left.y, left.z), max(right.x, right.w));
        float minY = min(min(left.y, left.z), min(right.x, right.w));
        finalY = clamp(edgeSharpness * finalY, minY, maxY);

        float deltaY = finalY - result.w;

        // smooth high contrast input
        deltaY = clamp(deltaY, -23.0 / 255.0, 23.0 / 255.0);

        result.x = clamp((result.x + deltaY), 0.0, 1.0);
        result.y = clamp((result.y + deltaY), 0.0, 1.0);
        result.z = clamp((result.z + deltaY), 0.0, 1.0);
    }

    result.w = 1.0; // assume alpha channel is not used
    color = result;
}
