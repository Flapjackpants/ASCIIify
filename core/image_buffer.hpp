#pragma once

#include "ascii_options.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

struct ImageBuffer {
    int width = 0;
    int height = 0;
    int channels = 3; // BGR or BGRA
    std::vector<uint8_t> data;

    ImageBuffer() = default;

    ImageBuffer(int w, int h, int ch, uint8_t fill = 0)
        : width(w), height(h), channels(ch), data(static_cast<size_t>(w) * h * ch, fill) {}

    size_t stride() const { return static_cast<size_t>(width) * channels; }
    size_t size() const { return static_cast<size_t>(width) * height * channels; }

    uint8_t* pixel(int x, int y) {
        return data.data() + (static_cast<size_t>(y) * width + x) * channels;
    }

    const uint8_t* pixel(int x, int y) const {
        return data.data() + (static_cast<size_t>(y) * width + x) * channels;
    }

    void fill(uint8_t b, uint8_t g, uint8_t r) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t* p = pixel(x, y);
                p[0] = b;
                p[1] = g;
                p[2] = r;
            }
        }
    }
};

struct FloatImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<float> data; // RGBA interleaved

    FloatImageBuffer() = default;

    FloatImageBuffer(int w, int h, float fill = 0.f)
        : width(w), height(h), data(static_cast<size_t>(w) * h * 4, fill) {}

    float* pixel(int x, int y) {
        return data.data() + (static_cast<size_t>(y) * width + x) * 4;
    }

    const float* pixel(int x, int y) const {
        return data.data() + (static_cast<size_t>(y) * width + x) * 4;
    }
};

inline void validateAsciiOptions(const AsciiOptions& opts) {
    if (opts.char_ramp.empty()) {
        throw std::invalid_argument("char_ramp must not be empty");
    }
    if (opts.cols <= 0) {
        throw std::invalid_argument("cols must be > 0");
    }
    if (opts.rows < 0) {
        throw std::invalid_argument("rows must be >= 0");
    }
    if (opts.saturation_boost <= 0.0) {
        throw std::invalid_argument("saturation_boost must be > 0");
    }
    if (opts.color_luma_scale <= 0.0) {
        throw std::invalid_argument("color_luma_scale must be > 0");
    }
    if (opts.mono_luma_scale <= 0.0) {
        throw std::invalid_argument("mono_luma_scale must be > 0");
    }
}
