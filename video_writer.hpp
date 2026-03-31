#pragma once
#include "frame.hpp"
#include <string>

struct VideoWriterOptions {
    // Output resolution; if either is 0 it will be inferred from the first frame
    int width  = 0;
    int height = 0;
    double fps = 30.0;
    // OpenCV fourcc code string, e.g. "mp4v", "avc1", "XVID"
    std::string fourcc = "mp4v";
    // Print sizing diagnostics to verify fullscreen writes.
    bool debug_fullscreen = false;
};

class VideoWriter {
public:
    using Options = VideoWriterOptions;

    VideoWriter(const std::string& output_path, const Options& opts = Options{});
    ~VideoWriter();

    // Write a single rendered ASCII frame
    void writeFrame(const AsciiFrame& frame);

    // Must be called after all frames are written
    void finalize();

    int framesWritten() const { return frames_written_; }

private:
    cv::VideoWriter writer_;
    Options opts_;
    std::string output_path_;
    bool initialized_ = false;
    int frames_written_ = 0;

    void initialize(int width, int height);
};
