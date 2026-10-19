#!/usr/bin/env python3
"""
ESP32-C3 Video Streamer - UDP Network Connectivity Test
Sends alternating black & white test frames over UDP to verify port 4210 reachability.

Usage:
  py test_send.py --ip <ESP32_IP>
"""

import argparse
import socket
import time

DEFAULT_PORT = 4210
FRAME_SIZE = 1024  # 128x64 SSD1306 / VID0 payload

def main():
    parser = argparse.ArgumentParser(description="Test UDP video packet delivery to ESP32-C3")
    parser.add_argument("--ip", required=True, help="ESP32-C3 IP address shown in Serial Monitor")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port (default: 4210)")
    parser.add_argument("--count", type=int, default=30, help="Number of test frames (default: 30)")
    args = parser.parse_args()

    print("=" * 60)
    print("  ESP32-C3 UDP PACKET CONNECTIVITY TEST ")
    print("=" * 60)
    print(f"Target: {args.ip}:{args.port}")
    print(f"Sending {args.count} alternating black/white frames...")
    print("-" * 60)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        for i in range(args.count):
            pattern = 0xFF if (i % 2 == 1) else 0x00
            color_name = "WHITE" if (i % 2 == 1) else "BLACK"
            
            frame_data = bytes([pattern]) * FRAME_SIZE
            payload = b"VID0" + frame_data
            
            sock.sendto(payload, (args.ip, args.port))
            print(f"[{i+1}/{args.count}] Sent {color_name} frame (1028 bytes)")
            time.sleep(0.35)
            
    except KeyboardInterrupt:
        print("\nTest cancelled by user.")
    finally:
        sock.close()
        
    print("-" * 60)
    print("Test finished! If your display flashed between black and white,")
    print("your network path and UDP port 4210 are fully working!")

if __name__ == "__main__":
    main()
