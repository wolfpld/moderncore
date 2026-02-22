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

Loading of HDR images is supported for OpenEXR, HEIC, AVIF, JPEG, JPEG XL, and RGBE formats.

HDR gain maps are supported in JPEG and Apple HEIC files.

There is initial support for extracting HDR content from RAW images. Some images may not appear as expected.

## Building

See [INSTALL.md](INSTALL.md) for detailed build instructions, dependencies, and configuration options.

> [!CAUTION]
> ModernCore is written in and for *the current year*, and it requires *the latest* libraries and other software to be installed on the system.

## New features

See [NEWS.md](NEWS.md) for a list of changes in the project.
