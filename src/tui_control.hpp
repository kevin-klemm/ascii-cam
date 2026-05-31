// tui_control.hpp - interactive runtime control of the render Config.
//
// Two cleanly separated concerns:
//   * applyTuiKey() / colorModeAfter() - pure functions that map a keystroke
//     onto a Config. No I/O, so they are unit-tested directly (mirroring the
//     ConfigLoader::applyKey split in config.hpp).
//   * TuiController - the thin terminal layer: it reads keys from /dev/tty in
//     raw mode (independent of stdin, which a pipe: source may own) and paints
//     a settings overlay. Disabled gracefully when there is no controlling
//     terminal, so headless / piped runs are unaffected.
#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "config.hpp"

// Cycle order for the 'm' key: fidelity high -> low -> wrap.
inline std::string colorModeAfter(const std::string& m) {
    if (m == "truecolor") return "256";
    if (m == "256")       return "16";
    if (m == "16")        return "mono";
    if (m == "mono")      return "truecolor";
    return "truecolor";   // normalise any unexpected value
}

// Result of feeding one key to the controller.
struct TuiAction {
    bool changed     = false;   // a render-affecting Config field changed
    bool quit        = false;   // user asked to exit
    bool togglePanel = false;   // show/hide the settings overlay
};

// Apply a single keystroke to cfg. Pure: no terminal, no I/O.
inline TuiAction applyTuiKey(Config& cfg, char k) {
    TuiAction a;
    switch (k) {
        case 'c': cfg.contrast = std::max(0.0, cfg.contrast - 0.1); a.changed = true; break;
        case 'C': cfg.contrast = std::min(8.0, cfg.contrast + 0.1); a.changed = true; break;
        case 'b': cfg.brightness = std::max(-255, cfg.brightness - 8); a.changed = true; break;
        case 'B': cfg.brightness = std::min( 255, cfg.brightness + 8); a.changed = true; break;
        case 'r': cfg.reverse = !cfg.reverse; a.changed = true; break;
        case 'o': cfg.color   = !cfg.color;   a.changed = true; break;
        case 'm': cfg.color_mode = colorModeAfter(cfg.color_mode); a.changed = true; break;
        case 'd': cfg.dither = !cfg.dither;   a.changed = true; break;
        case 'e': cfg.edge   = !cfg.edge;     a.changed = true; break;
        case 't': cfg.edge_threshold = std::max(0,   cfg.edge_threshold - 16); a.changed = true; break;
        case 'T': cfg.edge_threshold = std::min(255, cfg.edge_threshold + 16); a.changed = true; break;
        case '?': case 'h': a.togglePanel = true; break;
        case 'q': a.quit = true; break;
        default: break;
    }
    return a;
}

// Build the overlay panel as ANSI text (cursor-addressed, reverse-video rows so
// it reads over any background). Pure - takes a Config, returns bytes.
inline std::string tuiPanel(const Config& cfg) {
    auto onoff = [](bool b) { return b ? "on" : "off"; };
    char line[64];
    std::vector<std::string> rows;
    rows.push_back(" asciicam controls        [?] hide");
    std::snprintf(line, sizeof line, " c/C contrast   : %.2f", cfg.contrast);   rows.push_back(line);
    std::snprintf(line, sizeof line, " b/B brightness : %d",   cfg.brightness); rows.push_back(line);
    std::snprintf(line, sizeof line, " r   reverse    : %s",   onoff(cfg.reverse)); rows.push_back(line);
    std::snprintf(line, sizeof line, " o   color      : %s",   onoff(cfg.color));   rows.push_back(line);
    std::snprintf(line, sizeof line, " m   color_mode : %s",   cfg.color_mode.c_str()); rows.push_back(line);
    std::snprintf(line, sizeof line, " d   dither     : %s",   onoff(cfg.dither)); rows.push_back(line);
    std::snprintf(line, sizeof line, " e   edge       : %s",   onoff(cfg.edge));   rows.push_back(line);
    std::snprintf(line, sizeof line, " t/T edge thresh: %d",   cfg.edge_threshold); rows.push_back(line);
    rows.push_back(" q   quit");

    const int W = 35;
    std::string s = "\x1b[0m";
    for (size_t i = 0; i < rows.size(); ++i) {
        std::string ln = rows[i];
        if ((int)ln.size() > W) ln.resize(W);
        else ln.resize(W, ' ');
        s += "\x1b[";                 // move to row i+1, col 1
        s += std::to_string((int)i + 1);
        s += ";1H\x1b[7m";            // reverse video for legibility
        s += ln;
        s += "\x1b[0m";
    }
    return s;
}

class TuiController {
public:
    struct Poll {
        bool changed        = false;  // caller should reconfigure() the renderer
        bool redraw         = false;  // caller should forceRedraw() (panel toggled)
        bool panelDismissed = false;  // panel just went visible -> hidden
        bool quit           = false;
    };

    explicit TuiController(Config& cfg) : cfg_(cfg) {
        // Read keys from the controlling terminal directly - never stdin, which
        // a pipe: source may be feeding raw frames into.
        fd_ = ::open("/dev/tty", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd_ < 0) return;
        if (::tcgetattr(fd_, &saved_) != 0) { ::close(fd_); fd_ = -1; return; }
        termios raw = saved_;
        raw.c_lflag &= ~(ICANON | ECHO);   // unbuffered, no echo; keep ISIG so
        raw.c_cc[VMIN]  = 0;                // Ctrl-C still raises SIGINT
        raw.c_cc[VTIME] = 0;
        ::tcsetattr(fd_, TCSANOW, &raw);
        enabled_ = true;
    }

    ~TuiController() {
        if (fd_ >= 0) {
            if (enabled_) ::tcsetattr(fd_, TCSANOW, &saved_);
            ::close(fd_);
        }
    }

    TuiController(const TuiController&) = delete;
    TuiController& operator=(const TuiController&) = delete;

    bool enabled() const      { return enabled_; }
    bool panelVisible() const { return panel_; }

    // Drain pending keystrokes (non-blocking) and fold them into cfg.
    Poll poll() {
        Poll p;
        if (!enabled_) return p;
        char buf[64];
        ssize_t n;
        while ((n = ::read(fd_, buf, sizeof buf)) > 0) {
            for (ssize_t i = 0; i < n; ++i) {
                char k = buf[i];
                if (k == 0x1b) {                       // ESC...
                    if (i + 1 < n && buf[i + 1] == '[') {
                        i += 2;                        // CSI: swallow the whole
                        while (i < n && !(buf[i] >= '@' && buf[i] <= '~')) ++i;
                        continue;                      // (arrow/function keys)
                    }
                    p.quit = true;                     // bare ESC quits
                    continue;
                }
                TuiAction a = applyTuiKey(cfg_, k);
                if (a.changed) p.changed = true;
                if (a.quit)    p.quit = true;
                if (a.togglePanel) {
                    panel_ = !panel_;
                    p.redraw = true;
                    if (!panel_) p.panelDismissed = true;
                }
            }
        }
        return p;
    }

    // Overlay for the current settings; empty when hidden.
    std::string panel() const { return panel_ ? tuiPanel(cfg_) : std::string(); }

private:
    Config& cfg_;
    int     fd_      = -1;
    bool    enabled_ = false;
    bool    panel_   = false;
    termios saved_{};
};
