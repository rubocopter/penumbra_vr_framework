"""Read-only helper for initialized PE memory images (not packed disk EXEs).

Requires capstone. Search results are candidates, never validated hook sites.
The local capture is deliberately not distributed with this helper.
"""
import argparse
import struct
from pathlib import Path
import capstone

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("image", type=Path)
parser.add_argument("--text", action="append", default=[])
parser.add_argument("--import-name", action="append", default=[])
parser.add_argument("--call-target", action="append", default=[],
                    type=lambda s: int(s, 0),
                    help="report direct calls to this RVA")
parser.add_argument("--memory-disp", action="append", default=[],
                    type=lambda s: int(s, 0),
                    help="report instructions using this memory displacement")
parser.add_argument("--rva", type=lambda s: int(s, 0))
parser.add_argument("--size", type=lambda s: int(s, 0), default=256)
args = parser.parse_args()
data = args.image.read_bytes()
pe = struct.unpack_from("<I", data, 0x3C)[0]
if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0":
    parser.error("not a PE image")
base = struct.unpack_from("<I", data, pe + 52)[0]
if args.import_name:
    directory, size = struct.unpack_from("<II", data, pe + 128)
    def u32(at):
        return struct.unpack_from("<I", data, at)[0]
    def cstring(at):
        end = data.find(b"\0", at)
        if at < 0 or end < at:
            raise ValueError("invalid import string")
        return data[at:end].decode("ascii", errors="replace")
    for descriptor in range(directory, min(directory + size, len(data) - 19), 20):
        names, _, _, dll, addresses = struct.unpack_from("<IIIII", data, descriptor)
        if not dll:
            break
        if not names:
            continue
        for index in range((len(data) - names) // 4):
            name = u32(names + index * 4)
            if not name:
                break
            if name & 0x80000000:
                continue
            function = cstring(name + 2)
            if not any(query.casefold() in function.casefold() for query in args.import_name):
                continue
            slot = addresses + index * 4
            print(f"IMPORT {cstring(dll)}!{function} IAT RVA={slot:08X} VA={base + slot:08X}")
            address = struct.pack("<I", base + slot)
            pos = 0
            while (pos := data.find(address, pos)) >= 0:
                print(f"  REFERENCE candidate RVA={pos:08X} bytes={data[max(0,pos-8):pos+12].hex(' ')}")
                pos += 1
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
md.skipdata = True

# A mapped-image capture stores each section at its virtual address.  Walk only
# executable sections when searching instructions; decoding the whole image as
# one byte stream loses synchronization in headers/data and produces false
# xrefs.
section_count = struct.unpack_from("<H", data, pe + 6)[0]
optional_size = struct.unpack_from("<H", data, pe + 20)[0]
section_table = pe + 24 + optional_size
executable_ranges = []
for index in range(section_count):
    entry = section_table + index * 40
    if entry + 40 > len(data):
        break
    name = data[entry:entry + 8].split(b"\0", 1)[0].decode(
        "ascii", errors="replace")
    virtual_size, virtual_address = struct.unpack_from("<II", data, entry + 8)
    characteristics = struct.unpack_from("<I", data, entry + 36)[0]
    if characteristics & 0x20000000 and virtual_address < len(data):
        end = min(len(data), virtual_address + virtual_size)
        executable_ranges.append((name, virtual_address, end))

if args.call_target or args.memory_disp:
    call_targets = {base + value: value for value in args.call_target}
    memory_disps = set(args.memory_disp)
    for section, start, end in executable_ranges:
        for ins in md.disasm(data[start:end], base + start):
            if ins.id == 0:  # Capstone skip-data pseudo instruction.
                continue
            if (ins.mnemonic == "call" and len(ins.operands) == 1 and
                    ins.operands[0].type == capstone.x86.X86_OP_IMM and
                    ins.operands[0].imm in call_targets):
                target = call_targets[ins.operands[0].imm]
                print(f"CALL target RVA={target:08X} from RVA="
                      f"{ins.address - base:08X} section={section}")
            if memory_disps and any(
                    operand.type == capstone.x86.X86_OP_MEM and
                    operand.mem.disp in memory_disps
                    for operand in ins.operands):
                print(f"MEMORY disp={next(operand.mem.disp for operand in ins.operands if operand.type == capstone.x86.X86_OP_MEM and operand.mem.disp in memory_disps):08X} "
                      f"RVA={ins.address - base:08X} section={section} "
                      f"{ins.mnemonic} {ins.op_str}")
if args.rva is not None:
    if args.rva < 0 or args.size <= 0 or args.rva + args.size > len(data):
        parser.error("range is outside the image")
    for ins in md.disasm(data[args.rva:args.rva + args.size], base + args.rva):
        print(f"{ins.address - base:08X}  {ins.bytes.hex(' '):25s} {ins.mnemonic:8s} {ins.op_str}")
for value in args.text:
    start = 0
    needle = value.encode("ascii") + b"\0"
    while (offset := data.find(needle, start)) >= 0:
        start = offset + 1
        print(f"TEXT {value!r} RVA={offset:08X} VA={base + offset:08X}")
        address = struct.pack("<I", base + offset)
        pos = 0
        while (pos := data.find(address, pos)) >= 0:
            print(f"  ADDRESS REFERENCE candidate RVA={pos:08X} bytes={data[max(0,pos-8):pos+12].hex(' ')}")
            pos += 1
