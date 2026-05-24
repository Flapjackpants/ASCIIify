#include "image_ops.hpp"

#include <algorithm>
#include <cmath>

namespace image_ops {
namespace {

inline uint8_t clampU8(int v) {
    return static_cast<uint8_t>(std::max(0, std::min(255, v)));
}

inline uint8_t clampU8f(float v) {
    return clampU8(static_cast<int>(std::lround(v * 255.f)));
}

void rgbToHsv(float r, float g, float b, float& h, float& s, float& v) {
    const float maxc = std::max({r, g, b});
    const float minc = std::min({r, g, b});
    v = maxc;
    const float d = maxc - minc;
    s = (maxc <= 0.f) ? 0.f : d / maxc;
    if (d <= 0.f) {
        h = 0.f;
        return;
    }
    if (maxc == r) {
        h = std::fmod((g - b) / d, 6.f);
    } else if (maxc == g) {
        h = (b - r) / d + 2.f;
    } else {
        h = (r - g) / d + 4.f;
    }
    h *= 60.f;
    if (h < 0.f) h += 360.f;
}

void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    if (s <= 0.f) {
        r = g = b = v;
        return;
    }
    h = std::fmod(h, 360.f);
    if (h < 0.f) h += 360.f;
    const float c = v * s;
    const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    const float m = v - c;
    float rp = 0.f, gp = 0.f, bp = 0.f;
    if (h < 60.f) {
        rp = c; gp = x;
    } else if (h < 120.f) {
        rp = x; gp = c;
    } else if (h < 180.f) {
        gp = c; bp = x;
    } else if (h < 240.f) {
        gp = x; bp = c;
    } else if (h < 300.f) {
        rp = x; bp = c;
    } else {
        rp = c; bp = x;
    }
    r = rp + m;
    g = gp + m;
    b = bp + m;
}

} // namespace

void bgrToGray(const ImageBuffer& src, ImageBuffer& gray) {
    if (src.channels < 3) {
        throw std::invalid_argument("bgrToGray expects at least 3 channels");
    }
    gray = ImageBuffer(src.width, src.height, 1);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const uint8_t* p = src.pixel(x, y);
            const float b = p[0] / 255.f;
            const float g = p[1] / 255.f;
            const float r = p[2] / 255.f;
            const float luma = 0.114f * b + 0.587f * g + 0.299f * r;
            gray.pixel(x, y)[0] = clampU8f(luma);
        }
    }
}

void applySaturationBoost(ImageBuffer& bgr, double saturation_boost) {
    if (saturation_boost <= 1.0 + 1e-9) return;
    for (int y = 0; y < bgr.height; ++y) {
        for (int x = 0; x < bgr.width; ++x) {
            uint8_t* p = bgr.pixel(x, y);
            float r = p[2] / 255.f;
            float g = p[1] / 255.f;
            float b = p[0] / 255.f;
            float h, s, v;
            rgbToHsv(r, g, b, h, s, v);
            s = std::min(1.f, s * static_cast<float>(saturation_boost));
            hsvToRgb(h, s, v, r, g, b);
            p[0] = clampU8f(b);
            p[1] = clampU8f(g);
            p[2] = clampU8f(r);
        }
    }
}

void scaleLuma(ImageBuffer& gray, double scale) {
    if (std::abs(scale - 1.0) <= 1e-9) return;
    for (auto& v : gray.data) {
        v = clampU8(static_cast<int>(std::lround(v * scale)));
    }
}

void scaleColorLuma(ImageBuffer& bgr, double scale) {
    if (std::abs(scale - 1.0) <= 1e-9) return;
    for (auto& v : bgr.data) {
        v = clampU8(static_cast<int>(std::lround(v * scale)));
    }
}

void sampleCell(const ImageBuffer& gray, const ImageBuffer& color,
                int row, int col, int rows, int cols,
                uint8_t& out_luma, uint8_t out_bgr[3]) {
    const int x0 = (col * gray.width) / cols;
    const int x1 = ((col + 1) * gray.width) / cols;
    const int y0 = (row * gray.height) / rows;
    const int y1 = ((row + 1) * gray.height) / rows;
    const int x = std::max(0, std::min(x0, gray.width - 1));
    const int y = std::max(0, std::min(y0, gray.height - 1));
    const int w = std::max(1, x1 - x0);
    const int h = std::max(1, y1 - y0);
    const int x_end = std::min(x + w, gray.width);
    const int y_end = std::min(y + h, gray.height);

    double sum_l = 0.0;
    double sum_b = 0.0;
    double sum_g = 0.0;
    double sum_r = 0.0;
    int count = 0;

    for (int yy = y; yy < y_end; ++yy) {
        for (int xx = x; xx < x_end; ++xx) {
            sum_l += gray.pixel(xx, yy)[0];
            const uint8_t* c = color.pixel(xx, yy);
            sum_b += c[0];
            sum_g += c[1];
            sum_r += c[2];
            ++count;
        }
    }

    out_luma = static_cast<uint8_t>(sum_l / count + 0.5);
    out_bgr[0] = static_cast<uint8_t>(sum_b / count + 0.5);
    out_bgr[1] = static_cast<uint8_t>(sum_g / count + 0.5);
    out_bgr[2] = static_cast<uint8_t>(sum_r / count + 0.5);
}

void resizeBilinear(const ImageBuffer& src, ImageBuffer& dst) {
    if (src.channels != dst.channels) {
        throw std::invalid_argument("resizeBilinear channel mismatch");
    }
    if (dst.width <= 0 || dst.height <= 0) return;

    dst.data.assign(static_cast<size_t>(dst.width) * dst.height * dst.channels, 0);

    const float x_ratio = static_cast<float>(src.width) / dst.width;
    const float y_ratio = static_cast<float>(src.height) / dst.height;

    for (int y = 0; y < dst.height; ++y) {
        const float src_y = (y + 0.5f) * y_ratio - 0.5f;
        const int y0 = std::max(0, static_cast<int>(std::floor(src_y)));
        const int y1 = std::min(src.height - 1, y0 + 1);
        const float fy = src_y - y0;

        for (int x = 0; x < dst.width; ++x) {
            const float src_x = (x + 0.5f) * x_ratio - 0.5f;
            const int x0 = std::max(0, static_cast<int>(std::floor(src_x)));
            const int x1 = std::min(src.width - 1, x0 + 1);
            const float fx = src_x - x0;

            uint8_t* d = dst.pixel(x, y);
            for (int c = 0; c < dst.channels; ++c) {
                const float p00 = src.pixel(x0, y0)[c];
                const float p10 = src.pixel(x1, y0)[c];
                const float p01 = src.pixel(x0, y1)[c];
                const float p11 = src.pixel(x1, y1)[c];
                const float top = p00 + fx * (p10 - p00);
                const float bot = p01 + fx * (p11 - p01);
                d[c] = clampU8(static_cast<int>(std::lround(top + fy * (bot - top))));
            }
        }
    }
}

void floatRgbaToBgr(const FloatImageBuffer& src, ImageBuffer& dst) {
    dst = ImageBuffer(src.width, src.height, 3);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const float* p = src.pixel(x, y);
            uint8_t* d = dst.pixel(x, y);
            d[0] = clampU8f(p[2]); // B from R in RGBA... wait RGBA order is R,G,B,A
            d[1] = clampU8f(p[1]);
            d[2] = clampU8f(p[0]);
        }
    }
}

void bgrToFloatRgba(const ImageBuffer& src, FloatImageBuffer& dst) {
    dst = FloatImageBuffer(src.width, src.height);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const uint8_t* p = src.pixel(x, y);
            float* d = dst.pixel(x, y);
            d[0] = p[2] / 255.f;
            d[1] = p[1] / 255.f;
            d[2] = p[0] / 255.f;
            d[3] = 1.f;
        }
    }
}

} // namespace image_ops
