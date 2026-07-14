# Building from source

## Prerequisites

**macOS**

- Xcode (latest stable, with command-line tools installed: `xcode-select --install`)
- CMake ≥ 3.24 and Ninja, via [Homebrew](https://brew.sh):

  ```sh
  brew install cmake ninja
  ```

**Windows**

- Visual Studio 2022 (or the standalone [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)), with the "Desktop development with C++" workload
- CMake ≥ 3.24 (bundled with recent Visual Studio installs, or install separately)

JUCE 8.0.14 and Catch2 v3 are fetched automatically via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) — no manual JUCE checkout is required.

## Configure, build, test

**macOS**

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For a macOS Universal Binary (arm64 + x86_64), add the architectures flag to the configure step:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

**Windows**

```sh
cmake -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

(The default Visual Studio generator is multi-config, so `--config Release` / `-C Release` select the configuration at build/test time rather than at configure time.)

## Speeding up repeat builds

CPM caches downloaded/fetched dependencies. Set `CPM_SOURCE_CACHE` to a persistent directory to avoid re-fetching JUCE and Catch2 on every clean build:

```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"
```

Set this before running `cmake -B build ...`. This is also how CI caches dependencies between runs (see [`.github/workflows/ci.yml`](../.github/workflows/ci.yml)).

## Build artefacts

Built plugin formats (AU, VST3, Standalone) land under `build/Silentium_artefacts/`, in a `Release/` subdirectory for multi-config generators (e.g. the Windows Visual Studio generator) or directly under `Silentium_artefacts/` for single-config generators (e.g. Ninja), depending on generator and configuration.
