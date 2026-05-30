// demo.cpp - drive the real rendering pipeline on a synthetic animated scene
// and print readable ASCII frames (strips cursor escapes, re-wraps to width).
//
// Build: c++ -std=c++17 -O3 -Isrc demo/demo.cpp -o build/demo
#include <cmath>
#include <cstdio>
#include <string>
#include "config.hpp"
#include "ascii_renderer.hpp"
#include "output_sink.hpp"

// A moving disc over a left-to-right luma gradient, packed as YUYV.
static Frame scene(int w, int h, double t, bool colorful) {
    Frame f; f.format = Frame::Format::YUYV; f.w = w; f.h = h;
    f.data.resize((size_t)w * h * 2);
    double cx = w * (0.5 + 0.35 * std::sin(t));
    double cy = h * (0.5 + 0.25 * std::sin(t * 1.7));
    double r  = h * 0.30;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double dx = x - cx, dy = y - cy;
            bool inside = dx*dx + dy*dy < r*r;
            int Y = inside ? 235 : (40 + 120 * x / w);
            int U = 128, V = 128;
            if (colorful) {               // tint the disc + background
                if (inside) { U = 90;  V = 200; }      // warm
                else        { U = 170; V = 100; }      // cool
            }
            size_t p = (size_t)y * w + x;
            f.data[p*2]     = (uint8_t)Y;
            f.data[p*2 + 1] = (uint8_t)((p & 1) ? V : U);
        }
    return f;
}

// Remove ANSI escape sequences and re-wrap to `cols` so a single rendered
// frame becomes a readable picture.
static std::string readable(const std::string& ansi, int cols) {
    std::string glyphs;
    for (size_t i = 0; i < ansi.size(); ++i) {
        if (ansi[i] == '\x1b') {          // skip ESC [ ... <final letter>
            i += 1;
            if (i < ansi.size() && ansi[i] == '[')
                while (i < ansi.size() && !std::isalpha((unsigned char)ansi[i])) ++i;
            continue;
        }
        glyphs += ansi[i];
    }
    std::string out;
    for (size_t i = 0; i < glyphs.size(); ++i) {
        out += glyphs[i];
        if ((int)((i + 1) % cols) == 0) out += '\n';
    }
    return out;
}

static void show(const char* title, const Config& cfg, int W, int H,
                 double t, bool colorScene) {
    const int cols = cfg.cols, rows = cfg.rows;
    auto r = makeRenderer(cfg, cols, rows);
    StringSink sink;
    r->render(scene(W, H, t, colorScene), sink);
    std::printf("\n== %s (%dx%d grid) ==\n%s",
                title, cols, rows, readable(sink.buffer, cols).c_str());
}

int main() {
    const int W = 320, H = 240;

    Config base;
    base.cols = 72; base.rows = 30; base.color = false;

    // A few frames of the animation (grayscale).
    for (int k = 0; k < 3; ++k)
        show(("frame " + std::to_string(k * 5)).c_str(), base, W, H, k * 0.6, false);

    // Sobel edge detection on the same scene.
    Config edge = base; edge.edge = true;
    show("Sobel edge detection", edge, W, H, 0.6, false);

    // Reverse video.
    Config rev = base; rev.reverse = true;
    show("reverse video", rev, W, H, 0.6, false);

    // Floyd-Steinberg dithering.
    Config dit = base; dit.dither = true;
    show("Floyd-Steinberg dither", dit, W, H, 0.6, false);

    // Color: show the truecolor escape codes are emitted (first 220 bytes).
    Config col = base; col.color = true; col.cols = 40; col.rows = 18;
    auto rc = makeRenderer(col, col.cols, col.rows);
    StringSink cs; rc->render(scene(W, H, 0.6, true), cs);
    std::printf("\n== color output (raw ANSI, first 220 bytes) ==\n");
    for (size_t i = 0; i < cs.buffer.size() && i < 220; ++i) {
        unsigned char c = cs.buffer[i];
        if (c == 0x1b) std::printf("\\e");
        else std::printf("%c", c);
    }
    std::printf("\n");
    return 0;
}
