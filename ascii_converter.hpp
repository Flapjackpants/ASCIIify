#pragma once
#include "frame.hpp"
#include "core/ascii_options.hpp"

#include <string>

struct AsciiConverterOptions : AsciiOptions {};

class AsciiConverter {
public:
    using Options = AsciiConverterOptions;

    explicit AsciiConverter(const Options& opts = Options{});

    AsciiFrame convert(const Frame& frame) const;

private:
    Options opts_;
};
