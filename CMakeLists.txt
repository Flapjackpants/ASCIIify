cmake_minimum_required(VERSION 3.16)
project(video_ascii VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Dependencies ─────────────────────────────────────────────────────────────
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    video_reader.cpp
    ascii_converter.cpp
    video_writer.cpp
)

add_executable(video_ascii ${SOURCES})

target_include_directories(video_ascii PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(video_ascii PRIVATE ${OpenCV_LIBS})

# ─── Optimization flags ───────────────────────────────────────────────────────
target_compile_options(video_ascii PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:Debug>:-g -fsanitize=address>
)
target_link_options(video_ascii PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

# ─── Install ──────────────────────────────────────────────────────────────────
install(TARGETS video_ascii DESTINATION bin)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
