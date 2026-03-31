#include "video_reader.hpp"
#include <stdexcept>
#include <iostream>

VideoReader::VideoReader(const std::string &filepath)
    : filepath_(filepath)
{
    cap_.open(filepath);
    if (!cap_.isOpened())
    {
        throw std::runtime_error("Failed to open video file: " + filepath);
    }
    loadInfo();
}

VideoReader::~VideoReader()
{
    if (cap_.isOpened())
        cap_.release();
}

void VideoReader::loadInfo()
{
    info_.width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    info_.height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    info_.fps = cap_.get(cv::CAP_PROP_FPS);
    info_.total_frames = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    info_.duration_ms = (info_.fps > 0)
                            ? (info_.total_frames / info_.fps) * 1000.0
                            : 0.0;

    // Codec fourcc → human readable
    int fourcc = static_cast<int>(cap_.get(cv::CAP_PROP_FOURCC));
    char codec[5] = {
        static_cast<char>(fourcc & 0xFF),
        static_cast<char>((fourcc >> 8) & 0xFF),
        static_cast<char>((fourcc >> 16) & 0xFF),
        static_cast<char>((fourcc >> 24) & 0xFF),
        '\0'};
    info_.codec = std::string(codec);
}

VideoReader::VideoInfo VideoReader::getInfo() const
{
    return info_;
}

bool VideoReader::isOpen() const
{
    return cap_.isOpened();
}

void VideoReader::forEachFrame(std::function<bool(Frame &&)> callback)
{
    // Reset to beginning
    cap_.set(cv::CAP_PROP_POS_FRAMES, 0);

    cv::Mat raw;
    int index = 0;

    while (true)
    {
        double ts = cap_.get(cv::CAP_PROP_POS_MSEC);
        if (!cap_.read(raw) || raw.empty())
            break;

        Frame f;
        f.image = raw.clone(); // clone so cap_ can reuse its buffer
        f.index = index++;
        f.timestamp_ms = ts;

        if (!callback(std::move(f)))
            break;
    }
}
