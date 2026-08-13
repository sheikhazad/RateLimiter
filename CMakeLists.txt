cmake_minimum_required(VERSION 3.28)

project(RateLimiter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use Apple's macOS SDK with Homebrew LLVM.
execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE MACOS_SDK
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_executable(RateLimiter
    Main.cpp
)

target_sources(RateLimiter
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            RateLimiter.cppm
)

target_compile_options(RateLimiter
    PRIVATE
        -isysroot ${MACOS_SDK}
)