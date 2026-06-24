#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
PACKAGE_DIR="${PACKAGE_DIR:-$ROOT_DIR/package}"
OUT_DIR="${OUT_DIR:-$PACKAGE_DIR/out}"
PRODUCT_NAME="${PRODUCT_NAME:-trident}"
ARCHIVE="${1:-}"
WORK_DIR=""
BACKEND_PID=""
BACKEND_PORT=""

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

cleanup() {
    stop_backend >/dev/null 2>&1 || true
    if [ "${KEEP_UNPACKED:-0}" != "1" ] && [ -n "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR"
    elif [ -n "$WORK_DIR" ]; then
        echo "Keeping unpacked test tree: $WORK_DIR"
    fi
}
trap cleanup EXIT

require_file() {
    [ -e "$1" ] || fail "Missing required file: $1"
}

require_exe() {
    require_file "$1"
    [ -x "$1" ] || fail "File is not executable: $1"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Required host command is missing: $1"
}

run() {
    echo "[RUN] $*"
    "$@"
}

run_log() {
    local name="$1"
    shift
    local log="$WORK_DIR/$name.log"
    echo "[RUN] $*"
    if ! "$@" >"$log" 2>&1; then
        sed -n '1,160p' "$log" >&2
        fail "$name failed"
    fi
}

capture_allow() {
    local name="$1"
    shift
    local log="$WORK_DIR/$name.log"
    echo "[RUN] $*"
    set +e
    "$@" >"$log" 2>&1
    local rc=$?
    set -e
    echo "$rc" >"$WORK_DIR/$name.rc"
}

latest_archive() {
    find "$OUT_DIR" -maxdepth 1 -type f -name "${PRODUCT_NAME}-*-linux-x86_64.tbz" \
        -printf '%T@ %p\n' 2>/dev/null \
        | sort -nr \
        | head -1 \
        | cut -d' ' -f2-
}

find_tcl_library() {
    local init_file
    init_file="$(find "$UNPACK_DIR" -path '*/tcl8.6/init.tcl' -print -quit 2>/dev/null || true)"
    if [ -n "$init_file" ]; then
        export TCL_LIBRARY
        TCL_LIBRARY="$(dirname "$init_file")"
        echo "[ENV] TCL_LIBRARY=$TCL_LIBRARY"
    fi
}

find_free_port() {
    local port
    for port in $(seq 18080 18120); do
        if ! (echo >/dev/tcp/127.0.0.1/"$port") >/dev/null 2>&1; then
            echo "$port"
            return 0
        fi
    done
    fail "Could not find a free localhost port"
}

stop_backend() {
    if [ -n "$BACKEND_PID" ]; then
        kill "$BACKEND_PID" >/dev/null 2>&1 || true
        wait "$BACKEND_PID" >/dev/null 2>&1 || true
        BACKEND_PID=""
        BACKEND_PORT=""
    fi
}

start_backend() {
    local name="$1"
    shift
    stop_backend
    BACKEND_PORT="$(find_free_port)"
    (
        cd "$UNPACK_DIR"
        ./trident_backend "$@" "$BACKEND_PORT"
    ) >"$WORK_DIR/$name.log" 2>&1 &
    BACKEND_PID="$!"
    for _ in $(seq 1 100); do
        if curl -fsS "http://127.0.0.1:$BACKEND_PORT/" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done
    sed -n '1,160p' "$WORK_DIR/$name.log" >&2
    fail "backend did not answer on port $BACKEND_PORT"
}

rpc_post() {
    local name="$1"
    local endpoint="$2"
    local body="${3:-{}}"
    local log="$WORK_DIR/$name.json"
    echo "[RPC] $endpoint"
    if ! curl -fsS \
        -H 'Content-Type: application/json' \
        -d "$body" \
        "http://127.0.0.1:$BACKEND_PORT$endpoint" >"$log"; then
        cat "$log" >&2 2>/dev/null || true
        fail "RPC failed: $endpoint"
    fi
}

require_rpc_exit_zero() {
    local name="$1"
    local log="$WORK_DIR/$name.json"
    if ! grep -q '"exitCode":0' "$log"; then
        cat "$log" >&2
        fail "RPC action did not return exitCode 0: $name"
    fi
}

if [ -z "$ARCHIVE" ]; then
    ARCHIVE="$(latest_archive)"
fi
[ -n "$ARCHIVE" ] || fail "No package archive found in $OUT_DIR"
ARCHIVE="$(cd "$(dirname "$ARCHIVE")" && pwd -P)/$(basename "$ARCHIVE")"
require_file "$ARCHIVE"

require_command tar
require_command curl
require_command ldd

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/trident-package-test.XXXXXX")"
echo "Testing package: $ARCHIVE"
echo "Temporary unpack dir: $WORK_DIR"

run tar -xjf "$ARCHIVE" -C "$WORK_DIR"

TOP_LEVEL="$(tar -tjf "$ARCHIVE" | sed 's#/.*##' | sort -u | head -1)"
[ -n "$TOP_LEVEL" ] || fail "Package is empty"
UNPACK_DIR="$WORK_DIR/$TOP_LEVEL"
[ -d "$UNPACK_DIR" ] || fail "Could not find unpacked package root: $UNPACK_DIR"

export LD_LIBRARY_PATH="$UNPACK_DIR/.conda/lib:${LD_LIBRARY_PATH:-}"
export PATH="$UNPACK_DIR/.conda/bin:$PATH"

TOOLS_DIR="$UNPACK_DIR/build/tools/build-current"
CPPHDL="$TOOLS_DIR/cpphdl/cpphdl"
CPPHDL_INCLUDE="$TOOLS_DIR/cpphdl/include"
SCALEPNR="$TOOLS_DIR/scalepnr/scalepnr"
YOSYS_DIR="$TOOLS_DIR/yosys"
YOSYS="$YOSYS_DIR/yosys"
CLANG="$UNPACK_DIR/.conda/bin/clang"
CLANGXX="$UNPACK_DIR/.conda/bin/clang++"

find_tcl_library

echo "[CHECK] product files"
require_exe "$UNPACK_DIR/trident_backend"
require_exe "$UNPACK_DIR/trident_starter"
require_exe "$UNPACK_DIR/run_trident.sh"
require_file "$UNPACK_DIR/default_flow.json"
require_file "$UNPACK_DIR/gui/index.html"
require_file "$UNPACK_DIR/tools/versions"
run_log ldd_trident_backend ldd "$UNPACK_DIR/trident_backend"
run_log ldd_trident_starter ldd "$UNPACK_DIR/trident_starter"

echo "[CHECK] backend http smoke"
if [ "${SKIP_BACKEND_HTTP:-0}" = "1" ]; then
    echo "[SKIP] backend http smoke"
else
    PORT="$(find_free_port)"
    (
        cd "$UNPACK_DIR"
        ./trident_backend --test "$PORT"
    ) >"$WORK_DIR/backend.log" 2>&1 &
    BACKEND_PID="$!"
    for _ in $(seq 1 80); do
        if curl -fsS "http://127.0.0.1:$PORT/" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    curl -fsS "http://127.0.0.1:$PORT/" >/dev/null || {
        sed -n '1,120p' "$WORK_DIR/backend.log" >&2
        fail "backend did not answer on port $PORT"
    }
    kill "$BACKEND_PID" >/dev/null 2>&1 || true
    wait "$BACKEND_PID" >/dev/null 2>&1 || true
    BACKEND_PID=""
fi

echo "[CHECK] clang and clang++"
require_exe "$CLANG"
require_exe "$CLANGXX"
run_log clang_version "$CLANG" --version
run_log clangxx_version "$CLANGXX" --version
cat >"$WORK_DIR/hello.cpp" <<'EOF'
#include <iostream>
int main() {
    std::cout << "hello\n";
    return 0;
}
EOF
run "$CLANGXX" -std=c++20 "$WORK_DIR/hello.cpp" -o "$WORK_DIR/hello"
run "$WORK_DIR/hello"

echo "[CHECK] cpphdl"
require_exe "$CPPHDL"
require_file "$CPPHDL_INCLUDE/cpphdl.h"
run_log ldd_cpphdl ldd "$CPPHDL"
capture_allow cpphdl_help "$CPPHDL" --help
if ! grep -Eq 'USAGE|OVERVIEW|cpphdl|Not enough positional' "$WORK_DIR/cpphdl_help.log"; then
    sed -n '1,120p' "$WORK_DIR/cpphdl_help.log" >&2
    fail "cpphdl help output did not look valid"
fi
cat >"$WORK_DIR/cpphdl_include_smoke.cpp" <<'EOF'
#include "cpphdl.h"
using namespace cpphdl;
int main() {
    logic<8> value{};
    (void)value;
    return 0;
}
EOF
run "$CLANGXX" -std=c++20 -I"$CPPHDL_INCLUDE" "$WORK_DIR/cpphdl_include_smoke.cpp" -o "$WORK_DIR/cpphdl_include_smoke"
run "$WORK_DIR/cpphdl_include_smoke"
if [ -x "$TOOLS_DIR/cpphdl/hdlcpp/hdlcpp" ]; then
    run_log ldd_hdlcpp ldd "$TOOLS_DIR/cpphdl/hdlcpp/hdlcpp"
    capture_allow hdlcpp_help "$TOOLS_DIR/cpphdl/hdlcpp/hdlcpp" --help
fi

echo "[CHECK] scalepnr"
require_exe "$SCALEPNR"
run_log ldd_scalepnr ldd "$SCALEPNR"
capture_allow scalepnr_help "$SCALEPNR" --help
if grep -Eq "Can't find a usable init.tcl|application-specific initialization failed" "$WORK_DIR/scalepnr_help.log"; then
    sed -n '1,160p' "$WORK_DIR/scalepnr_help.log" >&2
    fail "scalepnr could not initialize Tcl"
fi
if [ "$(cat "$WORK_DIR/scalepnr_help.rc")" != "0" ]; then
    sed -n '1,160p' "$WORK_DIR/scalepnr_help.log" >&2
    fail "scalepnr --help failed"
fi

echo "[CHECK] yosys"
require_exe "$YOSYS"
export YOSYS_DATDIR="$YOSYS_DIR/share"
run_log ldd_yosys ldd "$YOSYS"
run_log yosys_version "$YOSYS" -V
cat >"$WORK_DIR/yosys_smoke.v" <<'EOF'
module top(input a, output y);
    assign y = a;
endmodule
EOF
run "$YOSYS" -q -p "read_verilog $WORK_DIR/yosys_smoke.v; hierarchy -top top; proc; opt; write_json $WORK_DIR/yosys_smoke.json"
require_file "$WORK_DIR/yosys_smoke.json"

echo "[CHECK] yosys-slang"
SLANG_PLUGIN=""
if [ -f "$YOSYS_DIR/share/plugins/slang.so" ]; then
    SLANG_PLUGIN="$YOSYS_DIR/share/plugins/slang.so"
elif [ -f "$TOOLS_DIR/yosys-slang/slang.so" ]; then
    SLANG_PLUGIN="$TOOLS_DIR/yosys-slang/slang.so"
fi
[ -n "$SLANG_PLUGIN" ] || fail "Missing yosys-slang plugin"
run_log ldd_yosys_slang ldd "$SLANG_PLUGIN"
run_log yosys_slang_help "$YOSYS" -m "$SLANG_PLUGIN" -p "help read_slang"
cat >"$WORK_DIR/slang_smoke.sv" <<'EOF'
module top(input logic a, output logic y);
    always_comb y = a;
endmodule
EOF
run "$YOSYS" -q -m "$SLANG_PLUGIN" -p "read_slang -top top $WORK_DIR/slang_smoke.sv; synth; write_json $WORK_DIR/slang_smoke.json"
require_file "$WORK_DIR/slang_smoke.json"

echo "[CHECK] standalone slang"
SLANG_BIN=""
if [ -d "$TOOLS_DIR/slang/bin" ]; then
    SLANG_BIN="$(find "$TOOLS_DIR/slang/bin" -maxdepth 1 -type f -perm -111 -name 'slang*' -print -quit 2>/dev/null || true)"
fi
if [ -n "$SLANG_BIN" ]; then
    capture_allow slang_version "$SLANG_BIN" --version
    if [ "$(cat "$WORK_DIR/slang_version.rc")" != "0" ]; then
        capture_allow slang_help "$SLANG_BIN" --help
        [ "$(cat "$WORK_DIR/slang_help.rc")" = "0" ] || fail "standalone slang did not answer to --version or --help"
    fi
else
    echo "[INFO] No standalone slang executable in package; yosys-slang plugin was tested."
fi

echo "[CHECK] MemoryPrj RPC build flow"
SAMPLE_PROJECT_SRC="$PACKAGE_DIR/MemoryPrj"
require_file "$SAMPLE_PROJECT_SRC/MemoryPrj.trident"
TEST_PROJECT="$WORK_DIR/MemoryPrj"
cp -a "$SAMPLE_PROJECT_SRC" "$TEST_PROJECT"

if [ "${SKIP_BACKEND_HTTP:-0}" = "1" ]; then
    echo "[SKIP] MemoryPrj RPC build flow"
else
    start_backend memoryprj_backend
    rpc_post memoryprj_load /rpc/load-project "{\"path\":\"$TEST_PROJECT/MemoryPrj.trident\"}"
    rpc_post memoryprj_run /rpc/run '{}'
    require_rpc_exit_zero memoryprj_run
    rpc_post memoryprj_compile /rpc/compile '{}'
    require_rpc_exit_zero memoryprj_compile
    rpc_post memoryprj_synthesize /rpc/synthesize '{}'
    require_rpc_exit_zero memoryprj_synthesize
    stop_backend
fi

echo "Package smoke test passed."
