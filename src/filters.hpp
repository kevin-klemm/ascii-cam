// filters.hpp - composable in-place operations on the continuous-tone grid.
//
// IFilter is the extension point (Open/Closed): the renderer holds an ordered
// list of filters and applies them blindly. Brightness/contrast and Sobel are
// just two implementations; more can be added without touching the renderer.
#pragma once
#include <cmath>
#include <vector>
#include "image.hpp"

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual void apply(FloatGrid& g) const = 0;
};

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// out = (in - 128) * contrast + 128 + brightness, clamped to [0,255].
class BrightnessContrastFilter : public IFilter {
public:
    BrightnessContrastFilter(double contrast, int brightness)
        : contrast_(contrast), brightness_(brightness) {}
    void apply(FloatGrid& g) const override {
        for (float& v : g.v)
            v = clampf((v - 128.0f) * (float)contrast_ + 128.0f + brightness_, 0, 255);
    }
private:
    double contrast_;
    int    brightness_;
};

// Sobel 3x3 kernel convolution; magnitude = |Gx| + |Gy|.
//   Gx = [-1 0 1; -2 0 2; -1 0 1]   Gy = transpose(Gx)
// threshold > 0 turns it into a binary edge map.
class SobelFilter : public IFilter {
public:
    explicit SobelFilter(int threshold = 0) : threshold_(threshold) {}
    void apply(FloatGrid& g) const override {
        const int w = g.w, h = g.h;
        std::vector<float> out((size_t)w * h, 0.0f);
        auto at = [&](int x, int y) -> float {
            x = x < 0 ? 0 : (x >= w ? w - 1 : x);
            y = y < 0 ? 0 : (y >= h ? h - 1 : y);
            return g.v[(size_t)y * w + x];
        };
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                float gx = -at(x-1,y-1) - 2*at(x-1,y) - at(x-1,y+1)
                           + at(x+1,y-1) + 2*at(x+1,y) + at(x+1,y+1);
                float gy = -at(x-1,y-1) - 2*at(x,y-1) - at(x+1,y-1)
                           + at(x-1,y+1) + 2*at(x,y+1) + at(x+1,y+1);
                float m = std::fabs(gx) + std::fabs(gy);
                if (threshold_ > 0) m = (m >= threshold_) ? 255.0f : 0.0f;
                out[(size_t)y * w + x] = clampf(m, 0, 255);
            }
        g.v.swap(out);
    }
private:
    int threshold_;
};
