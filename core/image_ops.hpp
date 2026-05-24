#pragma once

#include "image_buffer.hpp"

#include <cmath>

namespace image_ops {

void bgrToGray(const ImageBuffer& src, ImageBuffer& gray);
void applySaturationBoost(ImageBuffer& bgr, double saturation_boost);
void scaleLuma(ImageBuffer& gray, double scale);
void scaleColorLuma(ImageBuffer& bgr, double scale);

void sampleCell(const ImageBuffer& gray, const ImageBuffer& color,
                int row, int col, int rows, int cols,
                uint8_t& out_luma, uint8_t out_bgr[3]);

void resizeBilinear(const ImageBuffer& src, ImageBuffer& dst);

void floatRgbaToBgr(const FloatImageBuffer& src, ImageBuffer& dst);
void bgrToFloatRgba(const ImageBuffer& src, FloatImageBuffer& dst);

} // namespace image_ops
