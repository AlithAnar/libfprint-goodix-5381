#!/usr/bin/env python3
"""
Proof-of-Concept USB handshake for Goodix 27c6:5381 (Dell G5 15 5587).
Performs CDC-ACM line setup on Interface 0 and replays the initial
register probe/configuration frames on Bulk endpoints 0x03 / 0x81.
"""

import time
import usb.core
import usb.util

VENDOR_ID = 0x27C6
PRODUCT_ID = 0x5381

EP_OUT = 0x03
EP_IN = 0x81
PACKET_SIZE = 64
TIMEOUT_MS = 1000

# Frame 38: Initial host probe (command 0xa7, zero-padded to 64 bytes)
CMD_PROBE_38 = bytes.fromhex(
    "0003000000a700000000000000000000"
    "00000000000000000000000000000000"
    "00000000000000000000000000000000"
    "00000000000000000000000000000000"
)

# Frame 42: Initial register/interrupt mask configuration (64 bytes)
CMD_INIT_CFG_42 = bytes.fromhex(
    "a003000000ffff0000ffffffffffffff"
    "ff7fffff000000000000000000000000"
    "00000000fffffff7ffffff0000000004"
    "00000000000000010000000000000000"
)


def main() -> None:
    dev = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)
    if dev is None:
        raise RuntimeError(f"Sensor {VENDOR_ID:04x}:{PRODUCT_ID:04x} not found.")

    # Fallback detach in case udev rule has not triggered yet
    for iface in (0, 1):
        if dev.is_kernel_driver_active(iface):
            try:
                dev.detach_kernel_driver(iface)
            except usb.core.USBError:
                pass

    # 1. Configure CDC-ACM serial parameters on Interface 0 (115200 baud, 8N1)
    line_coding = bytes([0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08])
    dev.ctrl_transfer(0x21, 0x20, 0x0000, 0, line_coding, timeout=TIMEOUT_MS)

    # 2. Assert DTR | RTS (0x0003) to unlock MCU bulk RX buffer
    dev.ctrl_transfer(0x21, 0x22, 0x0003, 0, None, timeout=TIMEOUT_MS)
    time.sleep(0.05)

    # 3. Claim CDC Data interface (Interface 1)
    usb.util.claim_interface(dev, 1)

    try:
        # Step 1: Send Frame 38 -> Read Frame 40
        assert len(CMD_PROBE_38) == PACKET_SIZE
        dev.write(EP_OUT, CMD_PROBE_38, timeout=TIMEOUT_MS)
        resp_40 = bytes(dev.read(EP_IN, PACKET_SIZE, timeout=TIMEOUT_MS))
        print(f"[Frame 40] ({len(resp_40)}B): {resp_40.hex()}")

        # Step 2: Send Frame 42 -> Read Frame 44 (+ optional Frame 46)
        assert len(CMD_INIT_CFG_42) == PACKET_SIZE
        dev.write(EP_OUT, CMD_INIT_CFG_42, timeout=TIMEOUT_MS)
        resp_44 = bytes(dev.read(EP_IN, PACKET_SIZE, timeout=TIMEOUT_MS))
        print(f"[Frame 44] ({len(resp_44)}B): {resp_44.hex()}")

        try:
            resp_46 = bytes(dev.read(EP_IN, PACKET_SIZE, timeout=300))
            print(f"[Frame 46] ({len(resp_46)}B): {resp_46.hex()}")
        except usb.core.USBTimeoutError:
            print("[Frame 46] No second packet queued.")

    finally:
        usb.util.release_interface(dev, 1)


if __name__ == "__main__":
    main()
