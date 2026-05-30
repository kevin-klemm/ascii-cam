// test_simd.cpp - YUYV -> grayscale (the SIMD path + scalar tail).
#include "test_framework.hpp"
#include "simd.hpp"
#include <vector>

TEST(simd, extracts_luma_bytes) {
    // 4 pixels: Y = 10,20,30,40 with arbitrary chroma.
    std::vector<uint8_t> yuyv = {10,200, 20,16, 30,99, 40,128};
    std::vector<uint8_t> gray(4, 0);
    yuyv_to_gray(yuyv.data(), gray.data(), 4);
    CHECK_EQ(gray[0], 10);
    CHECK_EQ(gray[1], 20);
    CHECK_EQ(gray[2], 30);
    CHECK_EQ(gray[3], 40);
}

TEST(simd, handles_non_multiple_of_16) {
    // 19 pixels exercises the SIMD body (16) + scalar tail (3).
    const size_t n = 19;
    std::vector<uint8_t> yuyv(n * 2);
    for (size_t i = 0; i < n; ++i) { yuyv[2*i] = (uint8_t)i; yuyv[2*i+1] = 0xAA; }
    std::vector<uint8_t> gray(n, 0);
    yuyv_to_gray(yuyv.data(), gray.data(), n);
    for (size_t i = 0; i < n; ++i) CHECK_EQ(gray[i], (uint8_t)i);
}

TEST(simd, backend_is_named) {
    std::string b = simd_backend();
    CHECK(b == "NEON" || b == "SSE2" || b == "scalar");
}
