// test_ansi_writer.cpp - inter-frame diff / temporal compression.
#include "test_framework.hpp"
#include "ansi_writer.hpp"
#include "palette.hpp"
#include "output_sink.hpp"

TEST(ansi, first_frame_draws_every_cell) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::Mono);
    w.resize(2, 2);
    std::vector<uint8_t> idx = {0, 1, 2, 3};
    ColorImage none;
    StringSink sink;
    auto st = w.render(idx, none, sink);
    CHECK_EQ(st.cells, 4u);
    CHECK_EQ(st.changed, 4u);
    CHECK(!sink.buffer.empty());
}

TEST(ansi, identical_frame_changes_nothing) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::Mono);
    w.resize(2, 2);
    std::vector<uint8_t> idx = {0, 1, 2, 3};
    ColorImage none;
    StringSink s1; w.render(idx, none, s1);
    StringSink s2;
    auto st = w.render(idx, none, s2);   // same frame again
    CHECK_EQ(st.changed, 0u);            // temporal compression
    CHECK(s2.buffer.empty());
}

TEST(ansi, only_changed_cell_is_redrawn) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::Mono);
    w.resize(2, 2);
    std::vector<uint8_t> a = {0, 1, 2, 3};
    ColorImage none;
    StringSink s1; w.render(a, none, s1);
    std::vector<uint8_t> b = {0, 1, 2, 0};   // last cell differs
    StringSink s2;
    auto st = w.render(b, none, s2);
    CHECK_EQ(st.changed, 1u);
    CHECK(s2.buffer.find("\x1b[2;2H") != std::string::npos);  // moved to (2,2)
}

TEST(ansi, color_emits_truecolor_code) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::TrueColor);
    w.resize(1, 1);
    std::vector<uint8_t> idx = {2};
    ColorImage c(1, 1);
    c.at(0, 0) = RGB{12, 34, 56};
    StringSink sink;
    w.render(idx, c, sink);
    CHECK(sink.buffer.find("\x1b[38;2;12;34;56m") != std::string::npos);
}

TEST(ansi, ansi256_emits_palette_index) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::Ansi256);
    w.resize(1, 1);
    std::vector<uint8_t> idx = {2};
    ColorImage c(1, 1);
    c.at(0, 0) = RGB{255, 0, 0};   // pure red -> cube index 196
    StringSink sink;
    w.render(idx, c, sink);
    CHECK(sink.buffer.find("\x1b[38;5;196m") != std::string::npos);
}

TEST(ansi, pen_coalesces_same_color_run) {
    // Two horizontally adjacent cells of identical color emit the color code
    // once, not twice (bandwidth win over SSH).
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::TrueColor);
    w.resize(2, 1);
    std::vector<uint8_t> idx = {2, 3};
    ColorImage c(2, 1);
    c.at(0, 0) = RGB{10, 20, 30};
    c.at(1, 0) = RGB{10, 20, 30};
    StringSink sink;
    w.render(idx, c, sink);
    size_t first = sink.buffer.find("\x1b[38;2;10;20;30m");
    CHECK(first != std::string::npos);
    CHECK(sink.buffer.find("\x1b[38;2;10;20;30m", first + 1) == std::string::npos);
}

TEST(color, xterm256_grayscale_and_cube) {
    CHECK_EQ(rgb_to_xterm256(0, 0, 0), 16);
    CHECK_EQ(rgb_to_xterm256(255, 255, 255), 231);
    CHECK_EQ(rgb_to_xterm256(255, 0, 0), 196);
}

TEST(ansi, resize_refits_grid_and_full_redraws) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, ColorMode::Mono);
    w.resize(2, 2);
    std::vector<uint8_t> a = {0, 1, 2, 3};
    ColorImage none;
    StringSink s1; w.render(a, none, s1);

    w.resize(3, 1);                       // window changed shape
    CHECK_EQ(w.cols(), 3);
    CHECK_EQ(w.rows(), 1);
    std::vector<uint8_t> b = {0, 1, 2};
    StringSink s2;
    auto st = w.render(b, none, s2);
    CHECK_EQ(st.cells, 3u);
    CHECK_EQ(st.changed, 3u);             // every cell redrawn after resize
}

TEST(color, ansi16_primaries) {
    CHECK_EQ(rgb_to_ansi16(0, 0, 0), 30);       // black
    CHECK_EQ(rgb_to_ansi16(200, 0, 0), 91);     // bright red
    CHECK_EQ(rgb_to_ansi16(0, 200, 0), 92);     // bright green
    CHECK_EQ(rgb_to_ansi16(255, 255, 255), 97); // bright white
}
