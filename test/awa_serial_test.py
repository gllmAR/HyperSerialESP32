#!/usr/bin/env python3
"""
Synthetic AWA-protocol frame sender for manual testing of HyperSerialESP32
devices (e.g. the M5Atom internal LED matrix feature).

NOTE: the frame trailer's third checksum byte uses a position counter that
is uint8_t on the device, so it WRAPS at 256 bytes -- keep the (pos & 0xFF).

Usage:
  python3 test/awa_serial_test.py /dev/cu.usbserial-XXXX [mode] [leds]

Modes:
  top       - top quarter of an 8x32 layout lit green (2D test, default)
  bottom    - bottom quarter lit green
  center    - only the middle matrix region lit green (cell 12)
  rainbow   - animated hue sweep across the strip
  stats     - request and print the device statistics

`leds` must match the device's configured LED count (default 256 = 8x32).
"""

import sys
import time

import serial


def fletcher(data):
    s1 = s2 = s3 = pos = 0
    for b in data:
        s1 = (s1 + b) % 255
        s2 = (s2 + s1) % 255
        s3 = (s3 + (b ^ (pos & 0xFF))) % 255  # uint8_t wrap on the device!
        pos += 1
    return s1, s2, (0xAA if s3 == 0x41 else s3)


def frame(pixel_fn, count):
    cnt = count - 1
    hdr = bytearray(b"Awa")
    hdr.append((cnt >> 8) & 0xFF)
    hdr.append(cnt & 0xFF)
    hdr.append(((cnt >> 8) & 0xFF) ^ (cnt & 0xFF) ^ 0x55)
    body = bytearray()
    for i in range(count):
        body += bytes(pixel_fn(i))
    f1, f2, f3 = fletcher(body)
    body += bytes([f1, f2, f3])
    return bytes(hdr + body)


def fold_2d(i, width=8):
    """linear strip index -> (col, row) of a row-wise serpentine layout"""
    row = i // width
    col = i % width
    if row & 1:
        col = width - 1 - col
    return col, row


def hsv(h):
    h = h % 360
    x = int(255 * (1 - abs((h / 60) % 2 - 1)))
    if h < 60:
        return (255, x, 0)
    if h < 120:
        return (x, 255, 0)
    if h < 180:
        return (0, 255, x)
    if h < 240:
        return (0, x, 255)
    if h < 300:
        return (x, 0, 255)
    return (255, 0, x)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    port = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "top"
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 256

    s = serial.Serial(port, 1500000, timeout=0.1)
    s.setDTR(False)
    s.setRTS(False)
    time.sleep(1.5)

    if mode == "stats":
        s.write(b"Awa\x2a\xa2\x15")
        time.sleep(1)
        print(s.read(65536).decode(errors="replace"))
        s.close()
        return

    dim = (12, 0, 0)

    if mode == "top":
        pix = lambda i: (0, 150, 0) if fold_2d(i)[1] <= count // 4 else dim
    elif mode == "bottom":
        pix = lambda i: (0, 150, 0) if fold_2d(i)[1] >= 3 * count // 4 else dim
    elif mode == "center":
        # with 25 matrix cells, region 12 is the center cell
        pix = lambda i: (0, 150, 0) if (i * 25) // count == 12 else dim
    elif mode == "rainbow":
        t0 = time.time()
        pix = lambda i: hsv(360 * ((i / count) + (time.time() - t0) / 4))
    else:
        print(f"unknown mode: {mode}")
        s.close()
        return

    print(f"streaming '{mode}' to {port} ({count} LEDs) - Ctrl+C to stop")
    try:
        while True:
            s.write(frame(pix, count))
            time.sleep(0.02)
    except KeyboardInterrupt:
        pass
    s.close()


if __name__ == "__main__":
    main()
