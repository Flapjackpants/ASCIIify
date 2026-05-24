#pragma once

#include <cstdint>
#include <string>

inline const char* kDefaultCharRamp =
    "            "
    ".`-_':,;^=+/\"|)\\<>)iv%xclrs{*}I?!][1taeo7zjLunT#JCwfy325Fp6mqSghVd4EgXPGZbYkOA8U$KHDBWNMR0Q@";

struct AsciiOptions {
    int cols = 120;
    int rows = 0; // 0 = auto from source aspect ratio and glyph metrics

    double font_scale = 0.4;
    int font_thickness = 1;

    uint8_t bg_color[3] = {0, 0, 0};
    uint8_t fg_color[3] = {255, 255, 255};

    bool use_color = true;
    std::string char_ramp = kDefaultCharRamp;
    bool invert = false;

    double saturation_boost = 1.5;
    double color_luma_scale = 1.0;
    double mono_luma_scale = 0.8;

    std::string font_path;
};
