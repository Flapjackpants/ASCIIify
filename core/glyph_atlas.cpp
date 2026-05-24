#include "glyph_atlas.hpp"

#include "image_buffer.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
std::vector<uint8_t> readFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open font: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

void uniqueChars(const std::string& ramp, std::string& out) {
    out.clear();
    for (char c : ramp) {
        if (out.find(c) == std::string::npos) {
            out.push_back(c);
        }
    }
    if (out.find('M') == std::string::npos) {
        out.push_back('M');
    }
}
} // namespace

bool GlyphAtlas::loadFont(const std::string& font_path, double font_scale, int thickness,
                          const std::string& char_ramp) {
    font_data_ = readFileBytes(font_path);
    thickness_ = std::max(1, thickness);
    glyphs_.clear();

    stbtt_fontinfo font{};
    if (!stbtt_InitFont(&font, font_data_.data(), stbtt_GetFontOffsetForIndex(font_data_.data(), 0))) {
        return false;
    }

  const float scale = stbtt_ScaleForPixelHeight(
        &font, static_cast<float>(std::max(8.0, font_scale * 40.0)));

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&font, 'M', &advance, &lsb);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&font, 'M', scale, scale, &x0, &y0, &x1, &y1);

    metrics_.char_px_w = std::max(1, static_cast<int>(std::lround(advance * scale)) + 1);
    metrics_.char_px_h = std::max(
        1,
        static_cast<int>(std::lround((ascent - descent) * scale)) + 1);
    metrics_.baseline = static_cast<int>(std::lround(ascent * scale));

    std::string chars;
    uniqueChars(char_ramp, chars);
    for (char c : chars) {
        rasterizeGlyph(c, static_cast<int>(std::max(8.0, font_scale * 40.0)));
    }
    return true;
}

void GlyphAtlas::rasterizeGlyph(char ch, int pixel_height) {
    stbtt_fontinfo font{};
    stbtt_InitFont(&font, font_data_.data(), stbtt_GetFontOffsetForIndex(font_data_.data(), 0));
    const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(pixel_height));

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&font, static_cast<int>(static_cast<unsigned char>(ch)), &advance, &lsb);

    int w = 0, h = 0, xoff = 0, yoff = 0;
    const unsigned char* bitmap = stbtt_GetCodepointBitmap(
        &font, 0, scale,
        static_cast<int>(static_cast<unsigned char>(ch)),
        &w, &h, &xoff, &yoff);

    Glyph g;
    g.width = w;
    g.height = h;
    g.xoff = xoff;
    g.yoff = yoff;
    g.advance = static_cast<int>(std::lround(advance * scale));

    if (bitmap && w > 0 && h > 0) {
        g.alpha.assign(static_cast<size_t>(w) * h, 0);
        for (int yy = 0; yy < h; ++yy) {
            for (int xx = 0; xx < w; ++xx) {
                g.alpha[static_cast<size_t>(yy) * w + xx] = bitmap[yy * w + xx];
            }
        }
    }

    stbtt_FreeBitmap(const_cast<unsigned char*>(bitmap), nullptr);
    glyphs_[ch] = std::move(g);
}

void GlyphAtlas::blitChar(ImageBuffer& canvas, char ch, int x, int y,
                          const uint8_t bgr[3]) const {
    const auto it = glyphs_.find(ch);
    if (it == glyphs_.end()) return;

    const Glyph& g = it->second;
    const int draw_x = x + g.xoff;
    const int draw_y = y + g.yoff + metrics_.baseline - g.height;

    for (int yy = 0; yy < g.height; ++yy) {
        const int cy = draw_y + yy;
        if (cy < 0 || cy >= canvas.height) continue;
        for (int xx = 0; xx < g.width; ++xx) {
            const int cx = draw_x + xx;
            if (cx < 0 || cx >= canvas.width) continue;
            const uint8_t alpha = g.alpha[static_cast<size_t>(yy) * g.width + xx];
            if (alpha == 0) continue;
            uint8_t* dst = canvas.pixel(cx, cy);
            const float a = alpha / 255.f;
            dst[0] = static_cast<uint8_t>(dst[0] * (1.f - a) + bgr[0] * a + 0.5f);
            dst[1] = static_cast<uint8_t>(dst[1] * (1.f - a) + bgr[1] * a + 0.5f);
            dst[2] = static_cast<uint8_t>(dst[2] * (1.f - a) + bgr[2] * a + 0.5f);
        }
    }
}
