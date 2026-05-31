// test_tui_control.cpp - the pure key/panel logic of the TUI (no terminal I/O).
#include "test_framework.hpp"
#include "tui_control.hpp"

TEST(tui, contrast_keys_adjust_and_clamp) {
    Config c; c.contrast = 1.0;
    CHECK(applyTuiKey(c, 'C').changed);
    CHECK_NEAR(c.contrast, 1.1, 1e-9);
    CHECK(applyTuiKey(c, 'c').changed);
    CHECK_NEAR(c.contrast, 1.0, 1e-9);
    for (int i = 0; i < 30; ++i) applyTuiKey(c, 'c');   // floors at 0
    CHECK(c.contrast >= 0.0);
    CHECK_NEAR(c.contrast, 0.0, 1e-9);
}

TEST(tui, brightness_keys_clamp_to_range) {
    Config c; c.brightness = 0;
    applyTuiKey(c, 'B');
    CHECK_EQ(c.brightness, 8);
    applyTuiKey(c, 'b'); applyTuiKey(c, 'b');
    CHECK_EQ(c.brightness, -8);
    for (int i = 0; i < 100; ++i) applyTuiKey(c, 'B');
    CHECK_EQ(c.brightness, 255);
}

TEST(tui, toggles_flip_booleans) {
    Config c;
    bool rev = c.reverse, col = c.color, dit = c.dither, edg = c.edge;
    CHECK(applyTuiKey(c, 'r').changed); CHECK(c.reverse == !rev);
    CHECK(applyTuiKey(c, 'o').changed); CHECK(c.color   == !col);
    CHECK(applyTuiKey(c, 'd').changed); CHECK(c.dither  == !dit);
    CHECK(applyTuiKey(c, 'e').changed); CHECK(c.edge    == !edg);
}

TEST(tui, edge_threshold_clamps) {
    Config c; c.edge_threshold = 0;
    applyTuiKey(c, 't');                       // already at floor
    CHECK_EQ(c.edge_threshold, 0);
    applyTuiKey(c, 'T');
    CHECK_EQ(c.edge_threshold, 16);
    for (int i = 0; i < 100; ++i) applyTuiKey(c, 'T');
    CHECK_EQ(c.edge_threshold, 255);
}

TEST(tui, color_mode_cycles_and_wraps) {
    CHECK(colorModeAfter("truecolor") == "256");
    CHECK(colorModeAfter("256") == "16");
    CHECK(colorModeAfter("16") == "mono");
    CHECK(colorModeAfter("mono") == "truecolor");
    CHECK(colorModeAfter("garbage") == "truecolor");
    Config c; c.color_mode = "truecolor";
    applyTuiKey(c, 'm');
    CHECK(c.color_mode == "256");
}

TEST(tui, quit_and_panel_keys_dont_change_render_config) {
    Config c;
    auto q = applyTuiKey(c, 'q');
    CHECK(q.quit); CHECK(!q.changed);
    auto p = applyTuiKey(c, '?');
    CHECK(p.togglePanel); CHECK(!p.changed);
    CHECK(applyTuiKey(c, 'h').togglePanel);
}

TEST(tui, unknown_key_is_inert) {
    Config c;
    auto a = applyTuiKey(c, 'z');
    CHECK(!a.changed); CHECK(!a.quit); CHECK(!a.togglePanel);
}

TEST(tui, panel_reflects_current_values) {
    Config c; c.contrast = 1.10; c.color_mode = "256"; c.edge = true;
    std::string p = tuiPanel(c);
    CHECK(p.find("1.10") != std::string::npos);
    CHECK(p.find("256") != std::string::npos);
    CHECK(p.find("edge") != std::string::npos);
    CHECK(p.find("\x1b[") != std::string::npos);   // contains cursor moves
}
