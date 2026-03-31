#include "ascii_converter.hpp"
#include <opencv2/opencv.hpp>
#include <cmath>
#include <stdexcept>

AsciiConverter::AsciiConverter(const Options& opts)
    : opts_(opts)
{
    if (opts_.char_ramp.empty()) {
        throw std::invalid_argument("char_ramp must not be empty");
    }
    if (opts_.cols <= 0) {
        throw std::invalid_argument("cols must be > 0");
    }
}

char AsciiConverter::luminanceToChar(uint8_t luma) const {
    uint8_t effective = opts_.invert ? (255 - luma) : luma;
    // Map [0,255] → [0, ramp.size()-1]
    size_t idx = static_cast<size_t>(effective) * (opts_.char_ramp.size() - 1) / 255;
    return opts_.char_ramp[idx];
}

void AsciiConverter::sampleCell(const cv::Mat& gray, const cv::Mat& color,
                                 int row, int col,
                                 int cell_w, int cell_h,
                                 uint8_t& out_luma, cv::Scalar& out_color) const
{
    int x = col * cell_w;
    int y = row * cell_h;

    // Clamp to image bounds
    int w = std::min(cell_w, gray.cols - x);
    int h = std::min(cell_h, gray.rows - y);
    if (w <= 0 || h <= 0) {
        out_luma  = 0;
        out_color = opts_.fg_color;
        return;
    }

    cv::Rect roi(x, y, w, h);
    cv::Scalar mean_luma  = cv::mean(gray(roi));
    out_luma = static_cast<uint8_t>(mean_luma[0]);

    if (opts_.use_color) {
        out_color = cv::mean(color(roi));
    } else {
        out_color = opts_.fg_color;
    }
}

AsciiFrame AsciiConverter::convert(const Frame& frame) const {
    if (frame.image.empty()) {
        throw std::runtime_error("Cannot convert empty frame");
    }

    const int src_w = frame.image.cols;
    const int src_h = frame.image.rows;

    const int cols = opts_.cols;

    // Derive character cell geometry from actual OpenCV text metrics instead of
    // hardcoded constants to preserve requested column count and frame aspect.
    constexpr int font_face = cv::FONT_HERSHEY_SIMPLEX;
    int baseline = 0;
    const cv::Size glyph_size = cv::getTextSize(
        "M", font_face, opts_.font_scale, opts_.font_thickness, &baseline
    );
    const int char_px_w = std::max(1, glyph_size.width + 1);
    const int char_px_h = std::max(1, glyph_size.height + baseline + 1);

    // Keep output aspect ratio close to source after accounting for glyph aspect.
    const double source_aspect = static_cast<double>(src_h) / static_cast<double>(src_w);
    const double cell_aspect   = static_cast<double>(char_px_h) / static_cast<double>(char_px_w);
    const int rows = std::max(
        1,
        static_cast<int>(std::lround(static_cast<double>(cols) * source_aspect / cell_aspect))
    );

    // Sampling grid dimensions in source image (ceil-div so right/bottom edges are covered).
    const int cell_w = std::max(1, (src_w + cols - 1) / cols);
    const int cell_h = std::max(1, (src_h + rows - 1) / rows);

    const int out_w = cols * char_px_w;
    const int out_h = rows * char_px_h;

    // Prepare grayscale and color versions of source
    cv::Mat gray, color_bgr;
    cv::cvtColor(frame.image, gray, cv::COLOR_BGR2GRAY);
    color_bgr = frame.image; // already BGR

    // Output image
    cv::Mat canvas(out_h, out_w, CV_8UC3, opts_.bg_color);

    AsciiFrame result;
    result.index        = frame.index;
    result.timestamp_ms = frame.timestamp_ms;
    result.rows.reserve(rows);

    for (int r = 0; r < rows; r++) {
        std::string row_str;
        row_str.reserve(cols);

        for (int c = 0; c < cols; c++) {
            uint8_t luma;
            cv::Scalar cell_color;
            sampleCell(gray, color_bgr, r, c, cell_w, cell_h, luma, cell_color);

            char ch = luminanceToChar(luma);
            row_str += ch;

            // Draw the character onto the canvas
            std::string ch_str(1, ch);
            int px = c * char_px_w;
            int py = r * char_px_h + char_px_h - 2; // baseline offset

            cv::putText(canvas, ch_str,
                        cv::Point(px, py),
                        font_face,
                        opts_.font_scale,
                        cell_color,
                        opts_.font_thickness,
                        cv::LINE_AA);
        }
        result.rows.push_back(std::move(row_str));
    }

    result.rendered = std::move(canvas);
    return result;
}
