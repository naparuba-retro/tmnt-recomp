#!/bin/bash
# Formats the C port with clang-format (style in .clang-format).
#   bash tools/format_c.sh            reformats src/**/*.c *.h in place
#   bash tools/format_c.sh --check    changes nothing, lists the files that are not formatted (exit 1 if any)
cd "$(dirname "$0")/.."
CF=$(command -v clang-format || ls "/c/Program Files"*"/Microsoft Visual Studio"/*/*/VC/Tools/Llvm/bin/clang-format.exe 2>/dev/null | head -1)
[ -n "$CF" ] || { echo "clang-format not found (Visual Studio 'C++ Clang tools' component, or LLVM)"; exit 2; }
FILES=$(ls src/*.c src/*.h src/*/*.c src/*/*.h | grep -v '^src/third_party/') # the third-party single-file libraries are not ours to format
if [ "$1" = "--check" ]; then
    bad=0
    for f in $FILES; do "$CF" --style=file --dry-run --Werror "$f" >/dev/null 2>&1 || { echo "not formatted: $f"; bad=1; }; done
    exit $bad
fi
"$CF" --style=file -i $FILES && echo "formatted: $(echo $FILES | wc -w) files"
