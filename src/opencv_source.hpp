// opencv_source.hpp - webcam / RTSP / file capture via OpenCV.
//
// The full-featured desktop path: handles RTSP and any codec OpenCV/FFmpeg
// supports. Compiled only when WITH_OPENCV is defined so the core and the IoT
// builds need no OpenCV at all.
#pragma once
#ifdef WITH_OPENCV
#include <string>
#include <stdexcept>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include "frame_source.hpp"

class OpenCvFrameSource : public IFrameSource {
public:
    OpenCvFrameSource(const std::string& source, int w, int h, double fpsOverride) {
        bool numeric = !source.empty() &&
            source.find_first_not_of("0123456789") == std::string::npos;
        if (numeric) {
            cap_.open(std::stoi(source));
            cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y','U','Y','V'));
            cap_.set(cv::CAP_PROP_CONVERT_RGB, 0);   // raw YUYV -> SIMD path
        } else {
            cap_.open(source);
        }
        if (!cap_.isOpened())
            throw std::runtime_error("opencv: cannot open source '" + source + "'");
        cap_.set(cv::CAP_PROP_FRAME_WIDTH,  w);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, h);
        fps_ = fpsOverride > 0 ? fpsOverride : cap_.get(cv::CAP_PROP_FPS);
    }

    bool read(Frame& out) override {
        cv::Mat m;
        if (!cap_.read(m) || m.empty()) return false;
        if (m.type() == CV_8UC2) {               // YUYV
            out.format = Frame::Format::YUYV;
            out.w = m.cols; out.h = m.rows;
            out.data.assign(m.data, m.data + (size_t)m.cols * m.rows * 2);
        } else {                                 // BGR (decoded)
            cv::Mat bgr = m.channels() == 3 ? m : cv::Mat();
            if (bgr.empty()) return read(out);   // skip odd frames
            out.format = Frame::Format::BGR;
            out.w = bgr.cols; out.h = bgr.rows;
            out.data.assign(bgr.data, bgr.data + (size_t)bgr.cols * bgr.rows * 3);
        }
        return true;
    }

    double fps() const override { return fps_; }
    const char* name() const override { return "opencv"; }

private:
    cv::VideoCapture cap_;
    double fps_ = 0;
};
#endif // WITH_OPENCV
