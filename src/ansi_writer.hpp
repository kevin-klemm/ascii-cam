// ansi_writer.hpp - emit only the cells that changed since the last frame.
//
// Two layers of compression live here:
//  * inter-frame / temporal: each cell's glyph+color is diffed against the
//    previous frame and skipped if identical; contiguous changed cells on a
//    row share a single cursor-move.
//  * color "pen" coalescing: a color escape is emitted only when the pen
//    actually changes, so runs of same-colored cells cost one escape.
// ColorMode trades fidelity for bytes - critical when piping over SSH:
//    TrueColor  \x1b[38;2;R;G;Bm   ~19 B   16.7M colors
//    Ansi256    \x1b[38;5;Nm       ~11 B   256 colors
//    Ansi16     \x1b[9Xm           ~5  B   16 colors
//    Mono       (none)             0  B
// num_[] caches decimal strings 0..255 (LUT optimization).
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "image.hpp"
#include "palette.hpp"
#include "output_sink.hpp"

enum class ColorMode { Mono, Ansi16, Ansi256, TrueColor };

// RGB -> xterm-256 (6x6x6 cube + 24-step gray ramp).
inline int rgb_to_xterm256(int r, int g, int b) {
    if (std::abs(r - g) < 8 && std::abs(g - b) < 8) {       // near-gray
        if (r < 8)   return 16;
        if (r > 248) return 231;
        return 232 + (r - 8) / 10;
    }
    return 16 + 36 * (r * 5 / 255) + 6 * (g * 5 / 255) + (b * 5 / 255);
}

// RGB -> ANSI 16-color foreground code (30-37 normal, 90-97 bright).
inline int rgb_to_ansi16(int r, int g, int b) {
    int bright = (r > 160 || g > 160 || b > 160) ? 1 : 0;
    int code = (r > 96 ? 1 : 0) | (g > 96 ? 2 : 0) | (b > 96 ? 4 : 0);
    return (bright ? 90 : 30) + code;
}

class AnsiFrameWriter {
public:
    struct Stats { size_t cells = 0, changed = 0; };

    AnsiFrameWriter(const Palette& pal, ColorMode mode)
        : pal_(pal), mode_(mode), color_(mode != ColorMode::Mono) {
        for (int i = 0; i < 256; ++i) num_[i] = std::to_string(i);
    }

    void resize(int cols, int rows) {
        cols_ = cols; rows_ = rows;
        const size_t n = (size_t)cols * rows;
        prevChar_.assign(n, 0);
        prevColor_.assign(n, RGB{255, 0, 255});
        first_ = true;
    }

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    // Drop the diff history so the next render() re-emits every cell. Used to
    // repaint after an overlay (e.g. the TUI panel) occluded part of the frame.
    void forceRedraw() { first_ = true; }

    Stats render(const std::vector<uint8_t>& idx, const ColorImage& color,
                 IOutputSink& sink) {
        Stats st; st.cells = (size_t)cols_ * rows_;
        out_.clear();
        int lastRow = -1, lastCol = -2;
        int penTok = -1; bool penSet = false;
        for (int y = 0; y < rows_; ++y)
            for (int x = 0; x < cols_; ++x) {
                size_t p = (size_t)y * cols_ + x;
                char ch = pal_.glyph(idx[p]);
                RGB col = (color_ && !color.empty()) ? color.at(x, y) : RGB{};
                if (!first_ && ch == prevChar_[p] && col == prevColor_[p])
                    continue;
                ++st.changed;
                if (!(lastRow == y && lastCol == x - 1)) {
                    out_ += "\x1b["; out_ += std::to_string(y + 1);
                    out_ += ';';     out_ += std::to_string(x + 1); out_ += 'H';
                }
                if (color_) {
                    int tok = colorToken(col);
                    if (!penSet || tok != penTok) {
                        appendColor(tok);
                        penTok = tok; penSet = true;
                    }
                }
                out_ += ch;
                prevChar_[p] = ch; prevColor_[p] = col;
                lastRow = y; lastCol = x;
            }
        first_ = false;
        sink.write(out_);
        return st;
    }

private:
    int colorToken(RGB c) const {
        switch (mode_) {
            case ColorMode::TrueColor: return (c.r << 16) | (c.g << 8) | c.b;
            case ColorMode::Ansi256:   return rgb_to_xterm256(c.r, c.g, c.b);
            case ColorMode::Ansi16:    return rgb_to_ansi16(c.r, c.g, c.b);
            default:                   return 0;
        }
    }

    void appendColor(int tok) {
        switch (mode_) {
            case ColorMode::TrueColor:
                out_ += "\x1b[38;2;";
                out_ += num_[(tok >> 16) & 0xFF]; out_ += ';';
                out_ += num_[(tok >> 8) & 0xFF];  out_ += ';';
                out_ += num_[tok & 0xFF];         out_ += 'm';
                break;
            case ColorMode::Ansi256:
                out_ += "\x1b[38;5;"; out_ += num_[tok & 0xFF]; out_ += 'm';
                break;
            case ColorMode::Ansi16:
                out_ += "\x1b["; out_ += num_[tok]; out_ += 'm';
                break;
            default: break;
        }
    }

    const Palette&        pal_;
    ColorMode             mode_;
    bool                  color_;
    int                   cols_ = 0, rows_ = 0;
    std::vector<char>     prevChar_;
    std::vector<RGB>      prevColor_;
    std::string           num_[256];
    std::string           out_;
    bool                  first_ = true;
};
