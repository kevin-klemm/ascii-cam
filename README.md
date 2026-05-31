# AsciiEncodeRender

Real-time **webcam / RTSP / encoder-stream → ASCII → terminal** renderer in
C++17. Output is FPS-locked to the source. It diffs frames and only redraws
changed cells, so it stays light enough to run over SSH (drop `color_mode` to
`16` or `mono` on a constrained link).

The core is OpenCV-free; OpenCV is only pulled in for the webcam-index / RTSP /
file backend, so headless Linux targets can build a tiny binary with V4L2 and
stdin-pipe sources only.

## Usage

```sh
asciicam [config.conf] [source]
```

- `config.conf` — path to a config file (default `ascii.conf`; missing file = defaults).
- `source` — overrides the `source` key in the config.
- `-h` / `--help` — print usage.

Press `Ctrl-C` to quit; the terminal is restored on exit.

### Live controls (TUI)

While running on a local terminal you can tune the render without restarting.
Keys are read from `/dev/tty`, so this works regardless of the source (a
`pipe:` feed can still own stdin). Press `?` to toggle an overlay of the
current settings; changes apply on the next frame. When the output is piped or
there is no controlling terminal, the controls are simply inactive.

| Key | Action |
|---|---|
| `c` / `C` | contrast −/+ |
| `b` / `B` | brightness −/+ |
| `r` | toggle reverse |
| `o` | toggle color |
| `m` | cycle `color_mode` (truecolor → 256 → 16 → mono) |
| `d` | toggle dithering |
| `e` | toggle edge detection |
| `t` / `T` | edge threshold −/+ |
| `?` / `h` | show/hide the settings overlay |
| `q` / `Esc` | quit |

Edits are written back to the active config file (the one named on the command
line, or `ascii.conf`) when you hide the panel — and again on exit, so nothing
is lost if you quit with it open. The rewrite is surgical: only the adjusted
keys' values change; comments, layout, and your `source`/resolution settings
are preserved. A missing config file is created with the current settings.

### Sources

The backend is selected from the form of the source string:

| Source | Backend | Needs |
|---|---|---|
| `0`, `1`, … | webcam index | OpenCV (falls back to `/dev/videoN` without it) |
| `/dev/video0` | direct V4L2 (mmap YUYV) | Linux |
| `rtsp://…`, `http://…`, `*.mp4` | network stream / file | OpenCV |
| `pipe:yuyv:WxH` | raw YUYV422 frames on stdin | nothing |
| `pipe:bgr:WxH` | raw BGR24 frames on stdin | nothing |

The `pipe:` forms let any encoder feed the renderer, e.g.:

```sh
ffmpeg -i rtsp://cam/stream -f rawvideo -pix_fmt yuyv422 - \
  | asciicam ascii.conf pipe:yuyv:1280x720
```

## Configuration

Config files are `key = value`, one per line; `#` starts a comment. See
[`ascii.conf`](ascii.conf) for a commented template. Recognized keys:

| Key | Default | Meaning |
|---|---|---|
| `source` | `0` | Capture source (see table above). |
| `width` / `height` | `640` / `480` | Requested capture resolution. |
| `fps` | `0` | Requested capture framerate; `0` = source default. |
| `capture_format` | `auto` | Webcam pixel format (numeric sources): `auto`, `mjpeg`, or `yuyv`. `yuyv` is often ~5 fps on USB cams. |
| `cols` / `rows` | `0` | Output grid; `0` = auto-fit the terminal and follow live resizes. |
| `contrast` | `1.0` | Contrast multiplier. |
| `brightness` | `0` | Brightness offset. |
| `reverse` | `0` | `1` inverts the brightness→charset mapping. |
| `color` | `1` | `1` = colored foreground from chroma, `0` = monochrome. |
| `color_mode` | `truecolor` | Color fidelity vs. output bytes: `truecolor`, `256`, `16`, `mono`. |
| `dither` | `0` | `1` = Floyd-Steinberg dithering. |
| `edge` | `0` | `1` = Sobel edge detection. |
| `edge_threshold` | `0` | `0` = raw magnitude; `>0` = binary edge cutoff. |
| `charset` | `" .:-=+*#%@"` | Glyph ramp, darkest→lightest. Quote to preserve leading spaces. |

## Build

```sh
# Full build (webcam / RTSP / file via OpenCV).
#   macOS:         brew install opencv cmake
#   Debian/Ubuntu: sudo apt install libopencv-dev cmake g++
cmake -B build -DWITH_OPENCV=ON
cmake --build build -j
./build/asciicam            # webcam 0 with ascii.conf

# Headless / IoT build (no OpenCV): V4L2 + pipe sources only.
cmake -B build -DWITH_OPENCV=OFF
cmake --build build -j
```

## Test

A header-only harness (no gtest/catch, no OpenCV) — runs anywhere with a
compiler.

```sh
ctest --test-dir build --output-on-failure   # or: ./build/unit_tests
```
