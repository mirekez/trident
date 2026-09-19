# Trident IDE

Trident Integrated Design Environment

## development

Currently development goes only under Windows MINGW-64 and Miniconda to be confident in Win64 building process.
Win64 requires the following to be done:
 - Install msys2-x86_64-20240727.exe, Miniconda3-py39_24.7.1-0-Windows-x86_64.exe, run MSYS2 MSYS console
 - git clone https://github.com/mirekez/scalepnr; cd scalepnr
And for Linux:
 - git clone ssh://github.com/mirekez/scalepnr; cd scalepnr
 - wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh; ./Miniconda3-latest-Linux-x86_64.sh
 - source ~/miniconda3/bin/activate; conda init
Then for both Win&Lin:
 - conda create -p ./.conda; source activate base; conda activate ./.conda; conda env update --file requirements.yaml
 - mkdir build; cd build; cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ..; make

## license

This software is distributed under GPLv3, except libraries in the folder libs/ which have their own Open-source licenses.

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

Build downloaded tools:

```sh
cmake --build build --target trident_tools
```

On Windows, configure from MSYS Bash with `-G "Unix Makefiles"`.
CppHDL uses the existing `.conda/Library` LLVM/Clang packages and is included
when `TRIDENT_BUILD_CPPHDL=ON` (the default). LLVM source fetching remains off.
The compiler must be link-compatible with the installed LLVM/Clang libraries;
successful package detection alone does not establish that compatibility.

When configuring the patched cpphdl source directly, set
`CPPHDL_LOCAL_CONDA_PREFIX` to the absolute path of Trident's `.conda` or
`.conda/Library` directory. Both layouts are recognized. Keep
`CPPHDL_FETCH_LLVM=OFF` to use installed packages.

## Run

```sh
./build/trident_starter
```

Or run the backend directly:

```sh
./build/trident_backend 8080
```

* requires Firefox installed
