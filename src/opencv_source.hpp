// opencv_source.hpp - webcam / RTSP / file capture via OpenCV.
//
// The full-featured desktop path: handles RTSP and any codec OpenCV/FFmpeg
// supports. Compiled only when WITH_OPENCV is defined so the core and the IoT
// builds need no OpenCV at all.
//
// IMPORTANT for framerate: forcing raw YUYV (CONVERT_RGB=0) makes most USB
// webcams negotiate their uncompressed mode, which is bandwidth-limited to
// ~5 fps. The default 'auto'/'mjpeg' format lets the camera use MJPEG and hit
// its full 30 fps; OpenCV decodes to BGR (no SIMD luma, but the camera is the
// bottleneck, not the conversion). Choose 'yuyv' only if you specifically want
// the SIMD path and your camera does YUYV at a usable rate.
#pragma once
#ifdef WITH_OPENCV
#include <cstdio>
#include <stdexcept>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include "frame_source.hpp"

class OpenCvFrameSource : public IFrameSource {
public:
    OpenCvFrameSource(const std::string& source, int w, int h,
                      double fpsOverride, const std::string& format) {
        bool numeric = !source.empty() &&
            source.find_first_not_of("0123456789") == std::string::npos;

        if (numeric) {
            cap_.open(std::stoi(source));
            realtime_ = true;                        // camera is self-paced
            if (cap_.isOpened()) configureCamera(format, w, h, fpsOverride);
        } else {
            cap_.open(source);
            realtime_ = isLiveUrl(source);           // rtsp/http live vs file
            if (fpsOverride > 0) cap_.set(cv::CAP_PROP_FPS, fpsOverride);
        }
        if (!cap_.isOpened())
            throw std::runtime_error("opencv: cannot open source '" + source + "'");

        double negotiated = cap_.get(cv::CAP_PROP_FPS);
        fps_ = fpsOverride > 0 ? fpsOverride : negotiated;
    }

    bool read(Frame& out) override {
        cv::Mat m;
        if (!cap_.read(m) || m.empty()) return false;
        if (m.type() == CV_8UC2) {                   // raw YUYV
            out.format = Frame::Format::YUYV;
            out.w = m.cols; out.h = m.rows;
            out.data.assign(m.data, m.data + (size_t)m.cols * m.rows * 2);
        } else if (m.channels() == 3) {              // decoded BGR
            out.format = Frame::Format::BGR;
            out.w = m.cols; out.h = m.rows;
            out.data.assign(m.data, m.data + (size_t)m.cols * m.rows * 3);
        } else {
            return read(out);                        // skip unexpected frame
        }
        return true;
    }

    double fps() const override { return fps_; }
    bool isRealtime() const override { return realtime_; }
    const char* name() const override { return "opencv"; }

private:
    void configureCamera(const std::string& format, int w, int h, double fps) {
        if (format == "yuyv") {
            cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y','U','Y','V'));
            cap_.set(cv::CAP_PROP_CONVERT_RGB, 0);   // raw -> SIMD path
        } else if (format == "mjpeg") {
            cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
            cap_.set(cv::CAP_PROP_CONVERT_RGB, 1);
        }
        // 'auto': leave the driver's default (typically MJPEG) and decode BGR.
        if (w > 0) cap_.set(cv::CAP_PROP_FRAME_WIDTH,  w);
        if (h > 0) cap_.set(cv::CAP_PROP_FRAME_HEIGHT, h);
        if (fps > 0) cap_.set(cv::CAP_PROP_FPS, fps); // ask for the rate too
    }

    static bool isLiveUrl(const std::string& s) {
        return s.rfind("rtsp://", 0) == 0 || s.rfind("rtmp://", 0) == 0 ||
               s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0 ||
               s.rfind("udp://", 0) == 0;
    }

    cv::VideoCapture cap_;
    double fps_ = 0;
    bool   realtime_ = true;
};
#endif // WITH_OPENCV
