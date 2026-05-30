// test_palette.cpp - charset LUT and reverse mapping.
#include "test_framework.hpp"
#include "palette.hpp"

TEST(palette, maps_brightness_extremes) {
    Palette p(" .:#@", false);   // 5 glyphs
    CHECK_EQ(p.glyph(p.indexForBrightness(0)),   ' ');
    CHECK_EQ(p.glyph(p.indexForBrightness(255)), '@');
}

TEST(palette, reverse_inverts) {
    Palette p(" .:#@", true);
    CHECK_EQ(p.glyph(p.indexForBrightness(0)),   '@');
    CHECK_EQ(p.glyph(p.indexForBrightness(255)), ' ');
}

TEST(palette, monotonic_non_decreasing) {
    Palette p(" .:-=+*#%@", false);
    int prev = -1;
    for (int b = 0; b < 256; ++b) {
        int idx = p.indexForBrightness(b);
        CHECK(idx >= prev);
        prev = idx;
    }
    CHECK_EQ(prev, p.size() - 1);
}

TEST(palette, empty_charset_falls_back) {
    Palette p("", false);
    CHECK(p.size() >= 1);
}
