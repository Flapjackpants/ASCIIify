#include "video_writer.hpp"
#include <stdexcept>
#include <iostream>

VideoWriter::VideoWriter(const std::string& output_path, const Options& opts)
    : opts_(opts), output_path_(output_path)
{
    // If both dimensions are known up front, initialize immediately
    if (opts_.width > 0 && opts_.height > 0) {
        initialize(opts_.width, opts_.height);
    }
}

VideoWriter::~VideoWriter() {
    if (writer_.isOpened()) {
        writer_.release();
    }
}

void VideoWriter::initialize(int width, int height) {
    int fourcc = cv::VideoWriter::fourcc(
        opts_.fourcc[0], opts_.fourcc[1], opts_.fourcc[2], opts_.fourcc[3]
    );

    writer_.open(output_path_, fourcc, opts_.fps, cv::Size(width, height), true);

    if (!writer_.isOpened()) {
        throw std::runtime_error(
            "Failed to open output video: " + output_path_ +
            " (codec=" + opts_.fourcc + ")"
        );
    }

    opts_.width  = width;
    opts_.height = height;
    initialized_ = true;
}

void VideoWriter::writeFrame(const AsciiFrame& ascii_frame) {
    const cv::Mat& img = ascii_frame.rendered;
    if (img.empty()) {
        throw std::runtime_error("Cannot write empty frame (index=" +
                                  std::to_string(ascii_frame.index) + ")");
    }

    if (!initialized_) {
        initialize(img.cols, img.rows);
    } else if (img.cols != opts_.width || img.rows != opts_.height) {
        // Resize to match the initialized dimensions
        cv::Mat resized;
        cv::resize(img, resized, cv::Size(opts_.width, opts_.height));
        writer_.write(resized);
        ++frames_written_;
        return;
    }

    writer_.write(img);
    ++frames_written_;
}

void VideoWriter::finalize() {
    if (writer_.isOpened()) {
        writer_.release();
        std::cout << "[VideoWriter] Finalized: " << output_path_
                  << " (" << frames_written_ << " frames written)\n";
    }
}
