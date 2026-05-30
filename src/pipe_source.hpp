// pipe_source.hpp - read raw frames from a pipe / fd (no OpenCV, no V4L2).
//
// The universal IoT escape hatch: anything that can produce raw frames can
// feed this. Connect it to any encoder/stream via ffmpeg, e.g.
//
//   ffmpeg -i rtsp://cam/stream -f rawvideo -pix_fmt yuyv422 -            \
//     | asciicam --source pipe:yuyv:1280x720
//
// or pull from a hardware H.264 encoder, MJPEG camera, etc. ffmpeg does the
// decode; this binary stays tiny and dependency-free.
#pragma once
#include <cstdio>
#include <unistd.h>
#include "frame_source.hpp"

class PipeFrameSource : public IFrameSource {
public:
    // fd defaults to stdin. format/width/height describe the raw stream.
    PipeFrameSource(Frame::Format fmt, int w, int h, double fps, int fd = 0)
        : fmt_(fmt), w_(w), h_(h), fps_(fps), fd_(fd) {
        bytesPerPixel_ = (fmt == Frame::Format::YUYV) ? 2 : 3;
        frameBytes_ = (size_t)w_ * h_ * bytesPerPixel_;
    }

    bool read(Frame& out) override {
        out.format = fmt_; out.w = w_; out.h = h_;
        out.data.resize(frameBytes_);
        size_t got = 0;
        while (got < frameBytes_) {
            ssize_t n = ::read(fd_, out.data.data() + got, frameBytes_ - got);
            if (n <= 0) return false;   // EOF / error
            got += (size_t)n;
        }
        return true;
    }

    double fps() const override { return fps_; }
    // A pipe may be a fast file dump (needs pacing) or a live feed; pace to
    // the configured fps to be safe either way.
    bool isRealtime() const override { return false; }
    const char* name() const override { return "pipe"; }

private:
    Frame::Format fmt_;
    int    w_, h_, fd_;
    double fps_;
    int    bytesPerPixel_;
    size_t frameBytes_;
};
