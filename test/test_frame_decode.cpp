// test_frame_decode.cpp - luma/color extraction per pixel format.
#include "test_framework.hpp"
#include "frame_decode.hpp"

static Frame makeYuyv() {
    Frame f; f.format = Frame::Format::YUYV; f.w = 2; f.h = 1;
    // grey pixels: Y=100 and Y=200, neutral chroma (128).
    f.data = {100, 128, 200, 128};
    return f;
}

TEST(decode, yuyv_luma_is_y_plane) {
    GrayImage g = YuyvLumaExtractor().extract(makeYuyv());
    CHECK_EQ(g.at(0, 0), 100);
    CHECK_EQ(g.at(1, 0), 200);
}

TEST(decode, yuyv_neutral_chroma_is_grey) {
    ColorImage c = YuyvColorExtractor().extract(makeYuyv());
    // neutral chroma -> r==g==b==Y
    CHECK_EQ(c.at(0, 0).r, 100);
    CHECK_EQ(c.at(0, 0).g, 100);
    CHECK_EQ(c.at(0, 0).b, 100);
    CHECK_EQ(c.at(1, 0).r, 200);
}

TEST(decode, bgr_luma_weights) {
    Frame f; f.format = Frame::Format::BGR; f.w = 1; f.h = 1;
    f.data = {0, 0, 255};   // pure red (B,G,R)
    GrayImage g = BgrLumaExtractor().extract(f);
    CHECK_NEAR(g.at(0, 0), 76, 2);   // 0.299*255 ~= 76
}

TEST(decode, bgr_color_swizzles_to_rgb) {
    Frame f; f.format = Frame::Format::BGR; f.w = 1; f.h = 1;
    f.data = {10, 20, 30};   // B=10 G=20 R=30
    ColorImage c = BgrColorExtractor().extract(f);
    CHECK_EQ(c.at(0, 0).r, 30);
    CHECK_EQ(c.at(0, 0).g, 20);
    CHECK_EQ(c.at(0, 0).b, 10);
}

TEST(decode, dispatch_by_format) {
    FrameDecoder d;
    GrayImage g = d.luma(makeYuyv());
    CHECK_EQ(g.at(0, 0), 100);
    Frame bgr; bgr.format = Frame::Format::BGR; bgr.w = 1; bgr.h = 1;
    bgr.data = {255, 255, 255};
    CHECK_EQ(d.luma(bgr).at(0, 0), 255);
}
