#!/usr/bin/env python3
"""Smoke test helper for CANable devices running SLCAN firmware."""

from __future__ import annotations

import argparse
import sys
import time


BITRATE_TO_SLCAN = {
    10000: "S0",
    20000: "S1",
    50000: "S2",
    100000: "S3",
    125000: "S4",
    250000: "S5",
    500000: "S6",
    800000: "S7",
    1000000: "S8",
}


def load_serial():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("pyserial is not installed. Install it with: python -m pip install pyserial")
        return None, None

    return serial, list_ports


def list_serial_ports(list_ports) -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return

    for port in ports:
        print(f"{port.device:8} {port.description} {port.hwid}")


def slcan_write(ser, command: str, pause: float = 0.08) -> bytes:
    ser.write((command + "\r").encode("ascii"))
    ser.flush()
    time.sleep(pause)
    waiting = getattr(ser, "in_waiting", 0)
    return ser.read(waiting) if waiting else b""


def slcan_frame(can_id: int, data: bytes) -> str:
    if not 0 <= can_id <= 0x7FF:
        raise ValueError("Only 11-bit standard IDs are supported by this smoke test.")
    if len(data) > 8:
        raise ValueError("CAN data length must be 0..8 bytes.")
    return f"t{can_id:03X}{len(data):X}{data.hex().upper()}"


def run(args: argparse.Namespace) -> int:
    serial, list_ports = load_serial()
    if serial is None:
        return 2

    if args.list:
        list_serial_ports(list_ports)
        return 0

    if not args.port:
        print("Missing --port. Use --list first, then pass the CANable COM port.")
        return 2

    bitrate_cmd = BITRATE_TO_SLCAN.get(args.bitrate)
    if bitrate_cmd is None:
        print(f"Unsupported bitrate: {args.bitrate}")
        return 2

    status_payload = bytes([args.speed & 0xFF, args.light & 0xFF, 1 if args.lamp else 0])
    deadline = time.monotonic() + args.duration

    with serial.Serial(args.port, args.serial_baud, timeout=0.1) as ser:
        print(f"Opening {args.port} as SLCAN, CAN bitrate={args.bitrate}")
        slcan_write(ser, "C")
        slcan_write(ser, bitrate_cmd)
        slcan_write(ser, "O")

        print("Sending heartbeat 0x100 and vehicle status 0x123.")
        print("Press KEY0 on F407 during this script to capture 0x200 TX frames.")

        next_tx = 0.0
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_tx:
                slcan_write(ser, slcan_frame(0x100, b"\x01"), 0.02)
                slcan_write(ser, slcan_frame(0x123, status_payload), 0.02)
                next_tx = now + 0.2

            line = ser.readline()
            if line:
                text = line.decode("ascii", errors="replace").strip()
                if text:
                    print(f"RX raw: {text}")

        slcan_write(ser, "C")

    print("Smoke test finished.")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="List serial ports and exit.")
    parser.add_argument("--port", help="CANable serial port, for example COM5.")
    parser.add_argument("--duration", type=float, default=10.0, help="Test duration in seconds.")
    parser.add_argument("--bitrate", type=int, default=500000, help="CAN bitrate. Default: 500000.")
    parser.add_argument("--serial-baud", type=int, default=115200, help="SLCAN serial baud. Default: 115200.")
    parser.add_argument("--speed", type=int, default=60, help="Vehicle speed byte. Default: 60.")
    parser.add_argument("--light", type=int, default=80, help="Ambient light byte. Default: 80.")
    parser.add_argument("--lamp", type=int, default=1, help="Lamp flag. Default: 1.")
    return parser.parse_args(argv)


if __name__ == "__main__":
    raise SystemExit(run(parse_args(sys.argv[1:])))
