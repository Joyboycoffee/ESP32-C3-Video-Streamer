#!/usr/bin/env python3
"""
ESP32-C3 Video Streamer (Desktop GUI & CLI)
Supports both ST7789 240x240 IPS and SSD1306 128x64 OLED over UDP and WebSocket.

Can be run from terminal with arguments, or double-clicked as a Windows GUI application!

Author: Gourav Kumar
License: MIT
"""

import sys
import os
import time
import socket
import threading
import argparse
import cv2
import numpy as np
from PIL import Image, ImageOps

DEFAULT_PORT = 4210

def encode_frame_oled(bgr, mode="dither", fit="crop"):
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
    packed = np.packbits(pixels.reshape(8, 8, WIDTH).transpose(0, 2, 1), axis=2, bitorder="little")
    return packed.tobytes()

def encode_frame_st7789_native(bgr, mode="dither", fit="crop"):
    WIDTH, HEIGHT = 240, 240
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    h, w = gray.shape

    if fit == "crop":
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

    packed = np.packbits(pixels, axis=1, bitorder="big")
    raw = packed.tobytes()

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

class StreamEngine:
    def __init__(self):
        self.running = False
        self.thread = None

    def start(self, video_path, ip, port=4210, fps=15.0, display="st7789-native", mode="dither", fit="crop", callback=None):
        if self.running:
            return
        self.running = True
        self.thread = threading.Thread(
            target=self._run,
            args=(video_path, ip, port, fps, display, mode, fit, callback),
            daemon=True
        )
        self.thread.start()

    def stop(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=1.0)
            self.thread = None

    def _run(self, video_path, ip, port, fps, display, mode, fit, callback):
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            if callback: callback("ERROR", f"Cannot open video: {video_path}")
            self.running = False
            return

        source_fps = cap.get(cv2.CAP_PROP_FPS)
        if source_fps <= 0 or not np.isfinite(source_fps):
            source_fps = 30.0

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        source_index = 0
        sent = 0
        started = time.perf_counter()

        if callback: callback("STARTED", f"Streaming to {ip}:{port} @ {fps} FPS")

        try:
            while self.running:
                ok, frame = cap.read()
                if not ok:
                    # Loop video
                    cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                    source_index = 0
                    continue

                timestamp = source_index / source_fps
                source_index += 1

                if timestamp + 1e-8 < sent / fps:
                    continue

                target = started + sent / fps
                delay = target - time.perf_counter()
                if delay > 0:
                    time.sleep(delay)

                if mode == "color" or display == "st7789-color":
                    chunks = encode_frame_jpeg(frame, fit, quality=65)
                    seq_bytes = (sent & 0xFFFF).to_bytes(2, "big")
                    for chunk_idx, total_chunks, chunk_data in chunks:
                        header = b"JPG0" + bytes([chunk_idx, total_chunks]) + seq_bytes
                        sock.sendto(header + chunk_data, (ip, port))
                elif display in ["oled", "st7789", "st7789-hud"]:
                    payload = b"VID0" + encode_frame_oled(frame, mode, fit)
                    sock.sendto(payload, (ip, port))
                else:
                    chunks = encode_frame_st7789_native(frame, mode, fit)
                    seq_bytes = (sent & 0xFFFF).to_bytes(2, "big")
                    for chunk_idx, chunk_data in enumerate(chunks):
                        header = b"V240" + bytes([chunk_idx, 6]) + seq_bytes
                        sock.sendto(header + chunk_data, (ip, port))

                sent += 1
                if sent % 15 == 0 and callback:
                    el = time.perf_counter() - started
                    act_fps = sent / el if el > 0 else 0
                    callback("PROGRESS", f"Frames: {sent} | Actual FPS: {act_fps:.1f}")

        except Exception as e:
            if callback: callback("ERROR", str(e))
        finally:
            cap.release()
            sock.close()
            self.running = False
            if callback: callback("STOPPED", f"Finished. Sent {sent} frames.")

# =========================================================================
# TKINTER GRAPHICAL USER INTERFACE
# =========================================================================
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    engine = StreamEngine()

    root = tk.Tk()
    root.title("ESP32-C3 Video Streamer // Deck")
    root.geometry("520x460")
    root.resizable(False, False)
    root.configure(bg="#0B0D10")

    style = ttk.Style()
    style.theme_use("clam")

    # Dark Theme Colors
    BG = "#0B0D10"
    SURFACE = "#13171D"
    CYAN = "#00F0FF"
    TEXT = "#F0F4F8"
    MUTED = "#8E9BAE"

    # Header
    header = tk.Frame(root, bg=SURFACE, height=50)
    header.pack(fill="x", padx=14, pady=10)
    lbl_brand = tk.Label(header, text=" ESP32-C3 ", bg=CYAN, fg="#000", font=("Segoe UI", 9, "bold"))
    lbl_brand.pack(side="left", padx=8, pady=8)
    lbl_title = tk.Label(header, text="WIRELESS VIDEO STREAMER", bg=SURFACE, fg=TEXT, font=("Segoe UI", 11, "bold"))
    lbl_title.pack(side="left", padx=6)

    # Main Card
    card = tk.Frame(root, bg=SURFACE, bd=1, relief="solid")
    card.pack(fill="both", expand=True, padx=14, pady=(0, 10))

    # Video File Row
    row_file = tk.Frame(card, bg=SURFACE)
    row_file.pack(fill="x", padx=14, pady=10)
    tk.Label(row_file, text="VIDEO FILE:", bg=SURFACE, fg=MUTED, font=("Segoe UI", 8, "bold"), width=12, anchor="w").pack(side="left")
    ent_file = tk.Entry(row_file, bg=BG, fg=TEXT, insertbackground=CYAN, font=("Segoe UI", 9))
    ent_file.pack(side="left", fill="x", expand=True, padx=6)

    def browse_file():
        fn = filedialog.askopenfilename(filetypes=[("Video Files", "*.mp4 *.webm *.avi *.mkv *.gif"), ("All Files", "*.*")])
        if fn:
            ent_file.delete(0, tk.END)
            ent_file.insert(0, fn)

    btn_browse = tk.Button(row_file, text="BROWSE", bg=CYAN, fg="#000", font=("Segoe UI", 8, "bold"), command=browse_file, relief="flat", padx=8)
    btn_browse.pack(side="right")

    # IP Address Row
    row_ip = tk.Frame(card, bg=SURFACE)
    row_ip.pack(fill="x", padx=14, pady=6)
    tk.Label(row_ip, text="ESP32 IP:", bg=SURFACE, fg=MUTED, font=("Segoe UI", 8, "bold"), width=12, anchor="w").pack(side="left")
    ent_ip = tk.Entry(row_ip, bg=BG, fg=TEXT, insertbackground=CYAN, font=("Segoe UI", 9))
    ent_ip.insert(0, "192.168.4.1")
    ent_ip.pack(side="left", fill="x", expand=True, padx=6)

    # Display & Mode Selectors
    row_opts = tk.Frame(card, bg=SURFACE)
    row_opts.pack(fill="x", padx=14, pady=8)

    tk.Label(row_opts, text="DISPLAY:", bg=SURFACE, fg=MUTED, font=("Segoe UI", 8, "bold")).grid(row=0, column=0, sticky="w", pady=4)
    cmb_disp = ttk.Combobox(row_opts, values=["ST7789 240x240 (Full)", "ST7789 240x240 (HUD)", "SSD1306 128x64 OLED"], width=20, state="readonly")
    cmb_disp.current(0)
    cmb_disp.grid(row=0, column=1, padx=6, pady=4)

    tk.Label(row_opts, text="FPS:", bg=SURFACE, fg=MUTED, font=("Segoe UI", 8, "bold")).grid(row=0, column=2, sticky="w", pady=4)
    spn_fps = ttk.Spinbox(row_opts, from_=5, to=30, width=5)
    spn_fps.set(15)
    spn_fps.grid(row=0, column=3, padx=6, pady=4)

    tk.Label(row_opts, text="RENDER:", bg=SURFACE, fg=MUTED, font=("Segoe UI", 8, "bold")).grid(row=1, column=0, sticky="w", pady=4)
    cmb_mode = ttk.Combobox(row_opts, values=["Full Color (Motion-JPEG)", "Binary Threshold", "Floyd-Steinberg Dither"], width=20, state="readonly")
    cmb_mode.current(0)
    cmb_mode.grid(row=1, column=1, padx=6, pady=4)

    tk.Label(row_opts, text="FIT:", bg=SURFACE, fg=MUTED, font=("Segoe UI", 8, "bold")).grid(row=1, column=2, sticky="w", pady=4)
    cmb_fit = ttk.Combobox(row_opts, values=["Crop (Fill)", "Letterbox (Fit)"], width=10, state="readonly")
    cmb_fit.current(0)
    cmb_fit.grid(row=1, column=3, padx=6, pady=4)

    # Status Bar
    lbl_status = tk.Label(card, text="READY. Select video & hit Start Stream.", bg=BG, fg=CYAN, font=("Segoe UI", 9), height=2)
    lbl_status.pack(fill="x", padx=14, pady=12)

    # Action Button
    btn_stream = tk.Button(card, text="▶ START STREAMING", bg=CYAN, fg="#000", font=("Segoe UI", 11, "bold"), relief="flat", pady=8)
    btn_stream.pack(fill="x", padx=14, pady=(0, 14))

    def on_engine_callback(status, msg):
        if status == "PROGRESS":
            lbl_status.config(text=msg, fg="#00FF66")
        elif status == "STARTED":
            lbl_status.config(text=msg, fg=CYAN)
            btn_stream.config(text="⏹ STOP STREAMING", bg="#FF3366", fg="#fff")
        elif status == "STOPPED":
            lbl_status.config(text=msg, fg=TEXT)
            btn_stream.config(text="▶ START STREAMING", bg=CYAN, fg="#000")
        elif status == "ERROR":
            lbl_status.config(text=f"ERROR: {msg}", fg="#FF3366")
            btn_stream.config(text="▶ START STREAMING", bg=CYAN, fg="#000")

    def toggle_stream():
        if engine.running:
            engine.stop()
        else:
            fn = ent_file.get().strip()
            ip = ent_ip.get().strip()
            if not fn or not os.path.exists(fn):
                messagebox.showerror("Error", "Please select a valid video file first!")
                return
            if not ip:
                messagebox.showerror("Error", "Please enter the ESP32 IP address!")
                return

            disp_map = {
                "ST7789 240x240 (Full)": "st7789-native",
                "ST7789 240x240 (HUD)": "st7789-hud",
                "SSD1306 128x64 OLED": "oled"
            }
            d_val = disp_map.get(cmb_disp.get(), "st7789-native")
            if "Color" in cmb_mode.get():
                m_val = "color"
            elif "Dither" in cmb_mode.get():
                m_val = "dither"
            else:
                m_val = "threshold"
            f_val = "crop" if "Crop" in cmb_fit.get() else "letterbox"
            try:
                fps_val = float(spn_fps.get())
            except:
                fps_val = 15.0

            engine.start(fn, ip, 4210, fps_val, d_val, m_val, f_val, callback=on_engine_callback)

    btn_stream.config(command=toggle_stream)

    def on_closing():
        if engine.running:
            engine.stop()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

# =========================================================================
# CLI ENTRY POINT
# =========================================================================
def main():
    if len(sys.argv) == 1:
        # No CLI arguments provided -> launch modern GUI
        run_gui()
    else:
        # CLI Mode
        parser = argparse.ArgumentParser(description="Stream video to ESP32-C3")
        parser.add_argument("video", help="Path to video file")
        parser.add_argument("--ip", required=True, help="ESP32-C3 IP address")
        parser.add_argument("--port", type=int, default=4210, help="UDP port (default: 4210)")
        parser.add_argument("--fps", type=float, default=15.0, help="Output FPS (default: 15)")
        parser.add_argument("--display", choices=["oled", "st7789", "st7789-native", "st7789-hud"], default="st7789-native")
        parser.add_argument("--mode", choices=["dither", "threshold"], default="dither")
        parser.add_argument("--fit", choices=["crop", "letterbox"], default="crop")
        args = parser.parse_args()

        engine = StreamEngine()
        def cli_cb(st, msg):
            print(f"[{st}] {msg}")
        engine.start(args.video, args.ip, args.port, args.fps, args.display, args.mode, args.fit, cli_cb)
        try:
            while engine.running:
                time.sleep(0.5)
        except KeyboardInterrupt:
            engine.stop()

if __name__ == "__main__":
    main()
