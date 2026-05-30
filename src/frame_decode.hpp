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
private:
    YuyvLumaExtractor  yuyvL_;
    BgrLumaExtractor   bgrL_;
    YuyvColorExtractor yuyvC_;
    BgrColorExtractor  bgrC_;
};
