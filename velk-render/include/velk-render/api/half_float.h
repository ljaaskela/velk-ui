#ifndef VELK_RENDER_API_HALF_FLOAT_H
#define VELK_RENDER_API_HALF_FLOAT_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
// MSVC x86: F16C intrinsics are exposed via <immintrin.h> regardless of
// /arch:. Assume runtime support (essentially universal on Vulkan-capable
// hardware: Ivy Bridge / Bulldozer and newer).
#include <immintrin.h>
#define VELK_HALF_FLOAT_F16C 1
#elif (defined(__clang__) || defined(__GNUC__)) && defined(__F16C__)
// clang/gcc x86: only when the F16C feature is explicitly enabled
// (e.g. -mf16c, -march=haswell).
#include <immintrin.h>
#define VELK_HALF_FLOAT_F16C 1
#elif defined(__ARM_NEON) && defined(__aarch64__)
// AArch64: float16 conversion is in the base ISA.
#include <arm_neon.h>
#define VELK_HALF_FLOAT_NEON 1
#endif

namespace velk {

namespace detail {

/**
 * @brief Scalar IEEE 754 float32 to float16 conversion.
 *
 * Handles normals, denormals, inf, and NaN. Used as the portable fallback
 * when no SIMD path is available, and for the tail of SIMD loops.
 */
inline uint16_t float_to_half_scalar(float value)
{
    uint32_t f;
    std::memcpy(&f, &value, 4);

    uint32_t sign = (f >> 16) & 0x8000u;
    int32_t exponent = static_cast<int32_t>((f >> 23) & 0xFFu) - 127;
    uint32_t mantissa = f & 0x007FFFFFu;

    if (exponent > 15) {
        // Overflow: clamp to half-float max or inf.
        return static_cast<uint16_t>(sign | 0x7C00u);
    }
    if (exponent < -14) {
        // Denormalized or too small.
        if (exponent < -24) {
            return static_cast<uint16_t>(sign);
        }
        mantissa |= 0x00800000u;
        uint32_t shift = static_cast<uint32_t>(-1 - exponent);
        return static_cast<uint16_t>(sign | (mantissa >> (shift + 1)));
    }

    return static_cast<uint16_t>(
        sign |
        (static_cast<uint32_t>(exponent + 15) << 10) |
        (mantissa >> 13));
}

} // namespace detail

/**
 * @brief Converts an array of float32 values to float16 (half) representation.
 *
 * Uses F16C on x86 with F16C support, vcvt_f16_f32 on AArch64 NEON, and a
 * scalar fallback otherwise. Counts not divisible by 4 are handled with a
 * scalar tail.
 *
 * @param src Source array of float32 values.
 * @param dst Destination array of half-float values (uint16_t storage).
 * @param count Number of elements to convert.
 */
inline void floats_to_halves(const float* src, uint16_t* dst, size_t count)
{
    size_t i = 0;
#if VELK_HALF_FLOAT_F16C
    for (; i + 4 <= count; i += 4) {
        __m128 v = _mm_loadu_ps(src + i);
        __m128i h = _mm_cvtps_ph(v, _MM_FROUND_TO_NEAREST_INT);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + i), h);
    }
#elif VELK_HALF_FLOAT_NEON
    for (; i + 4 <= count; i += 4) {
        float32x4_t v = vld1q_f32(src + i);
        float16x4_t h = vcvt_f16_f32(v);
        vst1_u16(dst + i, vreinterpret_u16_f16(h));
    }
#endif
    for (; i < count; ++i) {
        dst[i] = detail::float_to_half_scalar(src[i]);
    }
}

} // namespace velk

#endif // VELK_RENDER_API_HALF_FLOAT_H
