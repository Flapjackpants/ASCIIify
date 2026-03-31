# video_ascii

A C++ command-line tool that converts every frame of a video into ASCII art and recomposes them into a new video file.

## Architecture

```
video_ascii/
├── main.cpp              # CLI entry point & argument parsing
├── frame.hpp             # Shared Frame / AsciiFrame data structures
├── video_reader.hpp/cpp  # OpenCV-based frame extraction
├── ascii_converter.hpp/cpp  # Frame → ASCII art rendering
├── video_writer.hpp/cpp  # ASCII frames → output video
└── CMakeLists.txt
```

### Pipeline

```
Input Video
    │
    ▼
VideoReader::forEachFrame()      ← streams frames one at a time (memory efficient)
    │
    ▼  Frame { cv::Mat image, index, timestamp_ms }
    │
AsciiConverter::convert()
    │   1. Downsample frame to cols×rows character grid
    │   2. For each cell: compute average luminance & color
    │   3. Map luminance → character from density ramp
    │   4. Render characters onto output canvas via cv::putText
    │
    ▼  AsciiFrame { cv::Mat rendered, vector<string> rows }
    │
VideoWriter::writeFrame()        ← writes rendered frame to output video
    │
    ▼
Output Video
```

## Dependencies

- **OpenCV 4.x** (core, imgproc, videoio)
- **CMake 3.16+**
- **C++17** compiler (GCC 9+, Clang 10+, MSVC 2019+)

### Install OpenCV

**Ubuntu/Debian:**
```bash
sudo apt install libopencv-dev
```

**macOS (Homebrew):**
```bash
brew install opencv
```

**Windows (vcpkg):**
```bash
vcpkg install opencv4
```

## Build

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.logicalcpu)
```

Binary will be at `build/video_ascii`.

## Usage

```
video_ascii -i <input> -o <output> [options]

Required:
  -i <path>          Input video file
  -o <path>          Output video file

ASCII options:
  --cols <n>         Character columns in ASCII grid (default: 120)
  --cols-max         Auto-pick maximum codec-safe output columns
  --font-scale <f>   Font scale for rendering (default: 0.4)
  --no-color         Render in white-on-black instead of source colors
  --invert           Invert luminance mapping (dark chars on light bg)
  --saturation <f>   HSV saturation scale before convert (default: 1.0, none)
  --brighten <f>     Luma scale for color mode (default: 1.0; >1 brighter)
  --mono-darken <f>  Luma scale for --no-color (default: 0.5; 1 disables darkening)
  --ramp <string>    Custom character density ramp (darkest to brightest)

Output options:
  --fourcc <code>    Video codec fourcc (default: mp4v)
  --fps <f>          Override output FPS (default: match source)
  --width <n>        Output width in pixels (default: auto)
  --height <n>       Output height in pixels (default: auto)

Misc:
  --max-frames <n>   Process only first N frames (for testing)
  -v, --verbose      Print per-frame progress
  -h, --help         Show help
```

## Examples

```bash
# Basic conversion (colored ASCII)
./video_ascii -i input.mp4 -o ascii_out.mp4

# Auto-pick the highest safe column count for the selected codec
./video_ascii -i input.mp4 -o out.mp4 --cols-max

# Classic white-on-black, 80 columns
./video_ascii -i input.mp4 -o out.mp4 --cols 80 --no-color

# Increase color vividness for better readability
./video_ascii -i input.mp4 -o out.mp4 --saturation 1.8

# Brighten color-mode output without changing saturation
./video_ascii -i input.mp4 -o out.mp4 --brighten 1.2

# Monochrome mode with stronger darkening
./video_ascii -i input.mp4 -o out.mp4 --no-color --mono-darken 0.7

# Dark-on-light (inverted) with custom ramp
./video_ascii -i input.mp4 -o out.mp4 --invert --ramp " .-+*#@"

# Test with just the first 60 frames
./video_ascii -i input.mp4 -o test.mp4 --max-frames 60 -v

# Force 1280x720 output, preserve source FPS
./video_ascii -i input.mp4 -o out.mp4 --width 1280 --height 720
```

## How the ASCII Conversion Works

1. **Grid sizing** — The frame is divided into `cols` columns. Row count is derived from `cols` and the character aspect ratio (~2:1 height:width for monospace fonts).
2. **Cell sampling** — For each character cell, the average luminance (from grayscale) and average BGR color are computed.
3. **Character mapping** — Luminance is mapped linearly onto a density ramp string. Darker cells get sparse characters (space, `.`), brighter ones get dense characters (`#`, `@`).
4. **Rendering** — Each character is drawn onto a blank canvas using `cv::putText`. By default, each character is tinted with the source cell's average color; use `--no-color` for white-on-black mono output.

## Potential Extensions

- **Multithreaded conversion** — use a thread pool to convert frames in parallel
- **Audio passthrough** — copy the audio stream using FFmpeg CLI or libav
- **Text output mode** — dump ASCII frames as `.txt` files
- **Custom fonts** — use FreeType for sharper, denser character rendering
- **GPU acceleration** — use OpenCV CUDA modules for grayscale/resize ops
