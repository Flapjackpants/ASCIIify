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
    if (opts_.debug_fullscreen) {
        std::cout << "[VideoWriter][debug] Initialized encoder at "
                  << opts_.width << "x" << opts_.height
                  << " fps=" << opts_.fps
                  << " codec=" << opts_.fourcc << "\n";
    }
}

void VideoWriter::writeFrame(const AsciiFrame& ascii_frame) {
    const cv::Mat& img = ascii_frame.rendered;
    if (img.empty()) {
        throw std::runtime_error("Cannot write empty frame (index=" +
                                  std::to_string(ascii_frame.index) + ")");
    }

    if (!initialized_) {
        // Respect explicit user target size when provided; otherwise infer from first frame.
        const int init_w = (opts_.width  > 0) ? opts_.width  : img.cols;
        const int init_h = (opts_.height > 0) ? opts_.height : img.rows;
        initialize(init_w, init_h);
    }

    // Always normalize to encoder size before writing to avoid backend-dependent
    // top-left anchoring/padding when frame sizes disagree.
    cv::Mat out_frame;
    bool resized = false;
    if (img.cols != opts_.width || img.rows != opts_.height) {
        cv::resize(img, out_frame, cv::Size(opts_.width, opts_.height), 0.0, 0.0, cv::INTER_AREA);
        resized = true;
    } else {
        out_frame = img;
    }

    if (opts_.debug_fullscreen) {
        const bool log_this_frame = (frames_written_ < 5) || (frames_written_ % 120 == 0);
        if (log_this_frame) {
            std::cout << "[VideoWriter][debug] frame=" << ascii_frame.index
                      << " in=" << img.cols << "x" << img.rows
                      << " target=" << opts_.width << "x" << opts_.height
                      << " out=" << out_frame.cols << "x" << out_frame.rows
                      << " resized=" << (resized ? "yes" : "no")
                      << "\n";
        }
        if (out_frame.cols != opts_.width || out_frame.rows != opts_.height) {
            throw std::runtime_error("Debug fullscreen check failed: output frame does not match encoder size");
        }
    }

    writer_.write(out_frame);
    ++frames_written_;
}

void VideoWriter::finalize() {
    if (writer_.isOpened()) {
        writer_.release();
        std::cout << "[VideoWriter] Finalized: " << output_path_
                  << " (" << frames_written_ << " frames written)\n";
    }
}
