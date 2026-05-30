// frame_decode.hpp - turn a raw Frame into luma / color images.
//
// One strategy per pixel format (Open/Closed: add a format by adding a class,
// not by editing a switch buried in the renderer). FrameDecoder picks the
// right pair for a frame. All of this is OpenCV-free so it runs and is tested
// on bare buffers.
#pragma once
#include <cstdint>
#include "image.hpp"
#include "simd.hpp"

// ---- interfaces ---------------------------------------------------------
class ILumaExtractor {
public:
    virtual ~ILumaExtractor() = default;
    virtual GrayImage extract(const Frame& f) const = 0;
};

class IColorExtractor {
public:
    virtual ~IColorExtractor() = default;
    virtual ColorImage extract(const Frame& f) const = 0;
};

// ---- helpers ------------------------------------------------------------
inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

// BT.601 (full-range) YUV -> RGB.
inline RGB yuv_to_rgb(int y, int u, int v) {
    int cu = u - 128, cv = v - 128;
    return RGB{
        clamp8(y + ((91881 * cv) >> 16)),                       // 1.402
        clamp8(y - ((22554 * cu) >> 16) - ((46802 * cv) >> 16)),// 0.344, 0.714
        clamp8(y + ((116130 * cu) >> 16))                       // 1.772
    };
}

// ---- YUYV -------------------------------------------------------------
class YuyvLumaExtractor : public ILumaExtractor {
public:
    GrayImage extract(const Frame& f) const override {
        GrayImage g(f.w, f.h);
        yuyv_to_gray(f.data.data(), g.px.data(), (size_t)f.w * f.h);  // SIMD
        return g;
    }
};

class YuyvColorExtractor : public IColorExtractor {
public:
    ColorImage extract(const Frame& f) const override {
        ColorImage c(f.w, f.h);
        const uint8_t* s = f.data.data();
        const size_t pairs = (size_t)f.w * f.h / 2;
        for (size_t i = 0; i < pairs; ++i) {
            int y0 = s[4*i], u = s[4*i+1], y1 = s[4*i+2], v = s[4*i+3];
            c.px[2*i]     = yuv_to_rgb(y0, u, v);
            c.px[2*i + 1] = yuv_to_rgb(y1, u, v);
        }
        return c;
    }
};

// ---- BGR (decoded streams) -------------------------------------------
class BgrLumaExtractor : public ILumaExtractor {
public:
    GrayImage extract(const Frame& f) const override {
        GrayImage g(f.w, f.h);
        const uint8_t* s = f.data.data();
        const size_t n = (size_t)f.w * f.h;
        for (size_t i = 0; i < n; ++i) {
            int b = s[3*i], gr = s[3*i+1], r = s[3*i+2];
            g.px[i] = (uint8_t)((19595*r + 38470*gr + 7471*b) >> 16); // .299/.587/.114
        }
        return g;
    }
};

class BgrColorExtractor : public IColorExtractor {
public:
    ColorImage extract(const Frame& f) const override {
        ColorImage c(f.w, f.h);
        const uint8_t* s = f.data.data();
        const size_t n = (size_t)f.w * f.h;
        for (size_t i = 0; i < n; ++i)
            c.px[i] = RGB{ s[3*i+2], s[3*i+1], s[3*i] };
        return c;
    }
};

// ---- format dispatch --------------------------------------------------
class FrameDecoder {
public:
    GrayImage luma(const Frame& f) const {
        return f.format == Frame::Format::YUYV ? yuyvL_.extract(f)
                                               : bgrL_.extract(f);
    }
    ColorImage color(const Frame& f) const {
        return f.format == Frame::Format::YUYV ? yuyvC_.extract(f)
                                               : bgrC_.extract(f);
    }

    // Downsample-then-convert: average the chroma over each output cell's
    // source block and convert ONCE per cell. Because YUV->RGB is affine,
    // averaging in YUV equals averaging in RGB (modulo clamping), so this is
    // ~38x less conversion work than color() at 640x480 -> 160x50, with the
    // same result. Fills `out` in place to avoid per-frame allocation.
    void colorGrid(const Frame& f, int outW, int outH, ColorImage& out) const {
        if (out.w != outW || out.h != outH) out = ColorImage(outW, outH);
        if (f.format == Frame::Format::YUYV) yuyvGrid(f, outW, outH, out);
        else                                 bgrGrid(f, outW, outH, out);
    }

private:
    template <class Accum>
    static void forBlocks(int W, int H, int outW, int outH, Accum accum) {
        for (int oy = 0; oy < outH; ++oy) {
            int y0 = (int)((long)oy * H / outH);
            int y1 = (int)((long)(oy + 1) * H / outH);
            if (y1 <= y0) y1 = y0 + 1;
            for (int ox = 0; ox < outW; ++ox) {
                int x0 = (int)((long)ox * W / outW);
                int x1 = (int)((long)(ox + 1) * W / outW);
                if (x1 <= x0) x1 = x0 + 1;
                accum(ox, oy, x0, x1, y0, y1);
            }
        }
    }

    static void yuyvGrid(const Frame& f, int outW, int outH, ColorImage& out) {
        const uint8_t* s = f.data.data();
        const int W = f.w, H = f.h;
        forBlocks(W, H, outW, outH,
            [&](int ox, int oy, int x0, int x1, int y0, int y1) {
                long sy = 0, su = 0, sv = 0, cnt = 0;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x) {
                        size_t pix = (size_t)y * W + x, pair = pix >> 1;
                        sy += s[pix * 2]; su += s[pair * 4 + 1]; sv += s[pair * 4 + 3];
                        ++cnt;
                    }
                out.at(ox, oy) = yuv_to_rgb((int)(sy/cnt), (int)(su/cnt), (int)(sv/cnt));
            });
    }

    static void bgrGrid(const Frame& f, int outW, int outH, ColorImage& out) {
        const uint8_t* s = f.data.data();
        const int W = f.w, H = f.h;
        forBlocks(W, H, outW, outH,
            [&](int ox, int oy, int x0, int x1, int y0, int y1) {
                long sb = 0, sg = 0, sr = 0, cnt = 0;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x) {
                        size_t pix = (size_t)y * W + x;
                        sb += s[pix*3]; sg += s[pix*3+1]; sr += s[pix*3+2]; ++cnt;
                    }
                out.at(ox, oy) = RGB{ (uint8_t)(sr/cnt), (uint8_t)(sg/cnt), (uint8_t)(sb/cnt) };
            });
    }

    YuyvLumaExtractor  yuyvL_;
    BgrLumaExtractor   bgrL_;
    YuyvColorExtractor yuyvC_;
    BgrColorExtractor  bgrC_;
};
