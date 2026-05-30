// bench.cpp - measure pipeline cost (ms/frame) and output size (bytes/frame).
//
//   ms/frame   -> the CPU ceiling on FPS
//   bytes/frame-> the bandwidth cost when piping over SSH
//
// Build: c++ -std=c++17 -O3 -march=native -Isrc bench/bench.cpp -o build/bench
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
#include "config.hpp"
#include "ascii_renderer.hpp"
#include "output_sink.hpp"

using clk = std::chrono::steady_clock;

// Counts bytes instead of writing them.
struct CountingSink : IOutputSink {
    size_t bytes = 0;
    void write(const char*, size_t n) override { bytes += n; }
    void flush() override {}
};

static Frame scene(int w, int h, double t) {
    Frame f; f.format = Frame::Format::YUYV; f.w = w; f.h = h;
    f.data.resize((size_t)w * h * 2);
    double cx = w * (0.5 + 0.35 * std::sin(t));
    double cy = h * (0.5 + 0.25 * std::sin(t * 1.7));
    double r  = h * 0.30;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double dx = x - cx, dy = y - cy;
            bool in = dx*dx + dy*dy < r*r;
            int Y = in ? 235 : (40 + 120 * x / w);
            size_t p = (size_t)y * w + x;
            f.data[p*2]     = (uint8_t)Y;
            f.data[p*2 + 1] = (uint8_t)((p & 1) ? (in?200:100) : (in?90:170));
        }
    return f;
}

// Worst case: a scrolling full-field color gradient so *every* cell changes
// every frame (the diff/temporal compression can save nothing).
static Frame churn(int w, int h, int frame) {
    Frame f; f.format = Frame::Format::YUYV; f.w = w; f.h = h;
    f.data.resize((size_t)w * h * 2);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int Y = (x * 7 + y * 5 + frame * 11) & 0xFF;
            size_t p = (size_t)y * w + x;
            f.data[p*2]     = (uint8_t)Y;
            f.data[p*2 + 1] = (uint8_t)(((x + frame) & 1) ? (60 + (y & 127))
                                                          : (180 - (x & 127)));
        }
    return f;
}

enum Motion { STATIC, MOTION, CHURN };

static void run(const char* label, Config cfg, int W, int H, Motion m) {
    const int cols = cfg.cols, rows = cfg.rows;
    auto r = makeRenderer(cfg, cols, rows);

    // Pre-generate frames so we time rendering only, not scene synthesis.
    const int N = 120;
    std::vector<Frame> frames;
    frames.reserve(N);
    for (int i = 0; i < N; ++i)
        frames.push_back(m == CHURN ? churn(W, H, i)
                                    : scene(W, H, m == MOTION ? i * 0.05 : 0.6));

    CountingSink sink;
    // warmup
    for (int i = 0; i < 10; ++i) r->render(frames[i % N], sink);

    sink.bytes = 0;
    auto t0 = clk::now();
    for (int i = 0; i < N; ++i) r->render(frames[i], sink);
    auto t1 = clk::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / N;
    double bpf = (double)sink.bytes / N;
    std::printf("%-34s %6.3f ms/frame  (<=%5.0f fps)   %8.0f bytes/frame  %6.1f MB/s@30fps\n",
                label, ms, 1000.0 / ms, bpf, bpf * 30 / 1e6);
}

int main() {
    const int W = 640, H = 480;
    Config base; base.cols = 160; base.rows = 50;

    std::printf("=== pipeline @ %dx%d capture -> %dx%d grid (8000 cells) ===\n",
                W, H, base.cols, base.rows);

    Config mono = base; mono.color = false;
    Config color = base; color.color = true;
    Config coldit = color; coldit.dither = true;
    Config coledge = color; coledge.edge = true;

    run("mono,   motion", mono,   W, H, MOTION);
    run("color,  motion", color,  W, H, MOTION);
    run("color,  static (diff win)", color, W, H, STATIC);
    run("color+dither, motion", coldit, W, H, MOTION);
    run("color+sobel,  motion", coledge, W, H, MOTION);
    std::printf("--- worst case: every cell changes every frame ---\n");
    run("mono,        churn", mono,  W, H, CHURN);
    Config c256 = color; c256.color_mode = "256";
    Config c16  = color; c16.color_mode  = "16";
    run("truecolor,   churn", color, W, H, CHURN);
    run("256-color,   churn", c256,  W, H, CHURN);
    run("16-color,    churn", c16,   W, H, CHURN);
    return 0;
}
