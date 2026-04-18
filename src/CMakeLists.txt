cmake_minimum_required(VERSION 3.10)
project(fps-aiming LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile flags
add_compile_options(-Wall -Wextra -O2)

# Find packages
find_library(RKNNRT rknnrt REQUIRED)
find_library(DRM drm REQUIRED)
find_library(V4L2 v4l2 REQUIRED)

# Includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build HID utils (from first stage)
add_executable(mouse hid/mouse.c)
add_executable(keyboard hid/keyboard.c)
add_executable(mouse_absolute hid/mouse_absolute.c)
target_link_libraries(mouse_absolute m)

# Build second stage pipeline
add_executable(fps-aiming
    capture/v4l2_capture.cpp
    rknn/yolov8_pose.cpp
    preview/drm_preview.cpp
    main_pipeline.cpp
)

target_link_libraries(fps-aiming
    ${RKNNRT}
    ${DRM}
    ${V4L2}
    pthread
    m
)

# Print configuration
message(STATUS "RKNN runtime: ${RKNNRT}")
message(STATUS "DRM: ${DRM}")
message(STATUS "V4L2: ${V4L2}")
