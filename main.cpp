#include "video_reader.hpp"
#include "ascii_converter.hpp"
#include "video_writer.hpp"

#include <iostream>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <iomanip>

// ─── CLI Argument Parsing ────────────────────────────────────────────────────

struct Config {
    std::string input_path;
    std::string output_path;

    // ASCII options
    int    cols          = 120;
    double font_scale    = 0.4;
    bool   use_color     = true;
    bool   invert        = false;
    std::string char_ramp = ""; // empty = use default

    // Output video options
    std::string fourcc   = "mp4v";
    double fps_override  = 0.0;  // 0 = match source
    int    width         = 0;
    int    height        = 0;

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
        "  --font-scale <f>   Font scale for rendering (default: 0.4)\n"
        "  --no-color         Render in white-on-black instead of source colors\n"
        "  --invert           Invert luminance mapping (dark chars on light bg)\n"
        "  --ramp <string>    Custom character density ramp (darkest to brightest)\n"
        "\n"
        "Output options:\n"
        "  --fourcc <code>    Video codec fourcc (default: mp4v)\n"
        "  --fps <f>          Override output FPS (default: match source)\n"
        "  --width <n>        Output width in pixels (default: auto)\n"
        "  --height <n>       Output height in pixels (default: auto)\n"
        "\n"
        "Misc:\n"
        "  --max-frames <n>   Process only first N frames (useful for testing)\n"
        "  -v, --verbose      Print per-frame progress\n"
        "  -h, --help         Show this help\n"
        "\n"
        "Examples:\n"
        "  " << prog << " -i input.mp4 -o ascii_out.mp4\n"
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
        } else if (arg == "--font-scale") {
            cfg.font_scale = std::stod(nextArg());
        } else if (arg == "--no-color") {
            cfg.use_color = false;
        } else if (arg == "--invert") {
            cfg.invert = true;
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
        if (!cfg.char_ramp.empty()) asc_opts.char_ramp = cfg.char_ramp;

        AsciiConverter converter(asc_opts);

        // ── Build writer ────────────────────────────────────────────────────
        VideoWriter::Options wr_opts;
        wr_opts.fps    = cfg.fps_override > 0 ? cfg.fps_override : info.fps;
        wr_opts.fourcc = cfg.fourcc;
        wr_opts.width  = cfg.width;
        wr_opts.height = cfg.height;

        std::cout << "[*] Output: " << cfg.output_path << "\n"
                  << "    FPS    : " << wr_opts.fps << "\n"
                  << "    Codec  : " << wr_opts.fourcc << "\n\n";

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
