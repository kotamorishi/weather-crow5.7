#!/usr/bin/env python3
"""
Merge ESP32-S3 firmware binaries for ESP Web Tools.

This script merges bootloader.bin, partitions.bin, and firmware.bin
into a single merged_firmware.bin file with correct memory offsets.

ESP32-S3 Memory Map:
- 0x0000: bootloader.bin
- 0x8000: partitions.bin
- 0x10000: firmware.bin

Usage:
    python tools/merge_firmware.py
"""

import os
import sys

# ESP32-S3 memory offsets
BOOTLOADER_OFFSET = 0x0000
PARTITIONS_OFFSET = 0x8000
FIRMWARE_OFFSET = 0x10000

# Paths
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(PROJECT_ROOT, ".pio", "build", "esp32-s3")
DOCS_DIR = os.path.join(PROJECT_ROOT, "docs")

# Input files
BOOTLOADER_BIN = os.path.join(BUILD_DIR, "bootloader.bin")
PARTITIONS_BIN = os.path.join(BUILD_DIR, "partitions.bin")
FIRMWARE_BIN = os.path.join(BUILD_DIR, "firmware.bin")

# Output file
OUTPUT_BIN = os.path.join(DOCS_DIR, "firmware_merged.bin")


def merge_binaries():
    """Merge all binary files into a single firmware file."""

    # Verify input files exist
    for path, name in [
        (BOOTLOADER_BIN, "bootloader.bin"),
        (PARTITIONS_BIN, "partitions.bin"),
        (FIRMWARE_BIN, "firmware.bin"),
    ]:
        if not os.path.exists(path):
            print(f"Error: {name} not found at {path}")
            print("Please run 'pio run' first to build the firmware.")
            sys.exit(1)

    # Create docs directory if it doesn't exist
    os.makedirs(DOCS_DIR, exist_ok=True)

    # Read all binary files
    with open(BOOTLOADER_BIN, "rb") as f:
        bootloader = f.read()

    with open(PARTITIONS_BIN, "rb") as f:
        partitions = f.read()

    with open(FIRMWARE_BIN, "rb") as f:
        firmware = f.read()

    # Calculate total size (firmware offset + firmware size)
    total_size = FIRMWARE_OFFSET + len(firmware)

    # Create merged binary with padding
    merged = bytearray(b'\xFF' * total_size)

    # Place bootloader at offset 0x0
    merged[BOOTLOADER_OFFSET:BOOTLOADER_OFFSET + len(bootloader)] = bootloader

    # Place partitions at offset 0x8000
    merged[PARTITIONS_OFFSET:PARTITIONS_OFFSET + len(partitions)] = partitions

    # Place firmware at offset 0x10000
    merged[FIRMWARE_OFFSET:FIRMWARE_OFFSET + len(firmware)] = firmware

    # Write merged binary
    with open(OUTPUT_BIN, "wb") as f:
        f.write(merged)

    # Print summary
    print("=" * 60)
    print("ESP32-S3 Firmware Merge Complete")
    print("=" * 60)
    print(f"Bootloader:  {len(bootloader):>10,} bytes @ 0x{BOOTLOADER_OFFSET:04X}")
    print(f"Partitions:  {len(partitions):>10,} bytes @ 0x{PARTITIONS_OFFSET:04X}")
    print(f"Firmware:    {len(firmware):>10,} bytes @ 0x{FIRMWARE_OFFSET:04X}")
    print("-" * 60)
    print(f"Merged file: {len(merged):>10,} bytes")
    print(f"Output:      {OUTPUT_BIN}")
    print("=" * 60)


if __name__ == "__main__":
    merge_binaries()
