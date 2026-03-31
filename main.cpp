#include "video_reader.hpp"
#include "ascii_converter.hpp"
#include "video_writer.hpp"

#include <iostream>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <system_error>

// ─── CLI Argument Parsing ────────────────────────────────────────────────────

struct Config {
    std::string input_path;
    std::string output_path;

    // ASCII options
    int    cols          = 120;
    bool   cols_max      = false;
    double font_scale    = 0.4;
    bool   use_color     = true;
    bool   invert        = false;
    double saturation_boost = 1.0;
    double brighten = 1.0;
    double mono_luma_scale = 0.5;
    std::string char_ramp = ""; // empty = use default

    // Output video options
    std::string fourcc   = "mp4v";
    double fps_override  = 0.0;  // 0 = match source
    int    width         = 0;
    int    height        = 0;
    bool   debug_fullscreen = false;

    // Misc
    bool verbose         = false;
    int  max_frames      = 0;    // 0 = all
};

void printUsage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " -i <input> -o <output> [options]\n"
        "\n"
        "Required:\n"
        "  -i <path>          Input video file\n"
        "  -o <path>          Output video file\n"
        "\n"
        "ASCII options:\n"
        "  --cols <n>         Character columns in ASCII grid (default: 120)\n"
        "  --cols-max         Auto-pick maximum codec-safe output columns\n"
        "  --font-scale <f>   Font scale for rendering (default: 0.4)\n"
        "  --no-color         Render in white-on-black instead of source colors\n"
        "  --invert           Invert luminance mapping (dark chars on light bg)\n"
        "  --saturation <f>   HSV saturation scale before convert (default: 1.5; use 1 for none)\n"
        "  --brighten <f>     Luma scale for color mode (default: 1.0; >1 brighter)\n"
        "  --mono-darken <f>  Luma scale for --no-color (default: 0.8; 1 disables darkening)\n"
        "  --ramp <string>    Custom character density ramp (darkest to brightest)\n"
        "\n"
        "Output options:\n"
        "  --fourcc <code>    Video codec fourcc (default: mp4v)\n"
        "  --fps <f>          Override output FPS (default: match source)\n"
        "  --width <n>        Output width in pixels (default: auto)\n"
        "  --height <n>       Output height in pixels (default: auto)\n"
        "  --debug-fullscreen Print frame sizing diagnostics during write\n"
        "\n"
        "Misc:\n"
        "  --max-frames <n>   Process only first N frames (useful for testing)\n"
        "  -v, --verbose      Print per-frame progress\n"
        "  -h, --help         Show this help\n"
        "\n"
        "Examples:\n"
        "  " << prog << " -i input.mp4 -o ascii_out.mp4\n"
        "  " << prog << " -i input.mp4 -o out.mp4 --cols-max\n"
        "  " << prog << " -i input.mp4 -o out.mp4 --cols 80 --no-color --invert\n"
        "  " << prog << " -i input.mp4 -o out.mp4 --max-frames 100 -v\n";
}

Config parseArgs(int argc, char** argv) {
    Config cfg;
    if (argc < 2) {
        printUsage(argv[0]);
        std::exit(0);
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto nextArg = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + arg);
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "-i") {
            cfg.input_path = nextArg();
        } else if (arg == "-o") {
            cfg.output_path = nextArg();
        } else if (arg == "--cols") {
            cfg.cols = std::stoi(nextArg());
        } else if (arg == "--cols-max") {
            cfg.cols_max = true;
        } else if (arg == "--font-scale") {
            cfg.font_scale = std::stod(nextArg());
        } else if (arg == "--no-color") {
            cfg.use_color = false;
        } else if (arg == "--invert") {
            cfg.invert = true;
        } else if (arg == "--saturation") {
            cfg.saturation_boost = std::stod(nextArg());
        } else if (arg == "--brighten") {
            cfg.brighten = std::stod(nextArg());
        } else if (arg == "--mono-darken") {
            cfg.mono_luma_scale = std::stod(nextArg());
        } else if (arg == "--ramp") {
            cfg.char_ramp = nextArg();
        } else if (arg == "--fourcc") {
            cfg.fourcc = nextArg();
            if (cfg.fourcc.size() != 4) {
                throw std::invalid_argument("fourcc must be exactly 4 characters");
            }
        } else if (arg == "--fps") {
            cfg.fps_override = std::stod(nextArg());
        } else if (arg == "--width") {
            cfg.width = std::stoi(nextArg());
        } else if (arg == "--height") {
            cfg.height = std::stoi(nextArg());
        } else if (arg == "--debug-fullscreen") {
            cfg.debug_fullscreen = true;
        } else if (arg == "--max-frames") {
            cfg.max_frames = std::stoi(nextArg());
        } else if (arg == "-v" || arg == "--verbose") {
            cfg.verbose = true;
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    if (cfg.input_path.empty())  throw std::invalid_argument("Missing -i <input>");
    if (cfg.output_path.empty()) throw std::invalid_argument("Missing -o <output>");

    return cfg;
}

namespace {
bool canOpenVideoWriter(const VideoWriter::Options& wr_opts, int width, int height) {
    if (wr_opts.fourcc.size() != 4 || width <= 0 || height <= 0) return false;

    const int fourcc = cv::VideoWriter::fourcc(
        wr_opts.fourcc[0], wr_opts.fourcc[1], wr_opts.fourcc[2], wr_opts.fourcc[3]
    );
    const auto probe_path = (std::filesystem::temp_directory_path() /
                            ("asciiify_probe_" +
                             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                             ".mp4")).string();

    cv::VideoWriter probe;
    const bool opened = probe.open(probe_path, fourcc, wr_opts.fps, cv::Size(width, height), true);
    if (opened) probe.release();

    std::error_code ec;
    std::filesystem::remove(probe_path, ec);
    return opened;
}

bool renderableWithCols(const Frame& sample, const AsciiConverter::Options& asc_template,
                        const VideoWriter::Options& wr_opts, int cols) {
    try {
        AsciiConverter::Options opts = asc_template;
        opts.cols = cols;
        AsciiConverter converter(opts);
        AsciiFrame ascii = converter.convert(sample);

        const int out_w = (wr_opts.width  > 0) ? wr_opts.width  : ascii.rendered.cols;
        const int out_h = (wr_opts.height > 0) ? wr_opts.height : ascii.rendered.rows;
        return canOpenVideoWriter(wr_opts, out_w, out_h);
    } catch (...) {
        return false;
    }
}

int findMaxCodecSafeCols(const Frame& sample, const AsciiConverter::Options& asc_template,
                         const VideoWriter::Options& wr_opts) {
    constexpr int kMaxProbeCols = 12000;

    if (!renderableWithCols(sample, asc_template, wr_opts, 1)) {
        throw std::runtime_error("Could not initialize output writer even at 1 column");
    }

    int lo = 1;
    int hi = 2;
    while (hi <= kMaxProbeCols && renderableWithCols(sample, asc_template, wr_opts, hi)) {
        lo = hi;
        hi *= 2;
    }
    hi = std::min(hi, kMaxProbeCols);

    while (lo < hi) {
        const int mid = lo + (hi - lo + 1) / 2;
        if (renderableWithCols(sample, asc_template, wr_opts, mid)) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}
} // namespace

// ─── Progress Bar ────────────────────────────────────────────────────────────

void printProgress(int current, int total, double elapsed_s) {
    const int bar_width = 40;
    float pct = total > 0 ? static_cast<float>(current) / total : 0.f;
    int filled = static_cast<int>(bar_width * pct);

    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i)
        std::cout << (i < filled ? '=' : (i == filled ? '>' : ' '));
    std::cout << "] "
              << std::setw(3) << static_cast<int>(pct * 100) << "% "
              << current << "/" << total;

    if (elapsed_s > 0 && pct > 0) {
        double eta = elapsed_s / pct * (1.0 - pct);
        std::cout << "  ETA: " << std::fixed << std::setprecision(1) << eta << "s";
    }

    std::cout << "  " << std::flush;
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    Config cfg;
    try {
        cfg = parseArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        return 1;
    }

    try {
        // ── Open input ──────────────────────────────────────────────────────
        std::cout << "[*] Opening: " << cfg.input_path << "\n";
        VideoReader reader(cfg.input_path);
        auto info = reader.getInfo();

        std::cout << "    Resolution : " << info.width << "x" << info.height << "\n"
                  << "    FPS        : " << info.fps << "\n"
                  << "    Frames     : " << info.total_frames << "\n"
                  << "    Codec      : " << info.codec << "\n\n";

        // ── Build converter ─────────────────────────────────────────────────
        AsciiConverter::Options asc_opts;
        asc_opts.cols           = cfg.cols;
        asc_opts.font_scale     = cfg.font_scale;
        asc_opts.use_color      = cfg.use_color;
        asc_opts.invert         = cfg.invert;
        asc_opts.saturation_boost = cfg.saturation_boost;
        asc_opts.color_luma_scale = cfg.brighten;
        asc_opts.mono_luma_scale = cfg.mono_luma_scale;
        if (!cfg.char_ramp.empty()) asc_opts.char_ramp = cfg.char_ramp;

        // ── Build writer ────────────────────────────────────────────────────
        VideoWriter::Options wr_opts;
        wr_opts.fps    = cfg.fps_override > 0 ? cfg.fps_override : info.fps;
        wr_opts.fourcc = cfg.fourcc;
        wr_opts.width  = cfg.width;
        wr_opts.height = cfg.height;
        wr_opts.debug_fullscreen = cfg.debug_fullscreen;

        if (cfg.cols_max && (cfg.width > 0 || cfg.height > 0)) {
            throw std::invalid_argument("--cols-max cannot be combined with --width/--height");
        }

        if (cfg.cols_max) {
            Frame sample;
            bool got_sample = false;
            reader.forEachFrame([&](Frame&& frame) -> bool {
                sample = std::move(frame);
                got_sample = true;
                return false;
            });
            if (!got_sample || sample.image.empty()) {
                throw std::runtime_error("Failed to read a sample frame for --cols-max");
            }

            std::cout << "[*] Probing maximum codec-safe columns...\n";
            const int max_cols = findMaxCodecSafeCols(sample, asc_opts, wr_opts);
            cfg.cols = max_cols;
            asc_opts.cols = max_cols;
            std::cout << "    Selected cols: " << max_cols << "\n\n";
        }

        std::cout << "[*] Output: " << cfg.output_path << "\n"
                  << "    FPS    : " << wr_opts.fps << "\n"
                  << "    Codec  : " << wr_opts.fourcc << "\n"
                  << "    Cols   : " << cfg.cols << "\n\n";

        AsciiConverter converter(asc_opts);
        VideoWriter writer(cfg.output_path, wr_opts);

        // ── Process frames ──────────────────────────────────────────────────
        int total = (cfg.max_frames > 0)
                        ? std::min(cfg.max_frames, info.total_frames)
                        : info.total_frames;

        std::cout << "[*] Processing " << total << " frames...\n";

        auto t_start = std::chrono::steady_clock::now();
        int processed = 0;

        reader.forEachFrame([&](Frame&& frame) -> bool {
            if (cfg.max_frames > 0 && frame.index >= cfg.max_frames) return false;

            AsciiFrame ascii = converter.convert(frame);
            writer.writeFrame(ascii);
            ++processed;

            if (cfg.verbose) {
                std::cout << "  Frame " << frame.index
                          << " @ " << std::fixed << std::setprecision(1)
                          << frame.timestamp_ms << "ms\n";
            } else {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - t_start).count();
                printProgress(processed, total, elapsed);
            }

            return true;
        });

        std::cout << "\n";
        writer.finalize();

        auto t_end = std::chrono::steady_clock::now();
        double total_s = std::chrono::duration<double>(t_end - t_start).count();

        std::cout << "\n[✓] Done! Processed " << processed << " frames in "
                  << std::fixed << std::setprecision(2) << total_s << "s "
                  << "(" << std::setprecision(1) << (processed / total_s) << " fps)\n"
                  << "    Output saved to: " << cfg.output_path << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
