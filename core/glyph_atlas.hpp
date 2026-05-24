#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct ImageBuffer;

struct GlyphMetrics {
    int char_px_w = 1;
    int char_px_h = 1;
    int baseline = 0;
};

class GlyphAtlas {
public:
    GlyphAtlas() = default;

    bool loadFont(const std::string& font_path, double font_scale, int thickness,
                  const std::string& char_ramp);

    const GlyphMetrics& metrics() const { return metrics_; }

    void blitChar(ImageBuffer& canvas, char ch, int x, int y,
                  const uint8_t bgr[3]) const;

private:
    struct Glyph {
        int width = 0;
        int height = 0;
        int xoff = 0;
        int yoff = 0;
        int advance = 0;
        std::vector<uint8_t> alpha; // width * height, 0-255
    };

    GlyphMetrics metrics_{};
    std::unordered_map<char, Glyph> glyphs_;
    std::vector<uint8_t> font_data_;
    int thickness_ = 1;

    void rasterizeGlyph(char ch, int pixel_height);
};
