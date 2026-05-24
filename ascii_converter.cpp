#include "ascii_converter.hpp"

#include "core/ascii_core.hpp"
#include "core/image_ops.hpp"

#include <opencv2/opencv.hpp>
#include <cstring>
#include <stdexcept>

namespace {

ImageBuffer matToImageBuffer(const cv::Mat& mat) {
    if (mat.empty()) {
        return {};
    }
    cv::Mat bgr;
    if (mat.channels() == 4) {
        cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    } else if (mat.channels() == 3) {
        bgr = mat;
    } else if (mat.channels() == 1) {
        cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
    } else {
        throw std::invalid_argument("Unsupported cv::Mat channel count");
    }

    ImageBuffer buf(bgr.cols, bgr.rows, 3);
    if (!bgr.isContinuous()) {
        bgr = bgr.clone();
    }
    std::memcpy(buf.data.data(), bgr.data, buf.data.size());
    return buf;
}

cv::Mat imageBufferToMat(const ImageBuffer& buf) {
    cv::Mat mat(buf.height, buf.width, CV_8UC3);
    std::memcpy(mat.data, buf.data.data(), buf.data.size());
    return mat;
}

} // namespace

AsciiConverter::AsciiConverter(const Options& opts)
    : opts_(opts) {
    validateAsciiOptions(opts_);
}

AsciiFrame AsciiConverter::convert(const Frame& frame) const {
    if (frame.image.empty()) {
        throw std::runtime_error("Cannot convert empty frame");
    }

    AsciiCore core(opts_);
    const ImageBuffer src = matToImageBuffer(frame.image);
    const AsciiResult result = core.convert(src);

    AsciiFrame out;
    out.index = frame.index;
    out.timestamp_ms = frame.timestamp_ms;
    out.rows = result.rows;
    out.rendered = imageBufferToMat(result.canvas);
    return out;
}
