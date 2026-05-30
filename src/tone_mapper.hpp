// tone_mapper.hpp - map the continuous-tone grid to charset indices.
//
// Two interchangeable strategies behind one interface (Open/Closed +
// Liskov): a direct LUT mapping and Floyd-Steinberg dithering. Both depend on
// the Palette abstraction rather than a concrete charset.
#pragma once
#include <vector>
#include <cmath>
#include "image.hpp"
#include "palette.hpp"

class IToneMapper {
public:
    virtual ~IToneMapper() = default;
    // Fills idx (size w*h) with a glyph index per cell.
    virtual void map(const FloatGrid& g, std::vector<uint8_t>& idx) const = 0;
};

// Straight brightness -> index through the palette LUT (cache optimization).
class DirectToneMapper : public IToneMapper {
public:
    explicit DirectToneMapper(const Palette& p) : pal_(p) {}
    void map(const FloatGrid& g, std::vector<uint8_t>& idx) const override {
        idx.resize(g.v.size());
        for (size_t i = 0; i < g.v.size(); ++i)
            idx[i] = pal_.indexForBrightness((int)g.v[i]);
    }
private:
    const Palette& pal_;
};

// Floyd-Steinberg error diffusion straight to charset levels.
class DitherToneMapper : public IToneMapper {
public:
    explicit DitherToneMapper(const Palette& p) : pal_(p) {}
    void map(const FloatGrid& gIn, std::vector<uint8_t>& idx) const override {
        const int w = gIn.w, h = gIn.h, n = pal_.size();
        const float step = 255.0f / (n - 1);
        std::vector<float> g = gIn.v;        // mutable working copy
        idx.resize(g.size());
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                size_t p = (size_t)y * w + x;
                int level = (int)std::lround(g[p] / step);
                if (level < 0) level = 0; if (level > n - 1) level = n - 1;
                float err = g[p] - level * step;
                idx[p] = pal_.indexForLevel(level);
                spread(g, w, h, x+1, y,   err * (7.0f/16));
                spread(g, w, h, x-1, y+1, err * (3.0f/16));
                spread(g, w, h, x,   y+1, err * (5.0f/16));
                spread(g, w, h, x+1, y+1, err * (1.0f/16));
            }
    }
private:
    static void spread(std::vector<float>& g, int w, int h, int x, int y, float e) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        float& c = g[(size_t)y * w + x];
        c += e;
        if (c < 0) c = 0; if (c > 255) c = 255;
    }
    const Palette& pal_;
};
