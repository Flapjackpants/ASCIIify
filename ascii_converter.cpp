#include "ascii_converter.hpp"
#include <opencv2/opencv.hpp>
#include <cmath>
#include <stdexcept>
#include <vector>

AsciiConverter::AsciiConverter(const Options& opts)
    : opts_(opts)
{
    if (opts_.char_ramp.empty()) {
        throw std::invalid_argument("char_ramp must not be empty");
    }
    if (opts_.cols <= 0) {
        throw std::invalid_argument("cols must be > 0");
    }
    if (opts_.saturation_boost <= 0.0) {
        throw std::invalid_argument("saturation_boost must be > 0");
    }
    if (opts_.color_luma_scale <= 0.0) {
        throw std::invalid_argument("color_luma_scale must be > 0");
    }
    if (opts_.mono_luma_scale <= 0.0) {
        throw std::invalid_argument("mono_luma_scale must be > 0");
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
                                 int rows, int cols,
                                 uint8_t& out_luma, cv::Scalar& out_color) const
{
    // Compute cell bounds proportionally to guarantee full source coverage with no
    // out-of-bounds tail columns/rows.
    const int x0 = (col * gray.cols) / cols;
    const int x1 = ((col + 1) * gray.cols) / cols;
    const int y0 = (row * gray.rows) / rows;
    const int y1 = ((row + 1) * gray.rows) / rows;
    const int x = std::max(0, std::min(x0, gray.cols - 1));
    const int y = std::max(0, std::min(y0, gray.rows - 1));
    const int w = std::max(1, x1 - x0);
    const int h = std::max(1, y1 - y0);

    cv::Rect roi(x, y, std::min(w, gray.cols - x), std::min(h, gray.rows - y));
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

    const int out_w = cols * char_px_w;
    const int out_h = rows * char_px_h;

    cv::Mat color_bgr;
    if (opts_.saturation_boost > 1.0 + 1e-9) {
        cv::Mat hsv;
        cv::cvtColor(frame.image, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> ch;
        cv::split(hsv, ch);
        cv::Mat s32;
        ch[1].convertTo(s32, CV_32F, opts_.saturation_boost);
        cv::min(s32, 255.0, s32);
        cv::max(s32, 0.0, s32);
        s32.convertTo(ch[1], CV_8U);
        cv::merge(ch, hsv);
        cv::cvtColor(hsv, color_bgr, cv::COLOR_HSV2BGR);
    } else {
        color_bgr = frame.image;
    }
    if (opts_.use_color && std::abs(opts_.color_luma_scale - 1.0) > 1e-9) {
        color_bgr.convertTo(color_bgr, CV_8U, opts_.color_luma_scale, 0.0);
    }

    cv::Mat gray;
    cv::cvtColor(color_bgr, gray, cv::COLOR_BGR2GRAY);
    if (!opts_.use_color && std::abs(opts_.mono_luma_scale - 1.0) > 1e-9) {
        gray.convertTo(gray, CV_8U, opts_.mono_luma_scale, 0.0);
    }

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
            sampleCell(gray, color_bgr, r, c, rows, cols, luma, cell_color);

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
