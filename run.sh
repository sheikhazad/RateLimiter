#!/bin/bash

set -e

BUILD_DIR="build"

echo "==> Cleaning..."
rm -rf "$BUILD_DIR"

echo "==> Configuring..."

cmake \
    -S . \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
    -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++

echo
echo "==> Building..."

cmake --build "$BUILD_DIR"

echo
echo "==> Running..."

./"$BUILD_DIR"/RateLimiter