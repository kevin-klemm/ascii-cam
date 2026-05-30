# AsciiEncodeRender

Real-time **webcam / RTSP / encoder-stream → ASCII → terminal** renderer in
C++17. The output framerate is locked to the source video's framerate. Built
from small, single-responsibility components behind interfaces, with a
dependency-free unit suite and CI/release automation.

## Features

| Requirement | Where |
|---|---|
| Adjustable capture width/height | `width`/`height` in `ascii.conf`, applied by each source |
| Producer/consumer thread split | `src/bounded_queue.hpp` + producer thread in `main.cpp` |
| Custom ASCII charset via config | `src/config.hpp` (`ConfigLoader`), `src/palette.hpp` |
| Brightness / contrast | `BrightnessContrastFilter` in `src/filters.hpp` |
| Reverse video (invert mapping) | `src/palette.hpp` (baked into the LUT) |
| Color output (U/V → RGB → ANSI) | `src/frame_decode.hpp` + `src/ansi_writer.hpp` (`\x1b[38;2;…m`) |
| Dithering | `DitherToneMapper` (Floyd-Steinberg) in `src/tone_mapper.hpp` |
| Sobel edge detection | `SobelFilter` 3×3 kernel convolution in `src/filters.hpp` |
| SIMD YUYV → grayscale | `src/simd.hpp` (NEON / SSE2 / scalar) |
| Inter-frame change analysis | per-cell diff in `src/ansi_writer.hpp` |
| Temporal compression | only changed cells redraw; contiguous runs share one cursor move |
| LUT cache optimization | brightness→index LUT (`palette.hpp`) + cached decimal strings (`ansi_writer.hpp`) |

## Architecture (SOLID)

Each stage is one class with one responsibility, depended upon through an
interface so it can be swapped or tested in isolation:

```
IFrameSource ─► Frame ─► FrameDecoder ─► Downsampler ─► [IFilter…] ─► IToneMapper ─► AnsiFrameWriter ─► IOutputSink
 (capture)      (raw)    (luma/color)    (to grid)      (bright/    (LUT or         (diff + ANSI)      (fd / string)
                                                         Sobel)      dither)
```

- **S** — `Palette`, `Downsampler`, `SobelFilter`, `AnsiFrameWriter`, … each do one thing.
- **O** — add a pixel format, filter, tone mapper or capture backend by adding a
  class; no existing code changes.
- **L** — every `IFilter` / `IToneMapper` / `IFrameSource` is fully substitutable.
- **I** — small interfaces (`IOutputSink` is just `write`/`flush`).
- **D** — `AsciiRenderer` depends on abstractions; `makeRenderer()` /
  `makeSource()` are the composition roots.

The whole core is **OpenCV-free** — OpenCV lives only in `opencv_source.hpp`,
so the IoT build and the unit tests need no OpenCV at all.

## Capture backends

| Source string | Backend | Needs |
|---|---|---|
| `0`, `1`, … | OpenCV webcam (raw YUYV → SIMD path) | OpenCV |
| `rtsp://…`, `http://…`, `file.mp4` | OpenCV / FFmpeg | OpenCV |
| `/dev/video0` | direct V4L2 (mmap YUYV) | Linux only |
| `pipe:yuyv:WxH`, `pipe:bgr:WxH` | raw frames on stdin | nothing |

### IoT / headless Linux targets

Build a tiny binary with no OpenCV and drop it on the device:

```sh
cmake -B build -DWITH_OPENCV=OFF && cmake --build build -j

# direct camera capture on the device:
./build/asciicam ascii.conf /dev/video0

# or connect to ANY encoder/stream by letting ffmpeg decode and pipe raw frames:
ffmpeg -rtsp_transport tcp -i rtsp://cam/stream -f rawvideo -pix_fmt yuyv422 - \
  | ./build/asciicam ascii.conf pipe:yuyv:1280x720

# pipe the ASCII over the network to a viewer (FdSink writes a raw fd):
./build/asciicam ascii.conf /dev/video0 | ssh viewer 'cat'
```

## Build

```sh
# Full (webcam/RTSP). macOS: brew install opencv cmake
#                     Debian/Ubuntu: sudo apt install libopencv-dev cmake g++
cmake -B build -DWITH_OPENCV=ON
cmake --build build -j
./build/asciicam            # webcam 0 with ascii.conf

# IoT (no OpenCV): V4L2 + pipe sources only
cmake -B build -DWITH_OPENCV=OFF && cmake --build build -j
```

## Tests

A ~50-line header-only harness (`test/test_framework.hpp`) — no gtest/catch,
no OpenCV — so the suite runs anywhere with just a compiler.

```sh
ctest --test-dir build --output-on-failure   # or: ./build/unit_tests
```

37 tests cover the SIMD luma extraction, config parsing, palette/LUT, the
brightness/contrast & Sobel filters, both tone mappers, the downsampler, frame
decoding, the diff/temporal ANSI writer, and the bounded queue.

## CI / Release

- `.github/workflows/ci.yml` — on every push/PR: the core+IoT build with unit
  tests, plus full OpenCV builds on Linux and macOS.
- `.github/workflows/release.yml` — on a `v*` tag: builds and packages three
  artifacts (`linux-x86_64-iot`, `linux-x86_64`, `macos-arm64`) with SHA-256
  sums and publishes a GitHub Release.

## Performance & running over SSH

Benchmark (`bench/bench.cpp`, 640×480 → 160×50 grid = 8000 cells, Apple
Silicon):

| Case | ms/frame | FPS ceiling | bytes/frame | @30 fps |
|---|--:|--:|--:|--:|
| mono, motion | 0.22 | ~4400 | 689 | — |
| color, motion | 0.47 | ~2100 | 4.9 K | 0.1 MB/s |
| color, static scene | 0.45 | ~2200 | **0** | 0 (diff) |
| truecolor, full-frame churn | 0.78 | ~1300 | 149 K | 4.5 MB/s |
| 256-color, full-frame churn | 0.65 | ~1500 | 79 K | 2.4 MB/s |
| 16-color, full-frame churn | 0.59 | ~1700 | 29 K | 0.9 MB/s |

**Higher FPS:** the CPU is never the limit (>1000 fps headroom even in color);
output is *FPS-locked to the source* by design. To actually run faster, give
it a faster source — lower the capture resolution (`width`/`height`) so the
camera offers 60 fps, or `fps=60`. Color conversion runs at grid resolution
(`FrameDecoder::colorGrid`) and reuses buffers, so color costs ~2× mono, not
~4×.

**Over SSH the bottleneck is bytes, not CPU.** Three things keep it small:
1. inter-frame diff — a still scene sends 0 bytes;
2. color-pen coalescing — runs of same-colored cells share one escape;
3. `color_mode` — drop to `256`, `16`, or `mono` to cut per-cell color cost.

Worst case (every cell changing every frame) is ~4.5 MB/s in truecolor but
~0.9 MB/s in 16-color and ~0.4 MB/s in mono — comfortable over SSH. Typical
camera scenes sit at 0.1–0.6 MB/s in truecolor.

```sh
# light-weight remote view over ssh:
asciicam ascii.conf /dev/video0   # with color_mode=16 in the config
```

## Configuration

See `ascii.conf` for every option (size, charset, color, dither, edges,
brightness/contrast, reverse). Press `Ctrl-C` to quit; the terminal is
restored on exit.
