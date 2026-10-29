"""Verify BP/RQ Radio narrative callers against an initialized PE offline.

No game execution or attachment. Requires pefile/capstone; --research-tools
can select the existing local wheel directory. --data-root additionally checks
the installed producer scripts, including both Requiem final ending routes.
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
    parser.add_argument("--data-root", type=Path)
    args = parser.parse_args()
    if args.research_tools:
        sys.path.insert(0, str(args.research_tools))
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32

    builds = {"black_plague": "FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF",
              "requiem": "B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2"}
    manifest = Path(__file__).resolve().parents[1] / "manifests" / args.game / (builds[args.game] + ".json")
    evidence = json.loads(manifest.read_text())
    mapping = evidence["narrativeSubtitleReadability"]

    def check(condition, message):
        if not condition:
            raise ValueError(message)

    def at(value):
        return int(value, 16)

    raw = args.image.read_bytes()
    check(hashlib.sha256(raw).hexdigest().upper() == mapping["sourceImageSha256"],
          "Not the recorded initialized image for this game/build")
    pe = pefile.PE(data=raw)
    check(pe.FILE_HEADER.Machine == 0x14C and pe.OPTIONAL_HEADER.ImageBase == 0x400000,
          "Native x86 image/base mismatch")
    image = pe.get_memory_mapped_image()
    cs = Cs(CS_ARCH_X86, CS_MODE_32)

    def call_target(site):
        check(image[site] == 0xE8, f"Not a direct call at {site:X}")
        return site + 5 + struct.unpack_from("<i", image, site + 1)[0]

    def decode_window(window):
        start, size = at(window["rva"]), window["size"]
        block = image[start:start + size]
        check(hashlib.sha256(block).hexdigest().upper() == window["sha256"],
              f"Native window mismatch at {start:X}")
        instructions = list(cs.disasm(block, start))
        check(sum(i.size for i in instructions) == size, "Incomplete instruction window")
        return instructions

    decoded = {name: decode_window(window) for name, window in mapping["windows"].items()}

    def calls_to(window, target):
        return [hex(i.address) for i in decoded[window]
                if i.mnemonic == "call" and i.op_str == target]

    check(calls_to("radioDraw", mapping["wordWrapRva"]) == mapping["wordWrapCallsites"],
          "Radio current/previous WordWrap ownership mismatch")
    check(calls_to("radioDraw", mapping["guiHelperRva"]) == mapping["guiHelperCallsites"],
          "Radio ScreenGui current/previous helper ownership mismatch")
    check(calls_to("radioGuiHelper", mapping["getWordWrapRowsRva"]) ==
          [mapping["getWordWrapRowsCallsite"]], "Radio native reflow ownership mismatch")
    check(calls_to("radioGuiHelper", mapping["guiDrawFontRva"]) == mapping["guiDrawFontCallsites"],
          "Radio GUI text/shadow ownership mismatch")
    for name, pop in (("getWordWrapRows", 0x18), ("guiDrawFont", 0x1C)):
        check(image[at(mapping["abiReturns"][name]):][:3] == b"\xC2" + struct.pack("<H", pop),
              f"Native {name} stack ABI mismatch")
    for site, offset, depth in zip(mapping["wordWrapCallsites"][:3],
                                   mapping["shadowOffsetsPixels"], mapping["nativeDepthLayers"]):
        x, y = mapping["nativeOriginPixels"]
        payload = (b"\xC7\x00" + struct.pack("<f", x + offset[0]) +
                   b"\xC7\x40\x04" + struct.pack("<f", y + offset[1]) +
                   b"\xC7\x40\x08" + struct.pack("<f", depth))
        check(image[at(site) - 23:at(site) - 3] == payload,
              "Radio native text/shadow position/depth mismatch")
    # The previous shadow is composed through native Vector3 addition rather
    # than the immediate stores used by the other three callers.
    helper = at(mapping["guiHelperRva"])
    draw = at(mapping["drawRva"])
    check(image[draw + 0x256:draw + 0x26E] ==
          b"\xC7\x44\x24\x44\0\0\0\x40\xC7\x44\x24\x48\0\0\0\x40\xC7\x44\x24\x4C\0\0\0\0",
          "Previous Radio shadow delta mismatch")
    check(image[helper + 0x7C:helper + 0x80] == b"\x8B\x4C\x24\x40",
          "Native row-vector end field mismatch")
    check(image[helper + 0x187:helper + 0x18A] == b"\x83\xC6\x1C",
          "Native wstring row stride mismatch")
    # Extract the GUI's row spacing and origins from the actual x87 operands.
    for offset, expected in ((0xC9, 16.0), (0xE2, 500.0), (0x12B, 501.0)):
        address = struct.unpack_from("<I", image, helper + offset)[0] - 0x400000
        check(struct.unpack_from("<f", image, address)[0] == expected,
              "Native GUI row origin/spacing mismatch")
    producer = mapping["addRadioMessage"]
    reg = at(producer["registrationRva"])
    string = struct.unpack_from("<I", image, reg + 1)[0] - 0x400000
    check(image[reg] == 0x68 and image[string:string + 19] == b" AddRadioMessage (\0",
          "Radio script registration mismatch")
    check(struct.unpack_from("<I", image, at(producer["wrapperPointerRva"]))[0] ==
          0x400000 + at(producer["registeredWrapperRva"]), "Radio registered wrapper mismatch")
    check(call_target(at(producer["nativeAddCallsite"])) == at(producer["nativeAddRva"]),
          "Radio wrapper does not reach native Add")
    if args.game == "requiem":
        credits = mapping["finalEndingTrace"]["startCredits"]
        for window in credits["windows"].values():
            decode_window(window)
        check(call_target(at(credits["nativeSetActiveCallsite"])) == at(credits["nativeSetActiveRva"]),
              "Final scripts' StartCredits wrapper target mismatch")
    if args.data_root:
        for script in mapping["producerScripts"]:
            check(hashlib.sha256((args.data_root / script["path"]).read_bytes()).hexdigest().upper() ==
                  script["sha256"], "Native producer script mismatch: " + script["path"])
    ending = evidence["endingNarrativeReadability"]
    check(hashlib.sha256(raw).hexdigest().upper() == ending["sourceImageSha256"],
          "Ending map belongs to a different initialized image")
    ending_windows = {name: decode_window(window) for name, window in ending["windows"].items()}
    check(call_target(at(ending["textHelperCallsite"])) == at(ending["textHelperRva"]),
          "Ending body-text helper ownership mismatch")
    check(call_target(at(ending["nativePreparedRowsCallsite"])) == at(ending["getWordWrapRowsRva"]),
          "Ending native row preparation ownership mismatch")
    producer = ending["scriptStartEnding1"]
    check(call_target(at(producer["setActiveCallsite"])) == at(producer["setActiveRva"]),
          "Ending script wrapper target mismatch")
    check(struct.unpack_from("<I", image, at(producer["wrapperPointerRva"]))[0] ==
          0x400000 + at(producer["registeredWrapperRva"]), "Ending registered wrapper mismatch")
    # Verify the independently decoded field layout as instructions inside the
    # hash-checked helper, not only a synthetic C++ object that mirrors it.
    helper_text = [(i.mnemonic, i.op_str) for i in ending_windows["nativeTextHelper"]]
    for field in ("font", "textActive", "rowsBegin", "rowsEnd", "visibleCharacters", "shade", "alpha"):
        offset = at(ending["fields"][field])
        check(any(f"+ {hex(offset)}]" in operands for _, operands in helper_text),
              "Ending field not present in native helper: " + field)
    if args.data_root:
        script = ending["producerScript"]
        check(hashlib.sha256((args.data_root / script["path"]).read_bytes()).hexdigest().upper() ==
              script["sha256"], "Ending producer script mismatch")
    print(json.dumps({"verified": True, "game": args.game, "validation": "offline exact-image only",
                      "radioWordWrapCallsites": mapping["wordWrapCallsites"],
                      "radioGuiCallsites": [mapping["getWordWrapRowsCallsite"], *mapping["guiDrawFontCallsites"]],
                      "endingTextHelperCallsite": ending["textHelperCallsite"],
                      "producerScriptsVerified": bool(args.data_root)}, indent=2))


if __name__ == "__main__":
    main()
