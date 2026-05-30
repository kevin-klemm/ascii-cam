// ansi_writer.hpp - emit only the cells that changed since the last frame.
//
// This is the inter-frame change analysis + temporal compression: each cell's
// glyph+color is diffed against the previous frame and skipped if identical;
// consecutive changed cells on a row share a single cursor-move. num_[] caches
// the decimal strings used to build truecolor codes (LUT optimization).
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "image.hpp"
#include "palette.hpp"
#include "output_sink.hpp"

class AnsiFrameWriter {
public:
    struct Stats { size_t cells = 0, changed = 0; };

    AnsiFrameWriter(const Palette& pal, bool color) : pal_(pal), color_(color) {
        for (int i = 0; i < 256; ++i) num_[i] = std::to_string(i);
    }

    void resize(int cols, int rows) {
        cols_ = cols; rows_ = rows;
        const size_t n = (size_t)cols * rows;
        prevChar_.assign(n, 0);                 // 0 => force first draw
        prevColor_.assign(n, RGB{255, 0, 255}); // sentinel
        first_ = true;
    }

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    Stats render(const std::vector<uint8_t>& idx, const ColorImage& color,
                 IOutputSink& sink) {
        Stats st; st.cells = (size_t)cols_ * rows_;
        out_.clear();
        int lastRow = -1, lastCol = -2;
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
                    out_ += "\x1b[38;2;";
                    out_ += num_[col.r]; out_ += ';';
                    out_ += num_[col.g]; out_ += ';';
                    out_ += num_[col.b]; out_ += 'm';
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
    const Palette&        pal_;
    bool                  color_;
    int                   cols_ = 0, rows_ = 0;
    std::vector<char>     prevChar_;
    std::vector<RGB>      prevColor_;
    std::string           num_[256];
    std::string           out_;
    bool                  first_ = true;
};
