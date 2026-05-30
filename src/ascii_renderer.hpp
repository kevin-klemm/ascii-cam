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

class AsciiRenderer {
public:
    using Stats = AnsiFrameWriter::Stats;

    AsciiRenderer(int cols, int rows,
                  std::shared_ptr<Palette> palette,
                  std::unique_ptr<FrameDecoder> decoder,
                  std::vector<std::unique_ptr<IFilter>> filters,
                  std::unique_ptr<IToneMapper> mapper,
                  bool color)
        : cols_(cols), rows_(rows),
          palette_(std::move(palette)),
          decoder_(std::move(decoder)),
          filters_(std::move(filters)),
          mapper_(std::move(mapper)),
          color_(color),
          writer_(*palette_, color) {
        writer_.resize(cols, rows);
    }

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    Stats render(const Frame& frame, IOutputSink& sink) {
        // luma -> downscale -> float grid
        GrayImage small = down_.gray(decoder_->luma(frame), cols_, rows_);
        FloatGrid grid(cols_, rows_);
        for (size_t i = 0; i < grid.v.size(); ++i) grid.v[i] = small.px[i];

        for (const auto& f : filters_) f->apply(grid);

        mapper_->map(grid, idx_);

        ColorImage colorSmall;
        if (color_)
            colorSmall = down_.color(decoder_->color(frame), cols_, rows_);

        return writer_.render(idx_, colorSmall, sink);
    }

private:
    int  cols_, rows_;
    std::shared_ptr<Palette>              palette_;
    std::unique_ptr<FrameDecoder>         decoder_;
    std::vector<std::unique_ptr<IFilter>> filters_;
    std::unique_ptr<IToneMapper>          mapper_;
    bool                                  color_;
    Downsampler                           down_;
    AnsiFrameWriter                       writer_;
    std::vector<uint8_t>                  idx_;
};

// ---- composition root ---------------------------------------------------
inline std::unique_ptr<AsciiRenderer>
makeRenderer(const Config& cfg, int cols, int rows) {
    auto palette = std::make_shared<Palette>(cfg.charset, cfg.reverse);

    std::vector<std::unique_ptr<IFilter>> filters;
    filters.push_back(std::make_unique<BrightnessContrastFilter>(
        cfg.contrast, cfg.brightness));
    if (cfg.edge)
        filters.push_back(std::make_unique<SobelFilter>(cfg.edge_threshold));

    std::unique_ptr<IToneMapper> mapper =
        cfg.dither ? std::unique_ptr<IToneMapper>(std::make_unique<DitherToneMapper>(*palette))
                   : std::unique_ptr<IToneMapper>(std::make_unique<DirectToneMapper>(*palette));

    return std::make_unique<AsciiRenderer>(
        cols, rows, palette, std::make_unique<FrameDecoder>(),
        std::move(filters), std::move(mapper), cfg.color);
}
