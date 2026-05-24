#pragma once

#include "ascii_options.hpp"
#include "glyph_atlas.hpp"
#include "image_buffer.hpp"

#include <string>
#include <vector>

struct AsciiResult {
    ImageBuffer canvas;
    std::vector<std::string> rows;
};

class AsciiCore {
public:
    explicit AsciiCore(AsciiOptions opts);

    AsciiResult convert(const ImageBuffer& src_bgr) const;
    void convertToSize(const ImageBuffer& src_bgr, ImageBuffer& dst_bgr) const;
    void convertFloatRgbaToSize(const FloatImageBuffer& src, FloatImageBuffer& dst) const;

    const AsciiOptions& options() const { return opts_; }

private:
    AsciiOptions opts_;
    mutable GlyphAtlas atlas_;
    mutable bool atlas_ready_ = false;
    mutable std::string atlas_ramp_;
    mutable double atlas_font_scale_ = -1.0;

    void ensureAtlas() const;
    char luminanceToChar(uint8_t luma) const;
    int resolveRows(int src_w, int src_h, const GlyphMetrics& metrics) const;
};

std::string defaultFontPath();
