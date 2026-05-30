// output_sink.hpp - where rendered ANSI bytes go.
//
// Abstracting the sink (Dependency Inversion) lets the renderer be driven in
// tests against an in-memory StringSink, and lets the IoT build redirect the
// stream to a socket/fd instead of the local terminal.
#pragma once
#include <cstdio>
#include <string>
#include <unistd.h>

class IOutputSink {
public:
    virtual ~IOutputSink() = default;
    virtual void write(const char* data, size_t n) = 0;
    virtual void flush() = 0;
    void write(const std::string& s) { write(s.data(), s.size()); }
};

// Writes to a raw file descriptor (default stdout). Suitable for piping the
// ASCII stream over ssh/netcat to a remote viewer on a headless target.
class FdSink : public IOutputSink {
public:
    explicit FdSink(int fd = STDOUT_FILENO) : fd_(fd) {}
    void write(const char* data, size_t n) override {
        while (n > 0) {
            ssize_t w = ::write(fd_, data, n);
            if (w <= 0) break;
            data += w; n -= (size_t)w;
        }
    }
    void flush() override {}   // raw fd is unbuffered
private:
    int fd_;
};

// Captures everything in memory. Used by the unit tests.
class StringSink : public IOutputSink {
public:
    void write(const char* data, size_t n) override { buffer.append(data, n); }
    void flush() override {}
    std::string buffer;
};
