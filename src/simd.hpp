// simd.hpp - SIMD YUYV -> grayscale conversion.
//
// YUYV (a.k.a. YUY2) packs two pixels into four bytes: [Y0 U0 Y1 V0].
// The luma plane we need for ASCII brightness is simply every even byte,
// so the conversion is a byte de-interleave that maps perfectly onto a
// single SIMD instruction (NEON vld2q / SSE2 mask+pack). A scalar fallback
// keeps the program portable.
#pragma once
#include <cstdint>
#include <cstddef>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #include <arm_neon.h>
  #define ASCII_SIMD_NEON 1
#elif defined(__SSE2__) || defined(_M_X64) || defined(__x86_64__)
  #include <emmintrin.h>
  #define ASCII_SIMD_SSE2 1
#endif

// src: npix*2 bytes of YUYV. dst: npix bytes of luma. npix is pixel count.
inline void yuyv_to_gray(const uint8_t* src, uint8_t* dst, size_t npix) {
    size_t i = 0;

#if defined(ASCII_SIMD_NEON)
    // vld2q_u8 de-interleaves 32 input bytes into two lanes of 16:
    //   val[0] = Y0 Y1 Y2 ...  (luma)   val[1] = U0 V0 U1 ... (chroma)
    for (; i + 16 <= npix; i += 16) {
        uint8x16x2_t yc = vld2q_u8(src + i * 2);
        vst1q_u8(dst + i, yc.val[0]);
    }
#elif defined(ASCII_SIMD_SSE2)
    // Keep the low byte of each 16-bit lane (the Y samples), then pack two
    // 8-pixel chunks back down into 16 contiguous luma bytes.
    const __m128i lomask = _mm_set1_epi16(0x00FF);
    for (; i + 16 <= npix; i += 16) {
        __m128i a = _mm_loadu_si128((const __m128i*)(src + i * 2));      // 8 px
        __m128i b = _mm_loadu_si128((const __m128i*)(src + i * 2 + 16)); // 8 px
        a = _mm_and_si128(a, lomask);
        b = _mm_and_si128(b, lomask);
        __m128i y = _mm_packus_epi16(a, b);                             // 16 luma
        _mm_storeu_si128((__m128i*)(dst + i), y);
    }
#endif

    for (; i < npix; ++i) dst[i] = src[i * 2];   // scalar tail / fallback
}

inline const char* simd_backend() {
#if defined(ASCII_SIMD_NEON)
    return "NEON";
#elif defined(ASCII_SIMD_SSE2)
    return "SSE2";
#else
    return "scalar";
#endif
}
