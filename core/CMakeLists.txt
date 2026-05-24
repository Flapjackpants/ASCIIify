add_library(asciiify_core STATIC
    ascii_core.cpp
    glyph_atlas.cpp
    image_ops.cpp
)

target_include_directories(asciiify_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(asciiify_core PUBLIC cxx_std_17)
