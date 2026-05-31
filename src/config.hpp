// config.hpp - configuration data (Config) and its parser (ConfigLoader).
//
// Splitting the immutable data record from the parsing logic keeps each with
// a single responsibility and lets tests drive the parser from a string with
// no file I/O.
#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

// Plain data record. No behaviour beyond holding values + defaults.
struct Config {
    // Capture
    std::string source = "0";     // webcam index ("0") or RTSP/file URL
    int    cap_width  = 640;
    int    cap_height = 480;
    double fps        = 0.0;       // 0 => take from the stream
    // How to negotiate the webcam pixel format (numeric sources only):
    //   auto  - let the driver pick (usually MJPEG; high fps)   [default]
    //   mjpeg - force MJPEG, decode to BGR (high fps)
    //   yuyv  - force raw YUYV -> SIMD luma path (often LOW fps on USB!)
    std::string capture_format = "auto";

    // Output grid (0 => auto-detect from the terminal)
    int    cols = 0;
    int    rows = 0;

    // Adjustments
    double contrast   = 1.0;
    int    brightness = 0;
    bool   reverse    = false;
    bool   color      = true;
    std::string color_mode = "truecolor";  // truecolor | 256 | 16 | mono
    bool   dither     = false;
    bool   edge       = false;
    int    edge_threshold = 0;

    std::string charset = " .:-=+*#%@";   // darkest -> lightest
};

class ConfigLoader {
public:
    // Returns false if the file cannot be opened; otherwise applies every
    // recognised key onto `cfg` (unknown keys are ignored).
    static bool loadFile(Config& cfg, const std::string& path) {
        std::ifstream in(path);
        if (!in) return false;
        std::stringstream ss;
        ss << in.rdbuf();
        loadString(cfg, ss.str());
        return true;
    }

    static void loadString(Config& cfg, const std::string& text) {
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (t.empty() || t[0] == '#') continue;
            size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trim(t.substr(0, eq));
            std::string val = trim(t.substr(eq + 1));
            applyKey(cfg, key, val);
        }
    }

    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    static std::string unquote(const std::string& s) {
        if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') &&
            s.back() == s.front())
            return s.substr(1, s.size() - 2);
        return s;
    }

private:
    static void applyKey(Config& cfg, const std::string& key, const std::string& val) {
        if      (key == "source")     cfg.source     = unquote(val);
        else if (key == "width")      cfg.cap_width  = std::atoi(val.c_str());
        else if (key == "height")     cfg.cap_height = std::atoi(val.c_str());
        else if (key == "fps")        cfg.fps        = std::atof(val.c_str());
        else if (key == "capture_format") cfg.capture_format = unquote(val);
        else if (key == "cols")       cfg.cols       = std::atoi(val.c_str());
        else if (key == "rows")       cfg.rows       = std::atoi(val.c_str());
        else if (key == "contrast")   cfg.contrast   = std::atof(val.c_str());
        else if (key == "brightness") cfg.brightness = std::atoi(val.c_str());
        else if (key == "reverse")    cfg.reverse    = std::atoi(val.c_str()) != 0;
        else if (key == "color")      cfg.color      = std::atoi(val.c_str()) != 0;
        else if (key == "color_mode") cfg.color_mode = unquote(val);
        else if (key == "dither")     cfg.dither     = std::atoi(val.c_str()) != 0;
        else if (key == "edge")       cfg.edge       = std::atoi(val.c_str()) != 0;
        else if (key == "edge_threshold") cfg.edge_threshold = std::atoi(val.c_str());
        else if (key == "charset") {
            std::string c = unquote(val);
            if (!c.empty()) cfg.charset = c;
        }
    }
};

// Persists the live-adjustable settings back to a config file. The write is
// surgical: recognised keys are updated in place - preserving comments, blank
// lines, layout and any trailing comment (and its column) - and only appended
// when absent. Capture/source keys are never rewritten, so a user's hand-tuned
// `source`, resolution, etc. survive verbatim. Mirrors ConfigLoader: the
// string->string transform (serialize) is pure and unit-tested; saveFile is
// the thin I/O wrapper.
class ConfigWriter {
public:
    static bool saveFile(const Config& cfg, const std::string& path) {
        std::string existing;
        {
            std::ifstream in(path);
            if (in) { std::stringstream ss; ss << in.rdbuf(); existing = ss.str(); }
        }
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;
        out << serialize(cfg, existing);
        return out.good();
    }

    // Pure: current file contents + Config -> updated contents.
    static std::string serialize(const Config& cfg, const std::string& existing) {
        const auto kv = adjustable(cfg);
        std::vector<bool> seen(kv.size(), false);

        std::string out;
        std::istringstream in(existing);
        std::string line;
        while (std::getline(in, line)) {
            out += rewriteLine(line, kv, seen);
            out += '\n';
        }
        for (size_t i = 0; i < kv.size(); ++i)
            if (!seen[i]) { out += kv[i].first; out += " = "; out += kv[i].second; out += '\n'; }
        return out;
    }

private:
    // The exact set the TUI can change, in a stable order, serialised to the
    // same textual form the loader expects (and the template uses).
    static std::vector<std::pair<std::string, std::string>> adjustable(const Config& cfg) {
        return {
            {"contrast",       numd(cfg.contrast)},
            {"brightness",     std::to_string(cfg.brightness)},
            {"reverse",        cfg.reverse ? "1" : "0"},
            {"color",          cfg.color   ? "1" : "0"},
            {"color_mode",     cfg.color_mode},
            {"dither",         cfg.dither  ? "1" : "0"},
            {"edge",           cfg.edge    ? "1" : "0"},
            {"edge_threshold", std::to_string(cfg.edge_threshold)},
        };
    }

    static std::string numd(double v) {
        char b[32]; std::snprintf(b, sizeof b, "%g", v); return b;
    }

    static std::string rewriteLine(
            const std::string& line,
            const std::vector<std::pair<std::string, std::string>>& kv,
            std::vector<bool>& seen) {
        std::string t = ConfigLoader::trim(line);
        if (t.empty() || t[0] == '#') return line;          // blank / comment
        size_t eq = line.find('=');
        if (eq == std::string::npos) return line;
        std::string key = ConfigLoader::trim(line.substr(0, eq));
        for (size_t i = 0; i < kv.size(); ++i) {
            if (key != kv[i].first) continue;
            seen[i] = true;
            // Keep everything up to '=' verbatim, drop the old value, keep any
            // trailing comment. The adjustable values never contain '#', so the
            // first '#' reliably starts the comment.
            std::string prefix = line.substr(0, eq + 1);
            std::string rem    = line.substr(eq + 1);
            size_t hash = rem.find('#');
            std::string region  = (hash == std::string::npos) ? rem : rem.substr(0, hash);
            std::string comment = (hash == std::string::npos) ? "" : rem.substr(hash);
            std::string nv = " " + kv[i].second;
            // Pad to the old value's width only to hold a trailing comment's
            // column; with no comment, avoid leaving trailing whitespace.
            if (!comment.empty() && nv.size() < region.size())
                nv.resize(region.size(), ' ');
            return prefix + nv + comment;
        }
        return line;
    }
};
