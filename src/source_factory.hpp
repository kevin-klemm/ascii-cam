// source_factory.hpp - choose a capture backend from the source string.
//
// Accepted forms:
//   pipe:yuyv:1280x720   raw frames on stdin (any ffmpeg/encoder feed)
//   pipe:bgr:640x480     raw BGR24 frames on stdin
//   /dev/video0          direct V4L2 capture (Linux, no OpenCV)
//   0, 1, ...            webcam index: OpenCV if built, else /dev/videoN
//   rtsp://, http://,    OpenCV (RTSP / network / file)
//   /path/to.mp4
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include "config.hpp"
#include "frame_source.hpp"
#include "pipe_source.hpp"
#if defined(__linux__)
#include "v4l2_source.hpp"
#endif
#ifdef WITH_OPENCV
#include "opencv_source.hpp"
#endif

inline std::unique_ptr<IFrameSource> makeSource(const Config& cfg) {
    const std::string& s = cfg.source;

    // pipe:<fmt>:<WxH>
    if (s.rfind("pipe:", 0) == 0) {
        std::string rest = s.substr(5);
        size_t colon = rest.find(':');
        std::string fmt = rest.substr(0, colon);
        int w = cfg.cap_width, h = cfg.cap_height;
        if (colon != std::string::npos) {
            std::string dim = rest.substr(colon + 1);
            size_t xpos = dim.find('x');
            if (xpos != std::string::npos) {
                w = std::stoi(dim.substr(0, xpos));
                h = std::stoi(dim.substr(xpos + 1));
            }
        }
        Frame::Format f = (fmt == "bgr") ? Frame::Format::BGR : Frame::Format::YUYV;
        double fps = cfg.fps > 0 ? cfg.fps : 30.0;
        return std::make_unique<PipeFrameSource>(f, w, h, fps);
    }

#if defined(__linux__)
    // explicit V4L2 device node
    if (s.rfind("/dev/video", 0) == 0)
        return std::make_unique<V4l2FrameSource>(
            s, cfg.cap_width, cfg.cap_height, cfg.fps > 0 ? cfg.fps : 30.0);
#endif

    bool numeric = !s.empty() &&
        s.find_first_not_of("0123456789") == std::string::npos;

#ifdef WITH_OPENCV
    return std::make_unique<OpenCvFrameSource>(
        s, cfg.cap_width, cfg.cap_height, cfg.fps);
#else
  #if defined(__linux__)
    if (numeric)
        return std::make_unique<V4l2FrameSource>(
            "/dev/video" + s, cfg.cap_width, cfg.cap_height,
            cfg.fps > 0 ? cfg.fps : 30.0);
  #endif
    throw std::runtime_error(
        "no capture backend for source '" + s + "' (built without OpenCV; "
        "use pipe:/dev/video forms)");
    (void)numeric;
#endif
}
