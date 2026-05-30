// main.cpp - composition root + producer/consumer loop.
//
// Wiring only: parse args/config, build a source and a renderer, run the
// capture thread (producer) and the render/pace loop (consumer). All real
// work lives in the small, individually-tested components.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <sys/ioctl.h>
#include <unistd.h>

#include "config.hpp"
#include "bounded_queue.hpp"
#include "frame_source.hpp"
#include "source_factory.hpp"
#include "ascii_renderer.hpp"
#include "output_sink.hpp"

using clk = std::chrono::steady_clock;

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_winch{false};
static void on_signal(int) { g_running.store(false); }
static void on_winch(int)  { g_winch.store(true); }

static void restore_terminal() {
    // Reset colors, show the cursor, and leave the alternate screen buffer -
    // this returns the terminal to exactly its pre-launch contents, leaving
    // no ASCII frames behind in the scrollback.
    const char* r = "\x1b[0m\x1b[?25h\x1b[?1049l";
    (void)!::write(STDOUT_FILENO, r, std::strlen(r));
}

static void terminal_size(int& cols, int& rows) {
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
        cols = ws.ws_col; rows = ws.ws_row - 1;
    } else { cols = 100; rows = 40; }
}

static void usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s [config.conf] [source]\n"
        "  source overrides config 'source='. Examples:\n"
        "    0                       webcam 0 (OpenCV, or /dev/video0)\n"
        "    /dev/video0             direct V4L2 (Linux, no OpenCV)\n"
        "    rtsp://cam/stream       RTSP via OpenCV\n"
        "    pipe:yuyv:1280x720      raw YUYV frames on stdin\n", prog);
}

int main(int argc, char** argv) {
    Config cfg;
    std::string cfg_path = "ascii.conf";
    if (argc > 1) {
        if (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
            usage(argv[0]); return 0;
        }
        cfg_path = argv[1];
    }
    ConfigLoader::loadFile(cfg, cfg_path);     // missing file => defaults
    if (argc > 2) cfg.source = argv[2];

    std::unique_ptr<IFrameSource> source;
    try {
        source = makeSource(cfg);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }

    double fps = source->fps();
    if (fps <= 1.0 || fps > 240.0) fps = 30.0;
    const auto frame_dt = std::chrono::duration_cast<clk::duration>(
        std::chrono::duration<double>(1.0 / fps));

    // Auto-fit to the terminal unless the grid is pinned in the config.
    const bool autosize = (cfg.cols <= 0 || cfg.rows <= 0);
    int cols = cfg.cols, rows = cfg.rows;
    if (autosize) terminal_size(cols, rows);

    auto renderer = makeRenderer(cfg, cols, rows);

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    if (autosize) std::signal(SIGWINCH, on_winch);   // follow window resizes
    std::atexit(restore_terminal);                   // belt-and-suspenders cleanup
    // Enter the alternate screen, clear it, hide the cursor.
    { const char* init = "\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l";
      (void)!::write(STDOUT_FILENO, init, std::strlen(init)); }

    // ---- producer ------------------------------------------------------
    BoundedQueue<Frame> queue(2);
    std::thread producer([&] {
        while (g_running.load()) {
            Frame f;
            if (!source->read(f) || f.empty()) break;
            queue.push(std::move(f));
        }
        queue.close();   // EOF: close so the consumer drains, but leave
                         // g_running set (only SIGINT/SIGTERM clears it).
    });

    // ---- consumer ------------------------------------------------------
    // Realtime sources (cameras, live streams) pace themselves: read() blocks
    // at the hardware rate, so we render on arrival and never throttle. Only
    // files / fast pipes need explicit pacing to the target fps.
    const bool pace = !source->isRealtime();
    FdSink sink(STDOUT_FILENO);
    auto next = clk::now();
    auto t_prev = clk::now();
    double emit_fps = 0;
    Frame frame;
    char status[192];

    while (g_running.load() && queue.pop(frame)) {
        // Re-fit on terminal resize before rendering this frame.
        if (g_winch.exchange(false)) {
            int nc, nr; terminal_size(nc, nr);
            if (nc != cols || nr != rows) {
                cols = nc; rows = nr;
                renderer->resize(cols, rows);
                const char* clr = "\x1b[2J";          // wipe stale glyphs
                sink.write(clr, std::strlen(clr));
            }
        }

        auto st = renderer->render(frame, sink);

        auto now = clk::now();
        double inst = 1.0 / std::chrono::duration<double>(now - t_prev).count();
        t_prev = now;
        emit_fps = emit_fps == 0 ? inst : emit_fps * 0.9 + inst * 0.1;

        int len = std::snprintf(status, sizeof(status),
            "\x1b[%d;1H\x1b[0m\x1b[2K[%s/%s] %dx%d  %.1f fps  changed %zu/%zu",
            rows + 1, source->name(), simd_backend(), cols, rows,
            emit_fps, st.changed, st.cells);
        sink.write(status, (size_t)len);

        if (pace) {
            next += frame_dt;
            now = clk::now();
            if (next > now) std::this_thread::sleep_until(next);
            else next = now;
        }
    }

    g_running.store(false);
    queue.close();
    if (producer.joinable()) producer.join();
    restore_terminal();
    return 0;
}
