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

    // Determine grid dimensions
    // ASCII characters are roughly 2x taller than wide, so rows ≈ cols/2
    const int cols     = opts_.cols;
    const int cell_w   = std::max(1, src_w / cols);
    // Correct for character aspect ratio (~0.5 wide/tall for monospace fonts)
    const int cell_h   = std::max(1, static_cast<int>(cell_w * 2.0));
    const int rows     = src_h / cell_h;

    // Precompute output canvas size
    // We'll use a fixed pixel size per character cell based on font metrics
    // OpenCV FONT_HERSHEY_MONO at scale 0.4 → ~8px wide, ~10px tall per char
    int char_px_w = static_cast<int>(8.0  * opts_.font_scale / 0.4);
    int char_px_h = static_cast<int>(12.0 * opts_.font_scale / 0.4);
    if (char_px_w < 1) char_px_w = 1;
    if (char_px_h < 1) char_px_h = 1;

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
                        cv::FONT_HERSHEY_SIMPLEX,
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
