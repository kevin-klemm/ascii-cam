// test_tone_mapper.cpp - direct mapping and Floyd-Steinberg dithering.
#include "test_framework.hpp"
#include "tone_mapper.hpp"
#include "palette.hpp"

TEST(tonemap, direct_maps_through_palette) {
    Palette p(" .:#@", false);
    DirectToneMapper m(p);
    FloatGrid g(2, 1);
    g.v = {0.0f, 255.0f};
    std::vector<uint8_t> idx;
    m.map(g, idx);
    CHECK_EQ(idx.size(), 2u);
    CHECK_EQ(p.glyph(idx[0]), ' ');
    CHECK_EQ(p.glyph(idx[1]), '@');
}

TEST(tonemap, dither_indices_in_range) {
    Palette p(" .:-=+*#%@", false);
    DitherToneMapper m(p);
    FloatGrid g(16, 16);
    for (auto& v : g.v) v = 128.0f;   // flat mid-grey
    std::vector<uint8_t> idx;
    m.map(g, idx);
    for (auto i : idx) CHECK(i < p.size());
}

TEST(tonemap, dither_average_tracks_input) {
    // Floyd-Steinberg conserves average tone: a flat mid input should yield a
    // mean glyph index near the middle of the ramp.
    Palette p(" .:-=+*#%@", false);   // 10 levels
    DitherToneMapper m(p);
    FloatGrid g(32, 32);
    for (auto& v : g.v) v = 127.5f;
    std::vector<uint8_t> idx;
    m.map(g, idx);
    double sum = 0;
    for (auto i : idx) sum += i;
    double mean = sum / idx.size();
    CHECK_NEAR(mean, 4.5, 0.6);       // middle of 0..9
}

TEST(tonemap, dither_respects_reverse) {
    Palette p(" .:#@", true);
    DitherToneMapper m(p);
    FloatGrid g(8, 8);
    for (auto& v : g.v) v = 255.0f;   // brightest
    std::vector<uint8_t> idx;
    m.map(g, idx);
    CHECK_EQ(p.glyph(idx[0]), ' ');   // reverse => brightest is the space
}
