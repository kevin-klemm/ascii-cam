// test_ascii_renderer.cpp - end-to-end pipeline composition + live resize.
#include "test_framework.hpp"
#include "ascii_renderer.hpp"
#include "config.hpp"
#include "output_sink.hpp"

static Frame tinyYuyv(int w, int h, uint8_t y) {
    Frame f; f.format = Frame::Format::YUYV; f.w = w; f.h = h;
    f.data.assign((size_t)w * h * 2, 128);
    for (int i = 0; i < w * h; ++i) f.data[i * 2] = y;   // luma plane
    return f;
}

TEST(renderer, renders_at_requested_grid) {
    Config cfg; cfg.color = false;
    auto r = makeRenderer(cfg, 10, 4);
    StringSink sink;
    auto st = r->render(tinyYuyv(20, 8, 200), sink);
    CHECK_EQ(st.cells, 40u);
    CHECK_EQ(r->cols(), 10);
    CHECK_EQ(r->rows(), 4);
}

TEST(renderer, resize_changes_grid_dimensions) {
    Config cfg; cfg.color = false;
    auto r = makeRenderer(cfg, 10, 4);
    StringSink s1; r->render(tinyYuyv(20, 8, 100), s1);

    r->resize(16, 6);                        // simulate a window resize
    CHECK_EQ(r->cols(), 16);
    CHECK_EQ(r->rows(), 6);
    StringSink s2;
    auto st = r->render(tinyYuyv(20, 8, 100), s2);
    CHECK_EQ(st.cells, 96u);                 // 16*6
}

TEST(renderer, resize_ignores_noop_and_bad_dims) {
    Config cfg; cfg.color = false;
    auto r = makeRenderer(cfg, 10, 4);
    r->resize(10, 4);     // same -> no-op
    r->resize(0, 5);      // invalid -> ignored
    r->resize(-3, 4);     // invalid -> ignored
    CHECK_EQ(r->cols(), 10);
    CHECK_EQ(r->rows(), 4);
}

TEST(renderer, color_path_produces_truecolor) {
    Config cfg; cfg.color = true; cfg.color_mode = "truecolor";
    auto r = makeRenderer(cfg, 8, 3);
    StringSink sink;
    r->render(tinyYuyv(16, 6, 180), sink);
    CHECK(sink.buffer.find("\x1b[38;2;") != std::string::npos);
}
