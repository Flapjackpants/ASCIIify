#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct Frame {
    cv::Mat image;       // Original BGR image
    int index;           // Frame number (0-based)
    double timestamp_ms; // Timestamp in milliseconds
};

struct AsciiFrame {
    cv::Mat rendered;             // ASCII art rendered back as an image
    std::vector<std::string> rows; // Raw ASCII character rows (for optional text output)
    int index;
    double timestamp_ms;
};
