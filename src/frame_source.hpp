// frame_source.hpp - abstraction over "where frames come from".
//
// The renderer/consumer never knows whether frames arrive from OpenCV, raw
// V4L2, or a pipe. This is what makes the tool deployable on tiny Linux IoT
// targets without OpenCV: pick a lightweight source at the composition root.
#pragma once
#include <memory>
#include <string>
#include "image.hpp"

class IFrameSource {
public:
    virtual ~IFrameSource() = default;

    // Blocking read of the next frame. Returns false on end-of-stream / fatal
    // error. `out` is filled with an owned copy.
    virtual bool read(Frame& out) = 0;

    // Best-effort stream framerate (0 if unknown).
    virtual double fps() const = 0;

    // Human-readable backend name for the status line.
    virtual const char* name() const = 0;
};
