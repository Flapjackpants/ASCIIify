#pragma once
#include "frame.hpp"
#include <string>
#include <functional>

class VideoReader {
public:
    struct VideoInfo {
        int width;
        int height;
        double fps;
        int total_frames;
        double duration_ms;
        std::string codec;
    };

    explicit VideoReader(const std::string& filepath);
    ~VideoReader();

    // Returns metadata without reading all frames
    VideoInfo getInfo() const;

    // Read all frames; calls callback for each (avoids loading everything into RAM)
    // Return false from callback to stop early
    void forEachFrame(std::function<bool(Frame&&)> callback);

    bool isOpen() const;

private:
    cv::VideoCapture cap_;
    std::string filepath_;
    VideoInfo info_;

    void loadInfo();
};
