<div align="center">

# iv — Wayland HDR image viewer

![Screenshot](screenshots/iv1.png)

</div>

iv is a Wayland application that can display images. Fractional scaling is properly supported (you can't run without it), along with true HDR output if you have the proper setup. iv does not use any UI toolkits (such as GTK or Qt) to interact with the Wayland compositor.

### Fractional scaling

A lot of care has been taken to make the pictures look the way they should. This may sound silly, but some image viewers are not able to do this. See <https://wolf.nereid.pl/posts/image-viewer/#interlude-1> for more details.

### High dynamic range

To get HDR working in iv, you may need to install <https://github.com/Zamundaaa/VK_hdr_layer>. The `ENABLE_HDR_WSI=1` flag is automatically set by the viewer, so you don't need to set it up yourself. If a working HDR pipeline is not available, HDR images will be tone mapped to SDR.

### Correct gamma handling

Most image viewers do not handle gamma correction properly. For example, here's a screenshot of a specially crafted image scaled down to 50% in iv on the left and another program on the right. The rectangles on both sides should have the same color, which they do in iv. See <http://www.ericbrasseur.org/gamma.html> for a detailed explanation of this problem.

<div align="center">

![Gamma comparison](screenshots/gamma2.png)

</div>

A more practical example is shown below, where the legibility of scaled-down text depends strongly on the proper handling of the gamma correction. The image on the left is displayed with iv, and the image on the right is displayed with another program that does not handle gamma the right way.

<div align="center">

![Gamma comparison](screenshots/gamma1.png)

</div>

### Usage

To load an image in iv you can:
 - Pass it as a command-line parameter,
 - Drag and drop the image on the viewer window,
 - Copy and paste the image.

You can pan the image by holding down the right mouse button. Zoom is done with the mouse wheel.

Keybindings:
 - `1` sets the zoom level to 100%.
 - `f` makes the image fit in the window (will only scale down).
 - `ctrl+f` makes the image fill the window (will also scale up).
 - `shift+f` resizes the window to fit the image.
 - `F11` enables fullscreen mode.

### Unsupported features

iv is currently unable to display vector images (SVG, PDF). Animations are loaded as single-frame images. Support for these missing features may be added in the future.
