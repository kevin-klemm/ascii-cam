// test_filters.cpp - brightness/contrast and Sobel.
#include "test_framework.hpp"
#include "filters.hpp"

TEST(filter, brightness_offsets_and_clamps) {
    FloatGrid g(2, 1);
    g.v = {100.0f, 250.0f};
    BrightnessContrastFilter f(1.0, 50);
    f.apply(g);
    CHECK_NEAR(g.v[0], 150.0f, 1e-3);
    CHECK_NEAR(g.v[1], 255.0f, 1e-3);   // clamped
}

TEST(filter, contrast_pivots_around_128) {
    FloatGrid g(2, 1);
    g.v = {128.0f, 178.0f};
    BrightnessContrastFilter f(2.0, 0);
    f.apply(g);
    CHECK_NEAR(g.v[0], 128.0f, 1e-3);   // pivot unchanged
    CHECK_NEAR(g.v[1], 228.0f, 1e-3);   // (178-128)*2+128
}

TEST(filter, sobel_flat_region_is_zero) {
    FloatGrid g(5, 5);
    for (auto& v : g.v) v = 120.0f;     // uniform
    SobelFilter().apply(g);
    for (auto v : g.v) CHECK_NEAR(v, 0.0f, 1e-3);
}

TEST(filter, sobel_detects_vertical_edge) {
    // left half black, right half white -> strong response at the seam.
    FloatGrid g(6, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 6; ++x)
            g.at(x, y) = (x < 3) ? 0.0f : 255.0f;
    SobelFilter().apply(g);
    CHECK(g.at(2, 1) > 100.0f);   // near the edge
    CHECK(g.at(3, 1) > 100.0f);
    CHECK_NEAR(g.at(0, 1), 0.0f, 1e-3);   // flat far side
}

TEST(filter, sobel_threshold_is_binary) {
    FloatGrid g(6, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 6; ++x)
            g.at(x, y) = (x < 3) ? 0.0f : 255.0f;
    SobelFilter(50).apply(g);
    for (auto v : g.v) CHECK(v == 0.0f || v == 255.0f);
}
