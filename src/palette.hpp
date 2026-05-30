// palette.hpp - the ASCII ramp plus its brightness->index lookup table.
//
// Owns the single concern of "how does a brightness level become a glyph
// index". The precomputed LUT is the cache optimization; `reverse` is baked
// into it so consumers never branch on it.
#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Palette {
public:
    Palette(const std::string& charset, bool reverse) : reverse_(reverse) {
        chars_.assign(charset.begin(), charset.end());
        if (chars_.empty()) chars_ = {' ', '.', ':', '#', '@'};
        buildLut();
    }

    int  size() const { return (int)chars_.size(); }
    char glyph(uint8_t index) const { return chars_[index]; }

    // brightness in [0,255] -> glyph index, reverse applied.
    uint8_t indexForBrightness(int brightness) const {
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;
        return lut_[brightness];
    }

    // level in [0, size()-1] -> glyph index, reverse applied. Used by the
    // dithering mapper which already works in quantised levels.
    uint8_t indexForLevel(int level) const {
        const int n = size();
        if (level < 0) level = 0;
        if (level > n - 1) level = n - 1;
        return (uint8_t)(reverse_ ? (n - 1 - level) : level);
    }

private:
    void buildLut() {
        const int n = size();
        for (int b = 0; b < 256; ++b) {
            int base = b * n / 256;
            if (base > n - 1) base = n - 1;
            lut_[b] = (uint8_t)(reverse_ ? (n - 1 - base) : base);
        }
    }

    std::vector<char> chars_;
    bool              reverse_;
    uint8_t           lut_[256];
};
