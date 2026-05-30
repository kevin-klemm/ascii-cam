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
