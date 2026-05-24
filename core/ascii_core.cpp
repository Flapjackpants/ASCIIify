#include "ascii_core.hpp"

#include "image_ops.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace {

std::string envFontPath() {
    if (const char* env = std::getenv("ASCIIIFY_FONT")) {
        return env;
    }
    return {};
}

} // namespace

std::string defaultFontPath() {
    if (!envFontPath().empty()) {
        return envFontPath();
    }

    const std::filesystem::path candidates[] = {
        std::filesystem::path("resources/AndaleMono.ttf"),
        std::filesystem::path("../resources/AndaleMono.ttf"),
        std::filesystem::path("../../resources/AndaleMono.ttf"),
        std::filesystem::path("/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/OpenFX/../ASCIIify/resources/AndaleMono.ttf"),
    };

    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            return p.string();
        }
    }

    // Bundled next to OFX plugin Resources folder
    const std::filesystem::path ofx_font = std::filesystem::path("/Library/OFX/Plugins/ASCIIify.ofx.bundle/Contents/Resources/AndaleMono.ttf");
    if (std::filesystem::exists(ofx_font)) {
        return ofx_font.string();
    }

    return "/System/Library/Fonts/Supplemental/Andale Mono.ttf";
}

AsciiCore::AsciiCore(AsciiOptions opts)
    : opts_(std::move(opts)) {
    validateAsciiOptions(opts_);
    if (opts_.font_path.empty()) {
        opts_.font_path = defaultFontPath();
    }
}

void AsciiCore::ensureAtlas() const {
    if (atlas_ready_ &&
        atlas_ramp_ == opts_.char_ramp &&
        std::abs(atlas_font_scale_ - opts_.font_scale) < 1e-9) {
        return;
    }
    if (!atlas_.loadFont(opts_.font_path, opts_.font_scale, opts_.font_thickness, opts_.char_ramp)) {
        throw std::runtime_error("Failed to load font: " + opts_.font_path);
    }
    atlas_ready_ = true;
    atlas_ramp_ = opts_.char_ramp;
    atlas_font_scale_ = opts_.font_scale;
}

char AsciiCore::luminanceToChar(uint8_t luma) const {
    uint8_t effective = opts_.invert ? static_cast<uint8_t>(255 - luma) : luma;
    const size_t ramp_size = opts_.char_ramp.size();
    const size_t idx = static_cast<size_t>(effective) * (ramp_size - 1) / 255;
    return opts_.char_ramp[idx];
}

int AsciiCore::resolveRows(int src_w, int src_h, const GlyphMetrics& metrics) const {
    if (opts_.rows > 0) {
        return opts_.rows;
    }
    const double source_aspect = static_cast<double>(src_h) / static_cast<double>(src_w);
    const double cell_aspect = static_cast<double>(metrics.char_px_h) /
                               static_cast<double>(metrics.char_px_w);
    return std::max(
        1,
        static_cast<int>(std::lround(static_cast<double>(opts_.cols) * source_aspect / cell_aspect)));
}

AsciiResult AsciiCore::convert(const ImageBuffer& src_bgr) const {
    if (src_bgr.width <= 0 || src_bgr.height <= 0 || src_bgr.data.empty()) {
        throw std::runtime_error("Cannot convert empty frame");
    }
    if (src_bgr.channels < 3) {
        throw std::invalid_argument("Expected BGR input with at least 3 channels");
    }

    ensureAtlas();
    const GlyphMetrics& gm = atlas_.metrics();

    ImageBuffer color_bgr = src_bgr;
    if (opts_.saturation_boost > 1.0 + 1e-9) {
        image_ops::applySaturationBoost(color_bgr, opts_.saturation_boost);
    }
    if (opts_.use_color && std::abs(opts_.color_luma_scale - 1.0) > 1e-9) {
        image_ops::scaleColorLuma(color_bgr, opts_.color_luma_scale);
    }

    ImageBuffer gray;
    image_ops::bgrToGray(color_bgr, gray);
    if (!opts_.use_color && std::abs(opts_.mono_luma_scale - 1.0) > 1e-9) {
        image_ops::scaleLuma(gray, opts_.mono_luma_scale);
    }

    const int cols = opts_.cols;
    const int rows = resolveRows(src_bgr.width, src_bgr.height, gm);
    const int out_w = cols * gm.char_px_w;
    const int out_h = rows * gm.char_px_h;

    ImageBuffer canvas(out_w, out_h, 3);
    for (size_t i = 0; i < canvas.data.size(); i += 3) {
        canvas.data[i + 0] = opts_.bg_color[0];
        canvas.data[i + 1] = opts_.bg_color[1];
        canvas.data[i + 2] = opts_.bg_color[2];
    }

    AsciiResult result;
    result.canvas = std::move(canvas);
    result.rows.reserve(rows);

    for (int r = 0; r < rows; ++r) {
        std::string row_str;
        row_str.reserve(static_cast<size_t>(cols));

        for (int c = 0; c < cols; ++c) {
            uint8_t luma = 0;
            uint8_t cell_bgr[3];
            image_ops::sampleCell(gray, color_bgr, r, c, rows, cols, luma, cell_bgr);

            const char ch = luminanceToChar(luma);
            row_str += ch;

            const uint8_t draw_bgr[3] = {
                opts_.use_color ? cell_bgr[0] : opts_.fg_color[0],
                opts_.use_color ? cell_bgr[1] : opts_.fg_color[1],
                opts_.use_color ? cell_bgr[2] : opts_.fg_color[2],
            };

            const int px = c * gm.char_px_w;
            const int py = r * gm.char_px_h + gm.char_px_h - 2;
            atlas_.blitChar(result.canvas, ch, px, py, draw_bgr);
        }
        result.rows.push_back(std::move(row_str));
    }

    return result;
}

void AsciiCore::convertToSize(const ImageBuffer& src_bgr, ImageBuffer& dst_bgr) const {
    const AsciiResult ascii = convert(src_bgr);
    if (dst_bgr.width == ascii.canvas.width && dst_bgr.height == ascii.canvas.height) {
        dst_bgr = ascii.canvas;
        return;
    }
    dst_bgr = ImageBuffer(dst_bgr.width, dst_bgr.height, 3);
    image_ops::resizeBilinear(ascii.canvas, dst_bgr);
}

void AsciiCore::convertFloatRgbaToSize(const FloatImageBuffer& src, FloatImageBuffer& dst) const {
    ImageBuffer src_bgr;
    image_ops::floatRgbaToBgr(src, src_bgr);

    ImageBuffer dst_bgr(dst.width, dst.height, 3);
    convertToSize(src_bgr, dst_bgr);

    image_ops::bgrToFloatRgba(dst_bgr, dst);
}
