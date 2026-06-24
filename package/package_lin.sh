#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
PRODUCT_NAME="${PRODUCT_NAME:-trident}"
STAMP="${VERSION:-$(date +%Y%m%d_%H%M%S)}"
PACKAGE_DIR="${PACKAGE_DIR:-$ROOT_DIR/package}"
STAGE_BASE="${STAGE_BASE:-$PACKAGE_DIR/stage}"
OUT_DIR="${OUT_DIR:-$PACKAGE_DIR/out}"
STAGE_DIR="$STAGE_BASE/$PRODUCT_NAME"
OUT_FILE="$OUT_DIR/${PRODUCT_NAME}-${STAMP}-linux-x86_64.tbz"

CONDA_ROOT="${CONDA_PREFIX:-$ROOT_DIR/.conda}"
if [ ! -x "$CONDA_ROOT/bin/clang++" ]; then
    CONDA_ROOT="$ROOT_DIR/.conda"
fi
CONDA_ROOT="$(cd "$CONDA_ROOT" && pwd -P)"

require_file() {
    if [ ! -e "$1" ]; then
        echo "Missing required file: $1" >&2
        exit 1
    fi
}

copy_file() {
    local src="$1"
    local dst="$2"
    require_file "$src"
    mkdir -p "$(dirname "$dst")"
    cp -a "$src" "$dst"
}

copy_lib_file() {
    local src="$1"
    local dst="$2"
    require_file "$src"
    mkdir -p "$(dirname "$dst")"
    cp -L "$src" "$dst"
}

copy_dir() {
    local src="$1"
    local dst="$2"
    require_file "$src"
    mkdir -p "$(dirname "$dst")"
    cp -a "$src" "$dst"
}

copy_optional_file() {
    local src="$1"
    local dst="$2"
    if [ -e "$src" ]; then
        copy_file "$src" "$dst"
    fi
}

copy_optional_dir() {
    local src="$1"
    local dst="$2"
    if [ -d "$src" ]; then
        copy_dir "$src" "$dst"
    fi
}

copy_ldd_libs() {
    local binary
    for binary in "$@"; do
        [ -e "$binary" ] || continue
        ldd "$binary" 2>/dev/null \
            | awk '/=>/ { print $(NF - 1) } /^[[:space:]]*\// { print $1 }' \
            | while read -r lib; do
                [ -n "$lib" ] || continue
                [ -e "$lib" ] || continue
                lib="$(cd "$(dirname "$lib")" && pwd -P)/$(basename "$lib")"
                case "$lib" in
                    "$CONDA_ROOT"/lib/*)
                        copy_lib_file "$lib" "$STAGE_DIR/.conda/lib/$(basename "$lib")"
                        ;;
                    */.conda/lib/*)
                        copy_lib_file "$lib" "$STAGE_DIR/.conda/lib/$(basename "$lib")"
                        ;;
                esac
            done
    done
}

if [ "${SKIP_BUILD:-0}" != "1" ]; then
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
    cmake --build "$BUILD_DIR" --target trident_backend trident_starter trident_tools
fi

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR" "$OUT_DIR"

copy_file "$BUILD_DIR/trident_backend" "$STAGE_DIR/trident_backend"
copy_file "$BUILD_DIR/trident_starter" "$STAGE_DIR/trident_starter"
copy_file "$ROOT_DIR/default_flow.json" "$STAGE_DIR/default_flow.json"
copy_dir "$ROOT_DIR/gui" "$STAGE_DIR/gui"
copy_file "$ROOT_DIR/tools/versions" "$STAGE_DIR/tools/versions"

cat > "$STAGE_DIR/run_trident.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd -P)"
export LD_LIBRARY_PATH="$DIR/.conda/lib:${LD_LIBRARY_PATH:-}"
export PATH="$DIR/.conda/bin:$PATH"
if [ -f "$DIR/build/tools/tcl8.6.14/library/init.tcl" ]; then
    export TCL_LIBRARY="$DIR/build/tools/tcl8.6.14/library"
fi
cd "$DIR"
exec "$DIR/trident_starter" "$@"
EOF
chmod +x "$STAGE_DIR/run_trident.sh"

TOOLS_BUILD="$BUILD_DIR/tools/build-current"
TOOLS_STAGE="$STAGE_DIR/build/tools/build-current"

CPPHDL_BUILD="$TOOLS_BUILD/cpphdl"
if [ ! -x "$CPPHDL_BUILD/cpphdl" ] && [ -x "$ROOT_DIR/tools/cpphdl/build/cpphdl" ]; then
    CPPHDL_BUILD="$ROOT_DIR/tools/cpphdl/build"
fi
copy_file "$CPPHDL_BUILD/cpphdl" "$TOOLS_STAGE/cpphdl/cpphdl"
copy_optional_file "$CPPHDL_BUILD/hdlcpp/hdlcpp" "$TOOLS_STAGE/cpphdl/hdlcpp/hdlcpp"
copy_dir "$ROOT_DIR/tools/cpphdl/include" "$TOOLS_STAGE/cpphdl/include"
copy_optional_dir "$BUILD_DIR/cpphdl/include" "$STAGE_DIR/build/cpphdl/include"

copy_file "$TOOLS_BUILD/scalepnr/scalepnr" "$TOOLS_STAGE/scalepnr/scalepnr"
copy_dir "$ROOT_DIR/tools/scalepnr/libs/tcl8.6.14/library" "$STAGE_DIR/build/tools/tcl8.6.14/library"

copy_file "$TOOLS_BUILD/yosys/yosys" "$TOOLS_STAGE/yosys/yosys"
copy_optional_file "$TOOLS_BUILD/yosys/yosys-abc" "$TOOLS_STAGE/yosys/yosys-abc"
copy_optional_file "$TOOLS_BUILD/yosys/yosys-config" "$TOOLS_STAGE/yosys/yosys-config"
copy_optional_file "$TOOLS_BUILD/yosys/yosys-filterlib" "$TOOLS_STAGE/yosys/yosys-filterlib"
copy_optional_file "$TOOLS_BUILD/yosys/yosys-smtbmc" "$TOOLS_STAGE/yosys/yosys-smtbmc"
copy_optional_file "$TOOLS_BUILD/yosys/yosys-witness" "$TOOLS_STAGE/yosys/yosys-witness"
copy_dir "$TOOLS_BUILD/yosys/share" "$TOOLS_STAGE/yosys/share"
copy_optional_file "$TOOLS_BUILD/yosys/share/plugins/slang.so" "$TOOLS_STAGE/yosys/share/plugins/slang.so"

copy_optional_file "$TOOLS_BUILD/yosys-slang/slang.so" "$TOOLS_STAGE/yosys-slang/slang.so"
copy_optional_dir "$TOOLS_BUILD/slang/bin" "$TOOLS_STAGE/slang/bin"
copy_optional_dir "$TOOLS_BUILD/slang/lib" "$TOOLS_STAGE/slang/lib"
copy_optional_dir "$TOOLS_BUILD/slang/include" "$TOOLS_STAGE/slang/include"

copy_lib_file "$CONDA_ROOT/bin/clang" "$STAGE_DIR/.conda/bin/clang"
copy_lib_file "$CONDA_ROOT/bin/clang++" "$STAGE_DIR/.conda/bin/clang++"
if [ -e "$CONDA_ROOT/bin/clang-cpp" ]; then
    copy_lib_file "$CONDA_ROOT/bin/clang-cpp" "$STAGE_DIR/.conda/bin/clang-cpp"
fi
if [ -e "$CONDA_ROOT/bin/llvm-config" ]; then
    copy_lib_file "$CONDA_ROOT/bin/llvm-config" "$STAGE_DIR/.conda/bin/llvm-config"
fi
if [ -e "$CONDA_ROOT/bin/llvm-ar" ]; then
    copy_lib_file "$CONDA_ROOT/bin/llvm-ar" "$STAGE_DIR/.conda/bin/llvm-ar"
fi
if [ -e "$CONDA_ROOT/bin/llvm-ranlib" ]; then
    copy_lib_file "$CONDA_ROOT/bin/llvm-ranlib" "$STAGE_DIR/.conda/bin/llvm-ranlib"
fi

copy_dir "$CONDA_ROOT/lib/clang" "$STAGE_DIR/.conda/lib/clang"
copy_optional_dir "$CONDA_ROOT/lib/gcc" "$STAGE_DIR/.conda/lib/gcc"
copy_optional_dir "$CONDA_ROOT/include" "$STAGE_DIR/.conda/include"
copy_optional_dir "$CONDA_ROOT/x86_64-conda-linux-gnu" "$STAGE_DIR/.conda/x86_64-conda-linux-gnu"

copy_ldd_libs \
    "$STAGE_DIR/trident_backend" \
    "$STAGE_DIR/trident_starter" \
    "$CPPHDL_BUILD/cpphdl" \
    "$CPPHDL_BUILD/hdlcpp/hdlcpp" \
    "$TOOLS_BUILD/scalepnr/scalepnr" \
    "$TOOLS_BUILD/yosys/yosys" \
    "$TOOLS_BUILD/yosys/yosys-abc" \
    "$TOOLS_BUILD/yosys/share/plugins/slang.so" \
    "$TOOLS_BUILD/yosys-slang/slang.so" \
    "$CONDA_ROOT/bin/clang" \
    "$CONDA_ROOT/bin/clang++" \
    "$CONDA_ROOT/bin/clang-cpp"

find "$STAGE_DIR" -type f \( -name '*.a' -o -name '*.o' -o -name '*.cmake' \) \
    -path "$STAGE_DIR/build/tools/build-current/yosys/*" -delete

tar -cjf "$OUT_FILE" -C "$STAGE_BASE" "$PRODUCT_NAME"

echo "Package: $OUT_FILE"
du -h "$OUT_FILE"
