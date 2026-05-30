// config.hpp - configuration data (Config) and its parser (ConfigLoader).
//
// Splitting the immutable data record from the parsing logic keeps each with
// a single responsibility and lets tests drive the parser from a string with
// no file I/O.
#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>

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
