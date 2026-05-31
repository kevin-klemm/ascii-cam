// test_config.cpp - ConfigLoader parsing.
#include "test_framework.hpp"
#include "config.hpp"

TEST(config, defaults_when_empty) {
    Config c;
    ConfigLoader::loadString(c, "");
    CHECK_EQ(c.cap_width, 640);
    CHECK(c.color == true);
    CHECK(c.charset == " .:-=+*#%@");
}

TEST(config, parses_values_and_comments) {
    Config c;
    ConfigLoader::loadString(c,
        "# a comment\n"
        "width = 1280\n"
        "height=720\n"
        "contrast = 1.5\n"
        "brightness = -20\n"
        "color = 0\n"
        "edge = 1\n");
    CHECK_EQ(c.cap_width, 1280);
    CHECK_EQ(c.cap_height, 720);
    CHECK_NEAR(c.contrast, 1.5, 1e-9);
    CHECK_EQ(c.brightness, -20);
    CHECK(c.color == false);
    CHECK(c.edge == true);
}

TEST(config, charset_preserves_leading_space_in_quotes) {
    Config c;
    ConfigLoader::loadString(c, "charset = \" .:#@\"\n");
    CHECK(c.charset == " .:#@");
    CHECK_EQ((int)c.charset.size(), 5);
}

TEST(config, source_unquoted) {
    Config c;
    ConfigLoader::loadString(c, "source = 'rtsp://host/stream'\n");
    CHECK(c.source == "rtsp://host/stream");
}

TEST(config, writer_updates_value_in_place) {
    Config c; c.contrast = 1.5; c.edge = true;
    std::string out = ConfigWriter::serialize(c, "contrast = 1.0\nedge = 0\n");
    CHECK(out.find("contrast = 1.5") != std::string::npos);
    CHECK(out.find("edge = 1") != std::string::npos);
    CHECK(out.find("1.0") == std::string::npos);   // old value gone
}

TEST(config, writer_preserves_comments_blanks_and_other_keys) {
    Config c; c.brightness = 12;
    std::string in =
        "# header comment\n"
        "\n"
        "source = rtsp://cam/stream\n"
        "brightness = 0      # offset\n";
    std::string out = ConfigWriter::serialize(c, in);
    CHECK(out.find("# header comment") != std::string::npos);   // comment kept
    CHECK(out.find("source = rtsp://cam/stream") != std::string::npos); // untouched
    CHECK(out.find("brightness = 12") != std::string::npos);    // updated
    CHECK(out.find("# offset") != std::string::npos);           // trailing comment kept
}

TEST(config, writer_appends_missing_keys) {
    Config c; c.color_mode = "16";
    std::string out = ConfigWriter::serialize(c, "source = 0\n");
    CHECK(out.find("source = 0") != std::string::npos);
    CHECK(out.find("color_mode = 16") != std::string::npos);    // appended
    CHECK(out.find("contrast = ") != std::string::npos);        // all adjustable keys present
}

TEST(config, writer_does_not_touch_charset_line) {
    Config c;
    std::string in = "charset = \" .:-=+*#%@\"\n";
    std::string out = ConfigWriter::serialize(c, in);
    CHECK(out.find("charset = \" .:-=+*#%@\"") != std::string::npos);
}

TEST(config, writer_round_trips_through_loader) {
    Config c; c.contrast = 2.0; c.brightness = -16; c.reverse = true;
    c.color = false; c.color_mode = "256"; c.dither = true;
    c.edge = true; c.edge_threshold = 48;
    std::string out = ConfigWriter::serialize(c, "");   // start from nothing
    Config back;
    ConfigLoader::loadString(back, out);
    CHECK_NEAR(back.contrast, 2.0, 1e-9);
    CHECK_EQ(back.brightness, -16);
    CHECK(back.reverse == true);
    CHECK(back.color == false);
    CHECK(back.color_mode == "256");
    CHECK(back.dither == true);
    CHECK(back.edge == true);
    CHECK_EQ(back.edge_threshold, 48);
}
