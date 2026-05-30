// image.hpp - plain, dependency-free image value types.
//
// Keeping the domain types free of OpenCV lets every processing stage (and
// its unit tests) work on ordinary std::vector buffers. OpenCV only ever
// appears in the capture adapter (frame_source.hpp) and main.cpp.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

struct RGB {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const RGB& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const RGB& o) const { return !(*this == o); }
};

struct GrayImage {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    GrayImage() = default;
    GrayImage(int W, int H) : w(W), h(H), px((size_t)W * H, 0) {}
    uint8_t  at(int x, int y) const { return px[(size_t)y * w + x]; }
    uint8_t& at(int x, int y)       { return px[(size_t)y * w + x]; }
    bool empty() const { return px.empty(); }
};

struct ColorImage {
    int w = 0, h = 0;
    std::vector<RGB> px;
    ColorImage() = default;
    ColorImage(int W, int H) : w(W), h(H), px((size_t)W * H) {}
    const RGB& at(int x, int y) const { return px[(size_t)y * w + x]; }
    RGB&       at(int x, int y)       { return px[(size_t)y * w + x]; }
    bool empty() const { return px.empty(); }
};

// Continuous-tone working grid used between filters and the tone mapper.
struct FloatGrid {
    int w = 0, h = 0;
    std::vector<float> v;
    FloatGrid() = default;
    FloatGrid(int W, int H) : w(W), h(H), v((size_t)W * H, 0.0f) {}
    float  at(int x, int y) const { return v[(size_t)y * w + x]; }
    float& at(int x, int y)       { return v[(size_t)y * w + x]; }
};

// A raw captured frame, owned, decoupled from OpenCV's cv::Mat.
struct Frame {
    enum class Format { YUYV, BGR };
    Format format = Format::BGR;
    int w = 0, h = 0;
    std::vector<uint8_t> data;
    bool empty() const { return data.empty(); }
};
