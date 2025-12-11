#!/usr/bin/env bash
set -euo pipefail

# C++ compiler (you may override with: CXX=clang++ ./run_tests.sh)
CXX=${CXX:-g++}

# We are already inside src
SRC_DIR="."
BUILD_DIR="./build"

mkdir -p "$BUILD_DIR"

echo "=== Step 1: Compile all .cpp files in $(pwd) ==="
echo "$CXX -std=c++17 -Wall -Wextra -O2 $SRC_DIR/*.cpp -o $BUILD_DIR/frontend_test"
$CXX -std=c++17 -Wall -Wextra -O2 "$SRC_DIR"/*.cpp -o "$BUILD_DIR/frontend_test"

BIN="$BUILD_DIR/frontend_test"

if [ ! -x "$BIN" ]; then
    echo "Error: build failed, binary $BIN not found or not executable."
    exit 1
fi

# Detect test directory (prefer test_inputs, fallback to tests)
if [ -d "./test_inputs" ]; then
    TEST_DIR="./test_inputs"
elif [ -d "./tests" ]; then
    TEST_DIR="./tests"
else
    echo "Error: no test_inputs/ or tests/ directory found next to run_tests.sh."
    exit 1
fi

OUT_DIR="$TEST_DIR/out"
mkdir -p "$OUT_DIR"

echo
echo "=== Step 2: Run all tests ==="
echo "Test directory:   $TEST_DIR"
echo "Output directory: $OUT_DIR"
echo

shopt -s nullglob
for file in "$TEST_DIR"/*.lang; do
    base=$(basename "$file" .lang)

    echo "----- Running $base -----"
    echo "Input file: $file"

    # Run the compiled program and capture stdout and stderr
    "$BIN" "$file" > "$OUT_DIR/$base.out" 2> "$OUT_DIR/$base.err" || true

    echo "  -> stdout saved to: $OUT_DIR/$base.out"
    echo "  -> stderr saved to: $OUT_DIR/$base.err"
    echo
done
shopt -u nullglob

echo "All tests completed."
