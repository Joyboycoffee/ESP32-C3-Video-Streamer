#!/usr/bin/env python3
"""
ESP32-C3 Video Streamer
Streams any MP4, AVI, or GIF video to an ESP32-C3 driving:
  1) ST7789 240x240 IPS Display (High-Speed SPI)
  2) SSD1306 128x64 Monochrome OLED Display (I2C)

Author: Gourav Kumar
License: MIT
"""

import argparse
import socket
import time
import sys
import cv2
import numpy as np
from PIL import Image, ImageOps

DEFAULT_PORT = 4210

def encode_frame_oled(bgr, mode="dither", fit="crop"):
    """
    Encode frame for SSD1306 (128x64) in page format:
    8 pages tall, 128 columns wide, each byte = 8 vertical pixels (LSB at top).
    Total: 1024 bytes.
    """
    WIDTH, HEIGHT = 128, 64
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    h, w = gray.shape

    if fit == "crop":
        ratio = WIDTH / HEIGHT
        if w / h > ratio:
            new_w = max(1, round(h * ratio))
            x = (w - new_w) // 2
            gray = gray[:, x:x + new_w]
        else:
            new_h = max(1, round(w / ratio))
            y = (h - new_h) // 2
            gray = gray[y:y + new_h, :]
        image = Image.fromarray(gray).resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    else:
        image = ImageOps.contain(Image.fromarray(gray), (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        canvas = Image.new("L", (WIDTH, HEIGHT), 0)
        canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
        image = canvas

    image = ImageOps.autocontrast(image)
    dither = Image.Dither.FLOYDSTEINBERG if mode == "dither" else Image.Dither.NONE
    pixels = np.asarray(image.convert("1", dither=dither), dtype=np.uint8)

    # SSD1306 page format: each byte represents eight VERTICAL pixels
    packed = np.packbits(pixels.reshape(8, 8, WIDTH).transpose(0, 2, 1), axis=2, bitorder="little")
    return packed.tobytes()

def encode_frame_st7789_native(bgr, mode="dither", fit="crop"):
    """
    Encode frame for ST7789 (240x240) native 1-bit scanlines.
    Returns 6 chunks of 1200 bytes each (40 rows x 30 bytes = 1200 bytes).
    Total: 7200 bytes per frame.
    """
    WIDTH, HEIGHT = 240, 240
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    h, w = gray.shape

    if fit == "crop":
        # Center square crop
        dim = min(h, w)
        sy = (h - dim) // 2
        sx = (w - dim) // 2
        gray = gray[sy:sy + dim, sx:sx + dim]
        image = Image.fromarray(gray).resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    else:
        image = ImageOps.contain(Image.fromarray(gray), (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        canvas = Image.new("L", (WIDTH, HEIGHT), 0)
        canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
        image = canvas

    image = ImageOps.autocontrast(image)
    dither = Image.Dither.FLOYDSTEINBERG if mode == "dither" else Image.Dither.NONE
    pixels = np.asarray(image.convert("1", dither=dither), dtype=np.uint8)

    # Pack horizontally (MSB first) -> 30 bytes per row
    packed = np.packbits(pixels, axis=1, bitorder="big")
    raw = packed.tobytes() # 7200 bytes

    # Split into 6 chunks of 40 lines (1200 bytes each)
    chunks = []
    chunk_size = 1200
    for i in range(6):
        chunks.append(raw[i * chunk_size:(i + 1) * chunk_size])
    return chunks

def encode_frame_jpeg(bgr, fit="crop", quality=65):
    WIDTH, HEIGHT = 240, 240
    h, w = bgr.shape[:2]
    if fit == "crop":
        dim = min(h, w)
        sy = (h - dim) // 2
        sx = (w - dim) // 2
        cropped = bgr[sy:sy + dim, sx:sx + dim]
        resized = cv2.resize(cropped, (WIDTH, HEIGHT), interpolation=cv2.INTER_AREA)
    else:
        scale = min(WIDTH / w, HEIGHT / h)
        nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
        res = cv2.resize(bgr, (nw, nh), interpolation=cv2.INTER_AREA)
        resized = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)
        y_off = (HEIGHT - nh) // 2
        x_off = (WIDTH - nw) // 2
        resized[y_off:y_off + nh, x_off:x_off + nw] = res

    ret, jpeg_buf = cv2.imencode('.jpg', resized, [cv2.IMWRITE_JPEG_QUALITY, quality])
    raw = jpeg_buf.tobytes()
    chunk_size = 1200
    total_chunks = (len(raw) + chunk_size - 1) // chunk_size
    chunks = []
    for i in range(total_chunks):
        chunks.append((i, total_chunks, raw[i * chunk_size:(i + 1) * chunk_size]))
    return chunks

def main():
    parser = argparse.ArgumentParser(
        description="Stream MP4/AVI/GIF video to ESP32-C3 (ST7789 IPS or SSD1306 OLED) over Wi-Fi UDP"
    )
    parser.add_argument("video", help="Path to video file (.mp4, .avi, .mkv, .gif)")
    parser.add_argument("--ip", required=True, help="ESP32-C3 IP address shown in Serial Monitor / Display")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port (default: 4210)")
    parser.add_argument("--fps", type=float, default=12.0, help="Output streaming FPS (default: 12)")
    parser.add_argument(
        "--display",
        choices=["st7789-native", "st7789", "oled"],
        default="st7789-native",
        help="Target display: 'st7789-native' (240x240 full screen) or 'oled' (128x64)",
    )
    parser.add_argument(
        "--mode",
        choices=["color", "threshold", "dither"],
        default="color",
        help="Rendering mode: 'color' (Full Color Motion-JPEG, default), 'threshold' (Clean B&W), 'dither' (Retro dots)",
    )
    parser.add_argument("--fit", choices=["crop", "letterbox"], default="crop", help="Aspect ratio fit (default: crop)")
    args = parser.parse_args()

    if args.fps <= 0:
        parser.error("--fps must be greater than zero")

    cap = cv2.VideoCapture(args.video)
    if not cap.isOpened():
        sys.exit(f"[ERROR] Could not open video file: {args.video}")

    source_fps = cap.get(cv2.CAP_PROP_FPS)
    if source_fps <= 0 or not np.isfinite(source_fps):
        source_fps = 30.0

    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    print("=" * 60)
    print("  ESP32-C3 WIRELESS VIDEO STREAMER (UDP) ")
    print("=" * 60)
    print(f"File:       {args.video} ({total_frames} frames)")
    print(f"Source FPS: {source_fps:.2f} | Stream FPS: {args.fps:.2f}")
    print(f"Target:     {args.ip}:{args.port}")
    print(f"Display:    {args.display.upper()} | Mode: {args.mode.upper()} | Fit: {args.fit}")
    print("-" * 60)
    print("Streaming started... Press Ctrl+C to stop.")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    source_index = 0
    sent = 0
    started = time.perf_counter()

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break

            timestamp = source_index / source_fps
            source_index += 1

            # Frame dropping to synchronize with target FPS
            if timestamp + 1e-8 < sent / args.fps:
                continue

            # Accurate sleep / throttle
            target = started + sent / args.fps
            delay = target - time.perf_counter()
            if delay > 0:
                time.sleep(delay)

            # Send according to mode and display
            if args.mode == "color" or args.display == "st7789-color":
                chunks = encode_frame_jpeg(frame, args.fit, quality=65)
                seq_bytes = (sent & 0xFFFF).to_bytes(2, "big")
                for chunk_idx, total_chunks, chunk_data in chunks:
                    header = b"JPG0" + bytes([chunk_idx, total_chunks]) + seq_bytes
                    sock.sendto(header + chunk_data, (args.ip, args.port))
            elif args.display in ["oled", "st7789"]:
                # Standard VID0 protocol (128x64 dithered)
                payload = b"VID0" + encode_frame_oled(frame, args.mode, args.fit)
                sock.sendto(payload, (args.ip, args.port))
            else:
                # Native 240x240 V240 protocol (6 chunks)
                chunks = encode_frame_st7789_native(frame, args.mode, args.fit)
                seq_bytes = (sent & 0xFFFF).to_bytes(2, "big")
                for chunk_idx, chunk_data in enumerate(chunks):
                    # V240 + chunk_idx (1B) + total_chunks (1B) + seq (2B) + data (1200B)
                    header = b"V240" + bytes([chunk_idx, 6]) + seq_bytes
                    sock.sendto(header + chunk_data, (args.ip, args.port))

            sent += 1
            if sent % 30 == 0:
                elapsed = time.perf_counter() - started
                actual_fps = sent / elapsed if elapsed > 0 else 0
                print(f"[STREAM] Sent {sent} frames | Actual FPS: {actual_fps:.1f}")

    except KeyboardInterrupt:
        print("\n[INFO] Streaming stopped by user.")
    finally:
        cap.release()
        sock.close()
        elapsed = time.perf_counter() - started
        print(f"[DONE] Finished. Sent {sent} frames in {elapsed:.1f}s.")

if __name__ == "__main__":
    main()
