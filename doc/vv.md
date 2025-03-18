<div align="center">

# vv — terminal image viewer

![Screenshot](screenshots/vv1.png)

</div>

With vv you can display image files directly in your terminal. This works both locally and over remote connections. An extensive range of modern image formats is supported. Image data is displayed in full color, without any color space reduction or dithering. Images are scaled to fit the available space in the terminal. Small images can be upscaled.

*Note: vv was previously published at <https://github.com/wolfpld/vv>, with all the code extracted directly from this repository.*

### High dynamic range

HDR images are properly tone mapped using the Khronos PBR Neutral or AgX operator for display on the SDR terminal.

<div align="center">

![HDR image](screenshots/vv2.png)

</div>

### Color management

Color profiles embedded in images are taken into account to ensure that images look exactly as they should.

<div align="center">

![Proper color management](screenshots/vv6.png)

</div>

### Transparency

Images with an alpha channel are rendered with transparency over the terminal background color. If this is not suitable, vv gives you the option to render the image with either a checkerboard background or a solid color backdrop of your choice.

<div align="center">

![Transparent image](screenshots/vv4.png)

</div>

### Vector images

Vector image formats, such as SVG, are rendered at a terminal-native resolution, with full transparency support.

<div align="center">

![Vector image](screenshots/vv5.png)

</div>

## Terminal support

In order to be able to view images with vv, you need to use a terminal that implements the [Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/). If your terminal can't do this, vv will work in a text-only fallback mode with greatly reduced image resolution.

<div align="center">

![Text mode](screenshots/vv3.png)

</div>

Certain terminal features, such as Unicode fonts or true color support, are assumed to be always available.

On some terminals (e.g. Konsole), images may appear pixelated when using a high DPI monitor. This problem is caused by implementation details of the terminal itself, and cannot be fixed by vv. Try a different terminal if this bothers you.

### Animation

Animated images in WebP format can be played. The text-only fallback mode is driven by vv outputting images on its own. The Kitty graphics protocol allows much more sophisticated control of the animation, in which case the image will continue to play even after vv exits. Note that support for Kitty animations in terminal implementations is currently very limited.
