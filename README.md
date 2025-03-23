<div align="center">

# ModernCore

</div>

ModernCore is an abandoned Wayland compositor. Some of the technology developed for its needs has been repurposed to make image viewers.

The first viewer is *vv*, which can display images in the terminal. See [doc/vv.md](doc/vv.md) for more details.

The second viewer is *iv*, a Wayland application with support for fractional scaling and HDR. See [doc/iv.md](doc/iv.md) for more details.

If you are interested in more details about this project in general, you may want to read <https://wolf.nereid.pl/posts/image-viewer/>.

## Image formats

The following types of image files can be viewed with ModernCore:

- BC (Block Compression, also known as DXTC, S3TC), in DDS container,
- OpenEXR,
- HEIC,
- AVIF,
- JPEG,
- JPEG XL,
- PCX,
- PNG,
- ETC (Ericsson Texture Compression), in PVR container,
- RAW, digital camera negatives, virtually all formats,
- TGA,
- BMP,
- PSD (Photoshop),
- GIF (animations not supported),
- RGBE (Radiance HDR),
- PIC (Softimage),
- PPM and PGM (only binary),
- TIFF,
- WebP (with animations),
- PDF,
- SVG.

For viewing PDF files, you must have the Poppler library (glib backend) installed on your system. This library is not used at all at build time, and is not an explicit runtime dependency of ModernCore, unlike the other libraries.

## High dynamic range

Loading of HDR images is supported for OpenEXR, HEIC, AVIF, JPEG XL, and RGBE formats.

HDR gain maps are supported in Apple HEIC files.

There is initial support for extracting HDR content from RAW images. Some images may not appear as expected.

## Building

Follow the standard CMake build process.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

> [!NOTE]
> Installing the binaries requires setting up proper installation paths and having proper permissions. You must know what you are doing. Installation is not required to get started, you can run the executables from the build directory.

> [!TIP]
> By default ModernCore is built with the `-march=native` compiler option, to enable SIMD processing. If you want to build an executable that will run on any machine, you may want to disable this with the `MARCH_NATIVE` CMake option. In such a case, it is recommended to manually set an appropriate `-march` level for the CPU architecture baseline you want to support. Not doing so will make everything unnecessarily slow.

> [!CAUTION]
> ModernCore is written in and for *the current year*, and it requires *the latest* libraries and other software to be installed on the system. Don't ask me why it won't build or run on your OS, which insists on shipping years old software (it's *always* Ubuntu).
