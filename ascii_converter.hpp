#pragma once
#include "frame.hpp"
#include <string>

struct AsciiConverterOptions {
    // Number of character columns in the ASCII grid
    int cols = 120;
    // Font scale for rendering characters back to image
    double font_scale = 0.4;
    // Thickness of rendered characters
    int font_thickness = 1;
    // Background color (BGR)
    cv::Scalar bg_color = cv::Scalar(0, 0, 0);
    // Foreground color (BGR); if use_color=true this is ignored
    cv::Scalar fg_color = cv::Scalar(255, 255, 255);
    // If true, color each character with the source pixel color
    bool use_color = true;
    // Character density ramp (darkest → brightest)
    std::string char_ramp = " .`-_':,;^=+/\"|)\\<>)iv%xclrs{*}I?!][1taeo7zjLunT#JCwfy325Fp6mqSghVd4EgXPGZbYkOA8U$KHDBWNMR0Q@";
    // If true, invert luminance mapping (light bg, dark chars)
    bool invert = false;
};

class AsciiConverter {
public:
    using Options = AsciiConverterOptions;

    explicit AsciiConverter(const Options& opts = Options{});

    // Convert a single frame to ASCII art rendered as an image
    AsciiFrame convert(const Frame& frame) const;

private:
    Options opts_;

    // Map a 0-255 luminance value to a character from the ramp
    char luminanceToChar(uint8_t luma) const;

    // Compute per-cell average color and luminance from a ROI
    void sampleCell(const cv::Mat& gray, const cv::Mat& color,
                    int row, int col,
                    int cell_w, int cell_h,
                    uint8_t& out_luma, cv::Scalar& out_color) const;
};
