// ascii_renderer.hpp - composition of the rendering pipeline.
//
// AsciiRenderer owns the ordered stages but knows nothing about how they are
// implemented (it depends only on the IFilter / IToneMapper abstractions).
// makeRenderer() is the composition root that wires concrete stages from a
// Config.
#pragma once
#include <memory>
#include <vector>
#include "image.hpp"
#include "config.hpp"
#include "palette.hpp"
#include "frame_decode.hpp"
#include "downsampler.hpp"
#include "filters.hpp"
#include "tone_mapper.hpp"
#include "ansi_writer.hpp"
#include "output_sink.hpp"

// ---- stage builders (shared by the constructor and live reconfigure) ----
// color=0 forces Mono; otherwise color_mode selects the depth.
inline ColorMode resolveColorMode(const Config& cfg) {
    if (!cfg.color) return ColorMode::Mono;
    if (cfg.color_mode == "mono") return ColorMode::Mono;
    if (cfg.color_mode == "16")   return ColorMode::Ansi16;
    if (cfg.color_mode == "256")  return ColorMode::Ansi256;
    return ColorMode::TrueColor;   // default / "truecolor"
}

inline std::vector<std::unique_ptr<IFilter>> buildFilters(const Config& cfg) {
    std::vector<std::unique_ptr<IFilter>> filters;
    filters.push_back(std::make_unique<BrightnessContrastFilter>(
        cfg.contrast, cfg.brightness));
    if (cfg.edge)
        filters.push_back(std::make_unique<SobelFilter>(cfg.edge_threshold));
    return filters;
}

inline std::unique_ptr<IToneMapper> buildMapper(const Config& cfg, const Palette& pal) {
    return cfg.dither
        ? std::unique_ptr<IToneMapper>(std::make_unique<DitherToneMapper>(pal))
        : std::unique_ptr<IToneMapper>(std::make_unique<DirectToneMapper>(pal));
}

class AsciiRenderer {
public:
    using Stats = AnsiFrameWriter::Stats;

    // The decoder and grid dimensions are fixed for the renderer's lifetime;
    // every other stage is derived from the Config and can be rebuilt live via
    // reconfigure(), which is how the TUI applies settings changes at runtime.
    AsciiRenderer(int cols, int rows, const Config& cfg)
        : cols_(cols), rows_(rows),
          decoder_(std::make_unique<FrameDecoder>()) {
        reconfigure(cfg);
    }

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    // Rebuild every config-derived stage in place. Cheap enough to call on a
    // keypress (never per frame); the fresh writer forces a full redraw, so the
    // change appears immediately and no stale glyphs survive.
    void reconfigure(const Config& cfg) {
        palette_ = std::make_shared<Palette>(cfg.charset, cfg.reverse);
        filters_ = buildFilters(cfg);
        mapper_  = buildMapper(cfg, *palette_);
        ColorMode mode = resolveColorMode(cfg);
        color_   = (mode != ColorMode::Mono);
        writer_  = std::make_unique<AnsiFrameWriter>(*palette_, mode);
        writer_->resize(cols_, rows_);
    }

    // Repaint every cell on the next render (e.g. after dismissing an overlay
    // that occluded part of the frame).
    void forceRedraw() { writer_->forceRedraw(); }

    // Re-fit the output grid (e.g. after a terminal resize). The filters,
    // tone mapper and downsampler are size-agnostic; only the grid dims and
    // the writer's diff buffers need updating. Forces a full redraw next frame.
    void resize(int cols, int rows) {
        if (cols <= 0 || rows <= 0 || (cols == cols_ && rows == rows_)) return;
        cols_ = cols; rows_ = rows;
        writer_->resize(cols, rows);
    }

    Stats render(const Frame& frame, IOutputSink& sink) {
        // luma -> downscale -> float grid
        GrayImage small = down_.gray(decoder_->luma(frame), cols_, rows_);
        FloatGrid grid(cols_, rows_);
        for (size_t i = 0; i < grid.v.size(); ++i) grid.v[i] = small.px[i];

        for (const auto& f : filters_) f->apply(grid);

        mapper_->map(grid, idx_);

        // Convert color at grid resolution into a reused buffer (no full-res
        // RGB image, no per-frame allocation). colorSmall_ stays empty in
        // mono mode, which the writer treats as "no color".
        if (color_) decoder_->colorGrid(frame, cols_, rows_, colorSmall_);

        return writer_->render(idx_, colorSmall_, sink);
    }

private:
    int  cols_, rows_;
    std::shared_ptr<Palette>              palette_;
    std::unique_ptr<FrameDecoder>         decoder_;
    std::vector<std::unique_ptr<IFilter>> filters_;
    std::unique_ptr<IToneMapper>          mapper_;
    bool                                  color_ = false;
    Downsampler                           down_;
    std::unique_ptr<AnsiFrameWriter>      writer_;
    std::vector<uint8_t>                  idx_;
    ColorImage                            colorSmall_;
};

// ---- composition root ---------------------------------------------------
inline std::unique_ptr<AsciiRenderer>
makeRenderer(const Config& cfg, int cols, int rows) {
    return std::make_unique<AsciiRenderer>(cols, rows, cfg);
}
