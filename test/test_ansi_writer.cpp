// test_ansi_writer.cpp - inter-frame diff / temporal compression.
#include "test_framework.hpp"
#include "ansi_writer.hpp"
#include "palette.hpp"
#include "output_sink.hpp"

TEST(ansi, first_frame_draws_every_cell) {
    Palette p(" .:#@", false);
    AnsiFrameWriter w(p, /*color=*/false);
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
    AnsiFrameWriter w(p, false);
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
    AnsiFrameWriter w(p, false);
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
    AnsiFrameWriter w(p, /*color=*/true);
    w.resize(1, 1);
    std::vector<uint8_t> idx = {2};
    ColorImage c(1, 1);
    c.at(0, 0) = RGB{12, 34, 56};
    StringSink sink;
    w.render(idx, c, sink);
    CHECK(sink.buffer.find("\x1b[38;2;12;34;56m") != std::string::npos);
}
