"""Listen for HA Button advertisements without pairing or connecting."""
import argparse
import asyncio
import json
import time
from pathlib import Path

from bleak import BleakScanner

UUID = "0000fcd2-0000-1000-8000-00805f9b34fb"


async def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("seconds", nargs="?", type=float, default=35)
    parser.add_argument("--address", help="Optional Bluetooth address to match instead of the device name")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("seconds must be positive")
    observations = []
    last = None

    def received(device, advertisement):
        nonlocal last
        if args.address:
            if device.address.casefold() != args.address.casefold():
                return
        elif advertisement.local_name not in {"HA Button21014", "HA Button 21014"}:
            return
        payload = advertisement.service_data.get(UUID)
        if payload is None:
            return
        row = {"time": time.time(), "address": device.address,
               "rssi": advertisement.rssi, "service_data": payload.hex()}
        if len(payload) == 8 and payload[1] == 0 and payload[3] == 0x0c and payload[6] == 0x3a:
            row["packet_id"] = payload[2]
            row["voltage_v"] = int.from_bytes(payload[4:6], "little") / 1000
            row["event"] = {0: "none", 1: "press", 2: "double_press",
                            4: "long_press", 128: "hold_press"}.get(payload[7], "unknown")
        observations.append(row)
        if payload != last:
            print(json.dumps(row), flush=True)
            last = payload

    async with BleakScanner(detection_callback=received, scanning_mode="passive"):
        print("Listening for HA Button advertisements...", flush=True)
        await asyncio.sleep(args.seconds)
    output = Path(__file__).resolve().parents[1] / "firmware" / f"ble-observations-{time.time_ns()}.json"
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(observations, indent=2), encoding="utf-8")
    print(f"Received {len(observations)} advertisements; saved to {output}")
    return 0 if observations else 1


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
