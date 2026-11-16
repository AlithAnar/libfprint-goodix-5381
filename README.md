# libfprint-goodix-5381
Reverse-engineered USB protocol and experimental Linux libfprint driver for the Goodix 27c6:5381 fingerprint sensor (Dell G5 15 5587).

> **Status: Work in Progress (Protocol Analysis & Prototyping)**  
> Initial USB CDC-ACM handshake and register communication have been successfully reproduced on Linux. Full image frame capture and `libfprint` C driver integration are currently under development.

An experimental Linux `libfprint` driver and protocol documentation for the **Shenzhen Goodix Technology Co., Ltd. (`27c6:5381`)** fingerprint sensor, commonly found in the **Dell G5 15 5587** and related 2018 Dell laptops.

---

## Technical Findings & Protocol Architecture

Unlike newer Goodix sensors supported by community `goodixtls` forks (e.g., `538d`, `5395`, `55b4`), USB traffic analysis confirms that the **`27c6:5381` does not use TLS encryption**:

1. **USB CDC-ACM Interface:**
   * The device exposes a standard USB Communications Device Class (CDC-ACM) layout (`Interface 0`: Control, `Interface 1`: Bulk Data).
   * Linux automatically binds the `cdc_acm` kernel module to the sensor, blocking user-space `libusb`/`libfprint` access (`Resource busy`). Unbinding `cdc_acm` via a `udev` rule is required.
   * Before bulk transfers are accepted by the sensor's microcontroller, the host must issue standard CDC control requests on `Interface 0`: `SET_LINE_CODING` (`115200 8N1`) and `SET_CONTROL_LINE_STATE` (`DTR | RTS = 0x0003`).

2. **Endpoints & Packet Framing:**
   * **Bulk OUT:** `0x03` (`wMaxPacketSize = 64` bytes, USB Full-Speed). All outgoing packets must be zero-padded to exact multiples of 64 bytes.
   * **Bulk IN:** `0x81` (`wMaxPacketSize = 64` bytes).
   * **Host Commands:** Prefixed with `00 03 ...` or `a0 03 ...` (e.g., initial probe command `00 03 00 00 00 a7 ...`).
   * **Sensor Responses:** Prefixed with `b0 03 ...`, exposing direct ARM Cortex-M SRAM buffers (`0x20001700`) and MMIO peripheral registers (`0x400a8014`).

3. **Driver Architecture:**
   * Because there is no TLS handshake or cryptographic overhead, the driver can be implemented cleanly on top of upstream `libfprint`'s `FpImageDevice` class—sending the register initialization sequence, waiting for finger-touch interrupts, and passing raw grayscale sensor frames to `libfprint`'s built-in NBIS matcher.

---

## Roadmap

- [x] Identify USB endpoints (`0x03` OUT / `0x81` IN) and Full-Speed 64-byte packet padding requirement
- [x] Bypass `Resource busy` by unbinding `cdc_acm` via `udev`
- [x] Capture Windows driver initialization traffic via Wireshark + USBPcap
- [x] Reproduce CDC control line activation (`DTR`/`RTS`) and initial register handshake in Python (`pyusb`)
- [ ] Capture full finger-enrollment traffic to extract the complete initialization sequence and raw image frame dimensions
- [ ] Implement `goodix5381.c` / `goodix5381.h` (`FpImageDevice`) for upstream `libfprint`
- [ ] Provide automated build script for Ubuntu / Debian-based distributions

---

## Repository Contents

* **`udev/`** — Rules to detach `cdc_acm` from `27c6:5381` upon boot or USB enumeration.
* **`python/`** — Standalone `pyusb` scripts used to replay captured initialization frames and dump raw sensor matrices before porting to C.
* **`driver/`** — C source files designed to be injected into a clean upstream `libfprint` source tree.
* **`scripts/`** — Helper script to download upstream `libfprint`, patch `meson.build`, and compile the custom driver.

---

## Quick Start (USB Unbind & Python PoC)

1. **Install the `udev` rule** so `cdc_acm` does not claim the sensor:
   ```bash
   sudo cp udev/91-goodix-5381-unbind.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```

2. **Run the handshake test** inside a Python virtual environment:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install pyusb
   sudo ./.venv/bin/python3 python/poc_handshake.py
   ```

---

## Acknowledgments

* [goodix-fp-linux-dev](https://github.com/goodix-fp-linux-dev) for foundational research into Goodix USB protocols on Linux.
* [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint) maintainers for the `FpImageDevice` framework.
