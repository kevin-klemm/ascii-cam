// v4l2_source.hpp - direct V4L2 webcam capture, Linux-only, no OpenCV.
//
// This is the "drop it on the target" path for IoT Linux devices: it talks to
// /dev/videoN straight through the kernel using memory-mapped YUYV buffers,
// which also feeds the SIMD luma path natively. Compiled only on Linux.
#pragma once
#if defined(__linux__)
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "frame_source.hpp"

class V4l2FrameSource : public IFrameSource {
public:
    V4l2FrameSource(const std::string& dev, int w, int h, double fps)
        : fps_(fps) {
        fd_ = ::open(dev.c_str(), O_RDWR);
        if (fd_ < 0) throw std::runtime_error("v4l2: cannot open " + dev);

        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width  = w;
        fmt.fmt.pix.height = h;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        if (xioctl(VIDIOC_S_FMT, &fmt) < 0)
            throw std::runtime_error("v4l2: VIDIOC_S_FMT failed");
        w_ = fmt.fmt.pix.width;
        h_ = fmt.fmt.pix.height;
        if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
            throw std::runtime_error("v4l2: device did not accept YUYV");

        requestBuffers();
        startStreaming();
    }

    ~V4l2FrameSource() override {
        if (fd_ >= 0) {
            v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(VIDIOC_STREAMOFF, &t);
            for (auto& b : buffers_) if (b.start) munmap(b.start, b.length);
            ::close(fd_);
        }
    }

    bool read(Frame& out) override {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(VIDIOC_DQBUF, &buf) < 0) return false;

        out.format = Frame::Format::YUYV;
        out.w = w_; out.h = h_;
        const size_t bytes = (size_t)w_ * h_ * 2;
        out.data.resize(bytes);
        std::memcpy(out.data.data(), buffers_[buf.index].start,
                    std::min<size_t>(bytes, buf.bytesused));

        xioctl(VIDIOC_QBUF, &buf);   // requeue
        return true;
    }

    double fps() const override { return fps_; }
    const char* name() const override { return "v4l2"; }

private:
    struct Buffer { void* start = nullptr; size_t length = 0; };

    int xioctl(unsigned long req, void* arg) {
        int r;
        do { r = ioctl(fd_, req, arg); } while (r == -1 && errno == EINTR);
        return r;
    }

    void requestBuffers() {
        v4l2_requestbuffers req{};
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(VIDIOC_REQBUFS, &req) < 0)
            throw std::runtime_error("v4l2: VIDIOC_REQBUFS failed");
        buffers_.resize(req.count);
        for (unsigned i = 0; i < req.count; ++i) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (xioctl(VIDIOC_QUERYBUF, &buf) < 0)
                throw std::runtime_error("v4l2: VIDIOC_QUERYBUF failed");
            buffers_[i].length = buf.length;
            buffers_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd_, buf.m.offset);
            if (buffers_[i].start == MAP_FAILED)
                throw std::runtime_error("v4l2: mmap failed");
        }
    }

    void startStreaming() {
        for (unsigned i = 0; i < buffers_.size(); ++i) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            xioctl(VIDIOC_QBUF, &buf);
        }
        v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(VIDIOC_STREAMON, &t) < 0)
            throw std::runtime_error("v4l2: VIDIOC_STREAMON failed");
    }

    int    fd_ = -1;
    int    w_ = 0, h_ = 0;
    double fps_;
    std::vector<Buffer> buffers_;
};
#endif // __linux__
