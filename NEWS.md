# Changelog

## 20260222

### iv

- `ENABLE_HDR_WSI` is no longer automatically set by the application. You may set it manually, if you need it.
- **Selection tool**: Interactive selection rectangle with resize handles.
  - Drag to select image regions.
  - Resize selection by dragging corners/edges.
  - Click to cancel.
  - Visual "marching ants" selection indicator.
- **Clipboard support**
  - `Ctrl+C` - Copy selection (or entire image) to clipboard.
  - `Ctrl+X` - Cut selection to clipboard (fills with black).
  - `Ctrl+V` - Paste image from clipboard.
  - Tone mapping support when copying HDR images to clipboard and the clipboard destination does not support HDR.
  - HDR images can be transferred if the clipboard destination supports EXR format.
- **Save functionality**: `Ctrl+S` saves image to file (PNG for SDR, EXR for HDR)

### exrconv

Added a new utility to load a HDR image and save it to disk in EXR format.

### moderncore

- Prefer Vulkan drivers supporting HDR. This may result in choosing software rasterizer over e.g. Nvidia driver if [Vulkan Wayland HDR WSI Layer](https://github.com/Zamundaaa/VK_hdr_layer) is not set up.
- **JPEG gain map**: Ultra HDR and ISO 21496-1 images can be opened with proper HDR data reconstruction.
- **Wide gamut JPEG** are now loaded as HDR.
- Use proper white point value when loading HEIF images.
- Boolean values parser is now case insensitive.

### Build system

- Added CMake presets (`debug`, `release`, `static`, `profile`, `tsan`, `coverage`)
- Added framework for unit testing and coverage analysis.

---

## 20250421

### iv

- Escape key closes application.
- Added navigation between multiple images in directory.
- Window maximization state is now saved between sessions.

### moderncore

- Image loaders now expect expanded ~ in home paths.
- Significantly improved speed of image format detection.

---

## 20250323

- Initial release of the iv image viewer.

---

## As vv (https://github.com/wolfpld/vv/)

Before moderncore was made public, parts of it were released in a separate vv repository.

### 3.2 (20250215)

#### JPEG color management

This release correctly applies color profiles to JPEG files.

#### OpenEXR view windows

EXR images with more complex data and display windows are now loaded correctly.

#### AgX tone mapping operator

The tone mapping operator for viewing HDR images can be selected with the new command line option `-t`. In addition to the previously available PBR Neutral operator, the AgX operator is now supported, with two optional looks (golden and punchy).

#### Support for CMYK in JPEG

With this release, vv is now able to load and properly display CMYK JPEG images.

#### JPEG rotation

Image rotation metadata is now properly read and applied to JPEG files.

#### Alpha channel in sixel mode and block mode

Both sixel and block mode do not support transparency, which was not accounted for before. The alpha channel is now replaced by a solid background when these modes are active.

#### Sixel compatibility

Image data emitted in sixel mode is now compatible with a much wider variety of terminal emulators, at the expense of lower image quality. Terminal capability checks have been extended to include more terminals with sixel support.

#### JPEG library compatibility

Previously, vv required libjpeg-turbo to build and run. This release adds support for the classic libjpeg library.

### 3.1 (20241223)

This release introduces a number of optimizations that take advantage of modern hardware capabilities. This includes multi-core processing and SIMD code in three variants:

- SSE4.1 + FMA,
- AVX2,
- AVX512.

Note that FMA is typically provided together with AVX2, so the SSE4.1 code path won't be of much use to anyone (it's still needed to process the data if the number of pixels in the image is not divisible by two, which is an extreme edge case).

Since the x64 baseline only mandates SSE2 support, vv is now built with the `-march=native` option by default. This enables compiler-level support for all the SIMD features your CPU supports. It also makes the resulting executable less portable, which can be a problem in some use cases. You can use the `MARCH_NATIVE` CMake option to turn this off.

The optimizations make loading EXR, HEIF and AVIF files significantly faster. One of the test AVIF HDR image files that took 8+ seconds to load with vv 3.0 now loads in 0.8 seconds on my machine.

This release also introduces a new option, `-w file.png`, which saves the decoded image to a file instead of printing it to the screen.

### 3.0.1 (20241215)

Fix building on gcc.

### 3.0 (20241215)

#### Color management

This release introduces proper handling of color profiles embedded in image files. This is an important feature in today's world where things are no longer exclusively sRGB. Color management is implemented in this release for OpenEXR, JPEG XL, HEIF, and AVIF images.

#### High dynamic range

The HDR tone mapping pipeline previously available for OpenEXR images has been generalized for use with other image types that can carry HDR data. HDR processing currently supports OpenEXR, HEIF, AVIF, JPEG XL, and RGBE images.

#### Animation support

Added playback of animations contained in WebP images. This is generally available on all terminals with the text-only fallback mode. Animation playback via the Kitty graphics protocol has very limited support in terminal emulators at the moment, but is also available. Images displayed via the Kitty protocol will continue to animate even after vv has stopped running.

### 2.1 (20241125)

Fixed loading of muxed (animated) webp images. There is no support for animations.

### 2.0 (20241121)

- Support for vector images: SVG, PDF.
- FreeBSD build fixes.

### 1.1 (20241115)

- Build fixes.
- Support for BC4, BC5 in non-DX10 DDS containers.
- PCX support.
- Ability to set custom background (checkerboard or solid color).

### 1.0 (20241111)

Initial release.