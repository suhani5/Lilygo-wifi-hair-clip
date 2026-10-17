# LilyGO T-Display-S3 Wi-Fi screen editor

A PlatformIO project for the 1.9-inch LilyGO T-Display-S3. The board starts
its own password-protected Wi-Fi network and hosts a web page for choosing
between a text screen, two built-in original animations, and uploaded GIFs.
Text and animation settings are saved in non-volatile storage; the GIF is
stored in the board's flash filesystem.

## Use

1. Open this folder in VS Code with the PlatformIO extension installed.
2. Build and upload the `lilygo-t-display-s3` environment over USB.
3. Connect a phone or computer to Wi-Fi network `Display-Setup` using password
   `display123`.
4. Open `http://192.168.4.1` in a browser to pick a screen or animation.
5. To add an animation, upload a GIF from the page. Up to six GIFs can be
   stored and selected from the animation list.

GIF uploads are limited to 384 KB each and the display resolution (320 × 170).
Choose a small looping GIF for best playback. The built-in animations are an
original yellow dot-chomping character and a simple walking pup.
The upload page shows progress and displays a reason if an upload is rejected.
If a GIF is rejected as invalid, connect the USB serial monitor at 115200 baud
to see decoder diagnostics.

Change `AP_PASSWORD` in `src/main.cpp` before uploading if you want a different
access-point password. The board does not need an existing Wi-Fi router.

## Display assumptions

This configuration targets the standard T-Display-S3 with its 170 × 320
ST7789 display. Its display uses the board's 8-bit parallel interface (not
SPI); the firmware also enables the panel power and backlight pins. If your
board is the touch variant or a different hardware revision, verify its
display/pinout before powering or uploading.
