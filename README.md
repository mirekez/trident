# Trident IDE

Trident Integrated Design Environment

## Layout

- `backend/` - C++ HTTP/RPC server and RPC implementation files.
- `backend/tests/` - test-content providers plus a small executable test runner.
- `gui/` - plain JavaScript GUI files.
- `starter.cpp` - launches the backend and opens the browser.
- `CMakeLists.txt` - builds backend, backend tests, starter, and exposes a no-op `gui` target.

## Build

From an activated `.conda` environment with CMake and the requested compilers available:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Run

```sh
./build/trident_starter
```

Or run the backend directly:

```sh
./build/trident_backend 8080
```

* requires Firefox installed