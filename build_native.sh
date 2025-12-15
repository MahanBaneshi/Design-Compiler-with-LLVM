# file: build_native.sh
#!/usr/bin/env bash
set -e

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <compiler_binary> <source.lang> [output_exe]"
    exit 1
fi

COMPILER_BIN="$1"
SOURCE_FILE="$2"
OUT_EXE="${3:-a.out}"

OBJ_FILE="program.o"

# 1) تولید object از سورس زبان خودت
"$COMPILER_BIN" --emit-obj="$OBJ_FILE" --no-run "$SOURCE_FILE"

# 2) کامپایل runtime (فقط اگر runtime.o وجود ندارد یا قدیمی است)
if [ ! -f runtime.o ] || [ Runtime.cpp -nt runtime.o ]; then
    g++ -c Runtime.cpp -o runtime.o
fi

# 3) لینک نهایی
g++ "$OBJ_FILE" runtime.o -o "$OUT_EXE"

echo "Built native executable: $OUT_EXE"
echo "Run with: ./$OUT_EXE"
