#ifndef ZAPOTA_UI_H
#define ZAPOTA_UI_H

/* Opens a native window, paints the CLAP lowpass UI, and closes after ~2 seconds.
 * Windows: GDI. Linux: X11 via dlopen. macOS: AppKit via objc_msgSend.
 * Returns 0 on success. */
int zapota_ui_show(double cutoff_hz);

#endif
