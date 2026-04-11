## Dependencies

ModernCore requires the following libraries:

### Core Dependencies
- cairo
- lcms2
- libexif
- libheif
- libjpeg
- libjxl
- liblz4
- libpng
- libraw
- librsvg
- libsixel
- libtiff
- libwebp
- OpenEXR
- pugixml
- Vulkan
- wayland-client
- wayland-protocols
- xkbcommon
- zlib (zlib-ng recommended)

### Build Dependencies
- CMake
- C++ compiler
- Python
- shaderc

### Build Recommendations
- ninja
- clang
- mold
- ccache

### Test Dependencies
- Catch2 3.x
- LLVM tools (for coverage: llvm-profdata, llvm-cov)

## Building

### Using CMake Presets (Recommended)

```bash
# Debug build
cmake --preset debug
ninja -C build

# Release build
cmake --preset release
ninja -C build-release

# Static release
cmake --preset static
ninja -C build-static

# With profiler (Tracy)
cmake --preset profile
ninja -C build-profile

# With ThreadSanitizer
cmake --preset tsan
ninja -C build-tsan

# With code coverage
cmake --preset coverage
ninja -C build-coverage
```

### Manual Configuration

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `MARCH_NATIVE` | ON | Enable `-march=native` for SIMD |
| `BUILD_SHARED_LIBS` | ON | Build shared libraries |
| `BUILD_MCORE` | OFF | Build legacy mcore binary |
| `BUILD_TESTS` | ON when top-level | Enable unit testing (requires Catch2) |
| `TRACY_ENABLE` | OFF | Enable Tracy profiler |
| `COVERAGE` | OFF | Enable code coverage |
| `SANITIZE` | "" | Sanitizer flags (e.g., `thread`) |

### Examples

```bash
# Portable build (no native CPU optimizations, require AVX2 + FMA)
cmake --preset release -DMARCH_NATIVE=OFF -DCMAKE_CXX_FLAGS="-march=x86-64-v3"

# Show coverage
cmake --preset coverage
ninja -C build-coverage coverage-report
```

## Testing

```bash
# Run tests
ninja -C build test
```

## Installation

```bash
cmake --install build --prefix /usr/local
```

> **Note:** Installation requires proper permissions and path configuration. The executables can also be run directly from the build directory without installation.

## Platform Notes

ModernCore is developed on current versions of libraries and requires:
- C++20 compiler support
- Vulkan 1.4 (for iv)
- Wayland compositor (for iv)
- Terminal with Kitty graphics protocol support (for vv full functionality)
