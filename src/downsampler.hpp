// downsampler.hpp - box-average downscale from capture size to the grid.
//
// A self-contained area resampler so the core has zero OpenCV dependency.
// Single responsibility: spatial reduction only.
#pragma once
#include "image.hpp"

class Downsampler {
public:
    GrayImage gray(const GrayImage& src, int outW, int outH) const {
        GrayImage out(outW, outH);
        forEachCell(src.w, src.h, outW, outH,
            [&](int ox, int oy, int x0, int x1, int y0, int y1) {
                unsigned sum = 0, cnt = 0;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x) { sum += src.at(x, y); ++cnt; }
                out.at(ox, oy) = (uint8_t)(cnt ? sum / cnt : 0);
            });
        return out;
    }

    ColorImage color(const ColorImage& src, int outW, int outH) const {
        ColorImage out(outW, outH);
        forEachCell(src.w, src.h, outW, outH,
            [&](int ox, int oy, int x0, int x1, int y0, int y1) {
                unsigned r = 0, g = 0, b = 0, cnt = 0;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x) {
                        const RGB& p = src.at(x, y);
                        r += p.r; g += p.g; b += p.b; ++cnt;
                    }
                if (!cnt) cnt = 1;
                out.at(ox, oy) = RGB{ (uint8_t)(r/cnt), (uint8_t)(g/cnt), (uint8_t)(b/cnt) };
            });
        return out;
    }

private:
    template <class F>
    static void forEachCell(int srcW, int srcH, int outW, int outH, F f) {
        for (int oy = 0; oy < outH; ++oy) {
            int y0 = (int)((long)oy * srcH / outH);
            int y1 = (int)((long)(oy + 1) * srcH / outH);
            if (y1 <= y0) y1 = y0 + 1;
            for (int ox = 0; ox < outW; ++ox) {
                int x0 = (int)((long)ox * srcW / outW);
                int x1 = (int)((long)(ox + 1) * srcW / outW);
                if (x1 <= x0) x1 = x0 + 1;
                f(ox, oy, x0, x1, y0, y1);
            }
        }
    }
};
