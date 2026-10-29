"""Verify BP/Requiem transient-subtitle ownership in an initialized PE offline.

Never launches or attaches. Requires pefile/capstone; --research-tools may point
at the existing local wheel directory. Reads each exact-build manifest's map.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--game", required=True, choices=("black_plague", "requiem"))
    parser.add_argument("--research-tools", type=Path)
    args = parser.parse_args()
    if args.research_tools:
        sys.path.insert(0, str(args.research_tools))
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32

    build = {"black_plague": "FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF",
             "requiem": "B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2"}[args.game]
    manifest = Path(__file__).resolve().parents[1] / "manifests" / args.game / (build + ".json")
    mapping = json.loads(manifest.read_text())["transientSubtitleReadability"]
    raw = args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != mapping["sourceImageSha256"]:
        raise ValueError("Not the recorded initialized image for this game/build")
    pe = pefile.PE(data=raw)
    if pe.FILE_HEADER.Machine != 0x14C or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
        raise ValueError("Native x86 image/base mismatch")
    image = pe.get_memory_mapped_image()
    cs = Cs(CS_ARCH_X86, CS_MODE_32)

    def at(value):
        return int(value, 16)

    def check(condition, message):
        if not condition:
            raise ValueError(message)

    def call_target(site):
        check(image[site] == 0xE8, f"Not a direct call at {site:X}")
        return site + 5 + struct.unpack_from("<i", image, site + 1)[0]

    decoded = {}
    for name, window in mapping["windows"].items():
        start, size = at(window["rva"]), window["size"]
        block = image[start:start + size]
        check(hashlib.sha256(block).hexdigest().upper() == window["sha256"],
              f"Native {name} window mismatch")
        decoded[name] = list(cs.disasm(block, start))
    word_wrap = at(mapping["wordWrapRva"])
    sites = [i.address for i in decoded["subtitleDraw"]
             if i.mnemonic == "call" and i.op_str == hex(word_wrap)]
    check(sites == [at(s) for s in mapping["wordWrapCallsites"]],
          "Subtitle ordinary WordWrap caller ownership mismatch")
    # Decode native by-value position stores, including target-specific depth;
    # no coordinate inference from the reported visual symptom.
    for site, offset, depth in zip(sites, mapping["shadowOffsetsPixels"], mapping["nativeDepthLayers"]):
        x, y = mapping["nativeOriginPixels"]
        payload = (b"\xC7\x00" + struct.pack("<f", x + offset[0]) +
                   b"\xC7\x40\x04" + struct.pack("<f", y + offset[1]) +
                   b"\xC7\x40\x08" + struct.pack("<f", depth))
        check(image[site - 27:site - 7] == payload, "Subtitle native position/depth mismatch")
        check(call_target(site) == word_wrap, "Subtitle renderer target mismatch")
    signature = mapping["ordinaryDrawSignature"]
    encoded = bytes.fromhex(signature["bytes"])
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    section = image[text.VirtualAddress:text.VirtualAddress + text.Misc_VirtualSize]
    start = at(signature["rva"])
    check(image[start:start + len(encoded)] == encoded and section.count(encoded) == 1,
          "Ordinary draw signature is not unique in .text")
    producer = mapping["addSubTitleTrans"]
    registration = at(producer["registrationRva"])
    string_rva = struct.unpack_from("<I", image, registration + 1)[0] - 0x400000
    script_name = b" AddSubTitleTrans (\0"
    check(image[registration] == 0x68 and image[string_rva:string_rva + len(script_name)] == script_name,
          "Script registration name mismatch")
    check(struct.unpack_from("<I", image, at(producer["wrapperPointerRva"]))[0] ==
          0x400000 + at(producer["registeredWrapperRva"]), "Registered script wrapper mismatch")
    check(call_target(at(producer["nativeAddCallsite"])) == at(producer["nativeAddRva"]),
          "Script wrapper does not call the mapped subtitle Add")
    check(call_target(at(producer["postSceneDrawCallsite"])) == at(mapping["drawRva"]),
          "EffectHandler draw fan-out ownership mismatch")
    for family, callers in mapping["unmodifiedWordWrapCallsites"].items():
        for caller in callers:
            check(at(caller) not in sites and call_target(at(caller)) == word_wrap,
                  f"{family} exclusion mismatch")
    print(json.dumps({"verified": True, "game": args.game, "validation": "offline exact-image only",
                      "sourceImageSha256": mapping["sourceImageSha256"],
                      "subtitleWordWrapCallsites": mapping["wordWrapCallsites"],
                      "nativeOriginPixels": mapping["nativeOriginPixels"],
                      "nativeDepthLayers": mapping["nativeDepthLayers"]}, indent=2))


if __name__ == "__main__":
    main()
