// test_downsampler.cpp - box-average reduction.
#include "test_framework.hpp"
#include "downsampler.hpp"

TEST(downsample, gray_box_average_2x2_to_1x1) {
    GrayImage src(2, 2);
    src.px = {10, 20, 30, 40};   // mean = 25
    GrayImage out = Downsampler().gray(src, 1, 1);
    CHECK_EQ(out.w, 1);
    CHECK_EQ(out.h, 1);
    CHECK_EQ(out.px[0], 25);
}

TEST(downsample, gray_4x4_to_2x2_block_means) {
    GrayImage src(4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            src.at(x, y) = (uint8_t)((x < 2 ? 0 : 100) + (y < 2 ? 0 : 4));
    GrayImage out = Downsampler().gray(src, 2, 2);
    CHECK_EQ(out.at(0, 0), 0);     // top-left block all 0
    CHECK_EQ(out.at(1, 0), 100);   // top-right block all 100
    CHECK_EQ(out.at(0, 1), 4);     // bottom-left all 4
    CHECK_EQ(out.at(1, 1), 104);   // bottom-right all 104
}

TEST(downsample, color_box_average) {
    ColorImage src(2, 1);
    src.px = { RGB{10, 20, 30}, RGB{30, 40, 50} };
    ColorImage out = Downsampler().color(src, 1, 1);
    CHECK_EQ(out.px[0].r, 20);
    CHECK_EQ(out.px[0].g, 30);
    CHECK_EQ(out.px[0].b, 40);
}

TEST(downsample, upscale_dims_are_safe) {
    GrayImage src(2, 2);
    src.px = {1, 2, 3, 4};
    GrayImage out = Downsampler().gray(src, 5, 5);   // outW > srcW
    CHECK_EQ(out.w, 5);
    CHECK_EQ(out.h, 5);
    CHECK_EQ((int)out.px.size(), 25);
}
