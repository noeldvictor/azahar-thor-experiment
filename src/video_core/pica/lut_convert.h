// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#if defined(__aarch64__) || defined(__ARM_NEON)
#define CITRA_LUT_HAS_NEON
#include <arm_neon.h>
#endif

#include "common/common_types.h"
#include "common/vector_math.h"

namespace Pica::LutConvert {

/**
 * The lighting and fog lookup tables are rebuilt and uploaded whenever the guest rewrites them,
 * which in a lit scene is most frames, and the lighting set alone is 24 tables of 256 entries.
 * Every entry is a packed fixed point pair, so the conversion is a long run of identical work
 * over a contiguous array. The vector paths below divide rather than multiply by a reciprocal,
 * so they produce the same floats as the scalar tails bit for bit. The three PICA tables do not
 * share a layout; keep each one beside the union it reads.
 */

/// Lighting table entry. Bits 0 to 11 are an unsigned 0.0.12 value over 4095. Bits 12 to 22 are
/// an unsigned 0.0.11 difference magnitude over 2047, negated when bit 23 is set. Writes
/// interleaved {value, difference} floats.
inline void LightingEntries(const u32* raw, Common::Vec2f* out, std::size_t count) {
    std::size_t i = 0;
#if defined(CITRA_LUT_HAS_NEON) && defined(__aarch64__)
    const float32x4_t value_divisor = vdupq_n_f32(4095.f);
    const float32x4_t diff_divisor = vdupq_n_f32(2047.f);
    const uint32x4_t value_mask = vdupq_n_u32(0xFFF);
    const uint32x4_t diff_mask = vdupq_n_u32(0x7FF);
    const uint32x4_t sign_bit = vdupq_n_u32(1u << 23);
    for (; i + 4 <= count; i += 4) {
        const uint32x4_t packed = vld1q_u32(raw + i);
        const uint32x4_t value_bits = vandq_u32(packed, value_mask);
        const uint32x4_t diff_bits = vandq_u32(vshrq_n_u32(packed, 12), diff_mask);
        const float32x4_t magnitude = vdivq_f32(vcvtq_f32_u32(diff_bits), diff_divisor);
        const uint32x4_t negate = vtstq_u32(packed, sign_bit);
        float32x4x2_t pair;
        pair.val[0] = vdivq_f32(vcvtq_f32_u32(value_bits), value_divisor);
        pair.val[1] = vbslq_f32(negate, vnegq_f32(magnitude), magnitude);
        vst2q_f32(reinterpret_cast<f32*>(out + i), pair);
    }
#endif
    for (; i < count; ++i) {
        const u32 packed = raw[i];
        const f32 magnitude = static_cast<f32>((packed >> 12) & 0x7FFu) / 2047.f;
        out[i] = {static_cast<f32>(packed & 0xFFFu) / 4095.f,
                  (packed & (1u << 23)) ? -magnitude : magnitude};
    }
}

/// Fog table entry. Bits 0 to 12 are a signed 1.1.11 difference over 2047. Bits 13 to 23 are an
/// unsigned 0.0.11 value over 2047. Writes interleaved {value, difference} floats.
inline void FogEntries(const u32* raw, Common::Vec2f* out, std::size_t count) {
    std::size_t i = 0;
#if defined(CITRA_LUT_HAS_NEON) && defined(__aarch64__)
    const float32x4_t divisor = vdupq_n_f32(2047.f);
    const uint32x4_t value_mask = vdupq_n_u32(0x7FF);
    for (; i + 4 <= count; i += 4) {
        const uint32x4_t packed = vld1q_u32(raw + i);
        const uint32x4_t value_bits = vandq_u32(vshrq_n_u32(packed, 13), value_mask);
        // Sign extend the 13 bit difference: lift bit 12 to the sign bit, then shift back down.
        const int32x4_t diff_bits =
            vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(packed), 19), 19);
        float32x4x2_t pair;
        pair.val[0] = vdivq_f32(vcvtq_f32_u32(value_bits), divisor);
        pair.val[1] = vdivq_f32(vcvtq_f32_s32(diff_bits), divisor);
        vst2q_f32(reinterpret_cast<f32*>(out + i), pair);
    }
#endif
    for (; i < count; ++i) {
        const u32 packed = raw[i];
        const s32 difference = static_cast<s32>(packed << 19) >> 19;
        out[i] = {static_cast<f32>((packed >> 13) & 0x7FFu) / 2047.f,
                  static_cast<f32>(difference) / 2047.f};
    }
}

} // namespace Pica::LutConvert
