"""Offline exact-image verification for Requiem's GameMessage font boundary.

Requires pefile/capstone; --research-tools can point at their local wheel folder.
This reads the initialized/unpacked PE only. It never launches or attaches.
"""

import argparse
import hashlib
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--research-tools", type=Path)
    args = parser.parse_args()
    if args.research_tools:
        sys.path.insert(0, str(args.research_tools))
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32

    source_hash = hashlib.sha256(args.image.read_bytes()).hexdigest()
    expected_hash = "989726ee2780ac890486dfca0ab6b80af8e97bc4ff64af5a28cf1f23ace2ca5b"
    if source_hash != expected_hash:
        raise ValueError("Not the independently examined Requiem unpacked image")
    pe = pefile.PE(str(args.image))
    if pe.FILE_HEADER.Machine != 0x14C or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
        raise ValueError("Requiem x86 image/base mismatch")
    image = pe.get_memory_mapped_image()
    windows = {
        "gameMessageDraw": (0x3F500, 0x3F70D,
            "4ce5e42ce888ad23f487b09455873641b0e2d9b92da8f8c8f68f717a4b373bc1"),
        "drawWordWrap": (0xEBB00, 0xEBF3C,
            "3ab0e727156b43b193a43140de04d2b151cadd97434a2a4113318fc01c231fc5"),
        "getLength": (0xEB490, 0xEB4E7,
            "3039d0964e6346e9a7ae8f8bec5b5f5925dd250348eafec0277cc6bcd2dae433"),
    }
    disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
    decoded = {}
    for name, (start, end, digest) in windows.items():
        block = image[start:end]
        if hashlib.sha256(block).hexdigest() != digest:
            raise ValueError(f"Native {name} window mismatch")
        decoded[name] = list(disassembler.disasm(block, start))
    calls = [i.address for i in decoded["gameMessageDraw"]
             if i.mnemonic == "call" and i.op_str == "0xebb00"]
    expected_calls = [0x3F568, 0x3F5D2, 0x3F61D, 0x3F668, 0x3F6B3, 0x3F6FE]
    if calls != expected_calls:
        raise ValueError("GameMessage WordWrap caller ownership mismatch")
    measures = [i.address for i in decoded["drawWordWrap"]
                if i.mnemonic == "call" and i.op_str == "0xeb490"]
    if measures != [0xEBBBE, 0xEBCD6]:
        raise ValueError("DrawWordWrap native measurement boundary mismatch")
    if image[0xEBF39:0xEBF3C] != bytes.fromhex("C2 28 00"):
        raise ValueError("WordWrap thiscall stack cleanup mismatch")
    if image[0xEB4E4:0xEB4E7] != bytes.fromhex("C2 08 00"):
        raise ValueError("GetLength thiscall stack cleanup mismatch")
    if image[0xEB490:0xEB49A] != bytes.fromhex("8B 54 24 08 D9 05 C0 39 67 00"):
        raise ValueError("GetLength entry signature mismatch")
    # Native accesses independently establish VC7 string length, capacity,
    # inline threshold and data field. Never share a foreign CRT allocation.
    for site, encoded in [(0xEBB4F, "8B 45 14"), (0xEBB60, "8B 55 18"),
                          (0xEBB63, "83 FA 08"), (0xEBB66, "8D 45 04")]:
        expected = bytes.fromhex(encoded)
        if image[site:site + len(expected)] != expected:
            raise ValueError(f"VC7 string access mismatch at {site:X}")
    print(json.dumps({
        "verified": True, "validation": "offline exact-image only",
        "unpackedSha256": source_hash.upper(),
        "windows": {name: {"rva": hex(start), "size": end - start,
                           "sha256": digest.upper()}
                    for name, (start, end, digest) in windows.items()},
        "gameMessageFontCalls": [hex(site) for site in calls],
        "nativeMeasurementCalls": [hex(site) for site in measures],
        "legacyWideString": {"data": 4, "length": 20, "capacity": 24,
                             "inlineCapacityBelow": 8},
    }, indent=2))


if __name__ == "__main__":
    main()
