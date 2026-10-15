#!/usr/bin/env python3
"""Generate deterministic Lyra test data from zeros, never from a game ROM.

The directory follows src/rom/lyra_layout_descriptor.cc revision 1. These
images contain metadata, empty sprite lists and synthetic Tile32 numbers only;
they cannot boot or supply the game's graphics. Product writes remain refused.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

SIZE = 0x200000
DESCRIPTOR = 0x147000
HEADER = struct.Struct("<8sHHHHHHBBHII")
ENTRY = struct.Struct("<HHIHHI")
STRIDES = (2, 1, 1, 1, 2, 8, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
           2, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1)
COUNTS = tuple(64 if i in (23, 24, 26) else 128 if i == 27 else 192
               for i in range(31))
HEADER_PATH = Path(__file__).with_name("synthetic_fixture_data.h")


def pc_snes(pc):
    return (pc // 0x8000 << 16) | 0x8000 | (pc & 0x7FFF)


def directory():
    result = []
    cursor = 0x144000
    for i, (count, stride) in enumerate(zip(COUNTS, STRIDES)):
        pc = (0x1788D if i == 29 else 0x4C635 if i == 30 else cursor)
        result.append((i + 1, 1, pc, count, stride))
        cursor += count * stride
    result.extend(((100, 2, 0x1E4000, 0x4000, 1),
                   (200, 3, 0x58000, 0x7E70, 1),
                   (201, 3, 0x60000, 0x411F, 1),
                   (202, 3, 0x130000, 0x8000, 1)))
    return result


def reseal(image):
    entries = directory()
    if struct.unpack_from("<H", image, DESCRIPTOR + 18)[0] == 36:
        entries.append((203, 4, 0x132000, 2, 512))
    for i, (_, _, pc, count, stride) in enumerate(entries):
        struct.pack_into("<I", image, DESCRIPTOR + 32 + 16 * i + 12,
                         zlib.crc32(image[pc:pc + count * stride]))
    struct.pack_into("<I", image, DESCRIPTOR + 28, 0)
    struct.pack_into("<I", image, DESCRIPTOR + 28,
                     zlib.crc32(image[DESCRIPTOR:DESCRIPTOR + 1024]))


def pattern(area):
    return [0x20B2 if ((x in (0, 15) or y in (0, 15)) and
                      not (y in (7, 8) and x == (15 if area == 0xA0 else 0)))
            or (area == 0xA1 and x == 8 and 3 <= y <= 12) else 0x2103
            for y in range(16) for x in range(16)]


def make_image(preview=False):
    image = bytearray(SIZE)
    image[0x7FC0:0x7FC0 + 21] = b"THE LEGEND OF ZELDA".ljust(21, b" ")
    image[0x140145] = 3
    image[0x131000:0x131004] = bytes.fromhex("e4ff00ff")
    image[0x1E4000] = 0xFF
    for ident, _, pc, count, stride in directory()[:31]:
        for i in range(count):
            value = 0xC000 if ident in (16, 17, 18) else (
                0x269000 if ident in (20, 21) else i if ident == 7 else
                3 if ident == 29 and i >= 160 else 0xFF if ident == 5 else 0)
            image[pc + i * stride:pc + (i + 1) * stride] = value.to_bytes(
                stride, "little")
    # One original, synthetic single-tile collision record for save-refusal
    # tests. It is outside every descriptor-owned range.
    image[0x128090 + 0x25 * 3:0x128090 + 0x25 * 3 + 3] = (
        pc_snes(0x128450).to_bytes(3, "little"))
    image[0x128450:0x128457] = bytes.fromhex("f0f0000008ffff")
    if preview:
        cursor = 0x131010
        tables = directory()
        for area in (0xA0, 0xA1):
            tiles = pattern(area)
            for index, values in ((19, [v >> 8 for v in tiles]),
                                  (20, [v & 0xFF for v in tiles])):
                encoded = bytes((0xE0, 0xFF)) + bytes(values) + b"\xff"
                image[cursor:cursor + len(encoded)] = encoded
                pc = tables[index][2] + area * 3
                image[pc:pc + 3] = pc_snes(cursor).to_bytes(3, "little")
                cursor += len(encoded)
    HEADER.pack_into(image, DESCRIPTOR, b"OOSOW192", 1, 32, 592, 192, 160,
                     35, 0x3C, 1, 384, len(image), 0)
    for i, (ident, kind, pc, count, stride) in enumerate(directory()):
        ENTRY.pack_into(image, DESCRIPTOR + 32 + i * 16, ident, kind,
                        pc_snes(pc), count, stride, 0)
    reseal(image)
    return bytes(image)


def make_writable_image():
    image = bytearray(make_image(True))
    HEADER.pack_into(image, DESCRIPTOR, b"OOSOW192", 2, 32, 608, 192, 160,
                     36, 0x3C, 3, 384, len(image), 0)
    ENTRY.pack_into(image, DESCRIPTOR + 32 + 35 * 16, 203, 4,
                    pc_snes(0x132000), 2, 512, 0)
    tiles = pattern(0xA0)
    for index, pc, values in ((19, 0x132000, [v >> 8 for v in tiles]),
                              (20, 0x132200, [v & 0xFF for v in tiles])):
        image[pc:pc + 259] = b"\xe0\xff" + bytes(values) + b"\xff"
        pointer = directory()[index][2] + 0xA0 * 3
        image[pointer:pointer + 3] = pc_snes(pc).to_bytes(3, "little")
    # Original synthetic definitions make model mutations observable without
    # copying graphics or collision data from any game ROM.
    for tile, tile16 in ((0x2103, 1), (0x20B2, 2)):
        position = tile // 4 * 6 + tile % 4
        for base in (0x18000, 0x20000, 0x1F0000, 0x1F8000):
            image[base + position] = tile16
    reseal(image)
    return bytes(image)


def changes(before, after):
    patches = []
    i = 0
    while i < min(len(before), len(after)):
        if before[i] == after[i]:
            i += 1
            continue
        begin = i
        while i < min(len(before), len(after)) and before[i] != after[i]:
            i += 1
        patches.append({"pc": begin, "hex": after[begin:i].hex()})
    return patches


def fixtures():
    offline = make_image()
    legacy = bytearray(offline)
    legacy[DESCRIPTOR:DESCRIPTOR + 1024] = bytes(1024)
    images = {"OFFLINE_ONLY.bin": offline,
              "LYRA_PREVIEW_A0_A1.bin": make_image(True),
              "legacy_160_input.bin": bytes(legacy),
              "LYRA_WRITABLE_A0.bin": make_writable_image()}
    cases = []

    def case(name, base="OFFLINE_ONLY.bin", patches=(), expected="reject",
             seal=True, truncate=None):
        data = bytearray(images[base])
        for pc, value in patches:
            data[pc:pc + len(value)] = value
        if patches and seal:
            reseal(data)
        if truncate is not None:
            del data[truncate:]
        cases.append({"name": name, "base": base,
                      "patches": changes(images[base], data),
                      "truncate_to": truncate, "expected": expected,
                      "image_sha256": hashlib.sha256(data).hexdigest()})

    case("legacy_synthetic_160", "legacy_160_input.bin", expected="legacy_160")
    case("supported_revision1_offline", expected="supported_offline_inspection_only")
    case("unknown_magic", patches=((DESCRIPTOR, b"UNKNOWN!"),))
    case("unknown_revision", patches=((DESCRIPTOR + 8, b"\x02\x00"),))
    case("wrong_active_count", patches=((DESCRIPTOR + 16, b"\xc0\x00"),))
    case("wrong_phase_stride", patches=((DESCRIPTOR + 22, b"\x40\x01"),))
    case("unknown_table_id", patches=((DESCRIPTOR + 32, b"\xff\x00"),))
    case("invalid_table_bank", patches=((DESCRIPTOR + 36,
                                         struct.pack("<I", 0x408000)),))
    case("crossing_table", patches=((DESCRIPTOR + 36,
                                     struct.pack("<I", 0x28FF80)),))
    case("data_crc", patches=((directory()[5][2], b"\x01"),), seal=False)
    crc = bytes((offline[DESCRIPTOR + 28] ^ 1,))
    case("descriptor_crc", patches=((DESCRIPTOR + 28, crc),), seal=False)
    case("truncated_descriptor", truncate=DESCRIPTOR + 100)
    case("image_length", truncate=SIZE - 1)
    case("map_pointer", patches=((directory()[19][2] + 160 * 3,
                                   bytes.fromhex("008027")),))
    case("inactive_parent", patches=((directory()[6][2] + 160, b"\xc0"),))
    case("old_tail_marker_without_descriptor", "legacy_160_input.bin",
         patches=((0x1423FF, b"\xea"),), seal=False)
    return images, {"schema_version": 1, "provenance": "synthetic_no_rom_input",
                    "cases": cases}


def generate_header():
    images, cases = fixtures()
    lines = ["// Generated by generate_fixtures.py; synthetic data only.",
             "// clang-format off", "#ifndef YAZE_TEST_LYRA_SYNTHETIC_DATA_H_",
             "#define YAZE_TEST_LYRA_SYNTHETIC_DATA_H_", "",
             "#include <cstddef>", "#include <cstdint>", "#include <string_view>",
             "", "namespace yaze::test::lyra {", "",
             "struct CompressedFixture {", "  const uint8_t* data;",
             "  size_t size;", "  size_t image_size;", "};", ""]
    for i, (name, data) in enumerate(images.items()):
        compressed = zlib.compress(data, 9)
        lines.append(f"// {name}: {len(data)} bytes, SHA256 {hashlib.sha256(data).hexdigest()}")
        lines.append(f"inline constexpr uint8_t kImage{i}[] = {{")
        for start in range(0, len(compressed), 16):
            lines.append("  " + ", ".join(f"0x{v:02X}" for v in compressed[start:start + 16]) + ",")
        lines.extend(("};", ""))
    lines.append("inline CompressedFixture FindFixture(std::string_view name) {")
    for i, name in enumerate(images):
        lines.append(f'  if (name == "{name}") return {{kImage{i}, sizeof(kImage{i}), {SIZE}}};')
    lines.extend(("  return {nullptr, 0, 0};", "}", "",
                  'inline constexpr char kCases[] = R"LYRA(' + json.dumps(cases, separators=(",", ":")) + ')LYRA";',
                  "", "}  // namespace yaze::test::lyra", "",
                  "#endif  // YAZE_TEST_LYRA_SYNTHETIC_DATA_H_", ""))
    return "\n".join(lines)


def self_test():
    images, manifest = fixtures()
    assert images == fixtures()[0]
    assert len(manifest["cases"]) == 16
    for name, image in images.items():
        assert len(image) == SIZE
        assert zlib.decompress(zlib.compress(image, 9)) == image
        if name == "legacy_160_input.bin":
            continue
        descriptor = bytearray(image[DESCRIPTOR:DESCRIPTOR + 1024])
        expected = struct.unpack_from("<I", descriptor, 28)[0]
        descriptor[28:32] = bytes(4)
        assert zlib.crc32(descriptor) == expected
        for i, (_, _, pc, count, stride) in enumerate(directory()):
            crc = struct.unpack_from("<I", descriptor, 32 + 16 * i + 12)[0]
            assert zlib.crc32(image[pc:pc + count * stride]) == crc
        if name == "LYRA_WRITABLE_A0.bin":
            assert struct.unpack_from("<H", descriptor, 8)[0] == 2
            assert struct.unpack_from("<H", descriptor, 18)[0] == 36
            assert zlib.crc32(image[0x132000:0x132400]) == struct.unpack_from(
                "<I", descriptor, 32 + 35 * 16 + 12)[0]
    for case in manifest["cases"]:
        data = bytearray(images[case["base"]])
        for patch in case["patches"]:
            value = bytes.fromhex(patch["hex"])
            data[patch["pc"]:patch["pc"] + len(value)] = value
        if case["truncate_to"] is not None:
            del data[case["truncate_to"]:]
        assert hashlib.sha256(data).hexdigest() == case["image_sha256"]
    print("PASS: deterministic images, zlib round trips, directory/data CRCs, 16 vector hashes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check committed header matches generator")
    parser.add_argument("--self-test", action="store_true", help="run lightweight integrity checks")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    generated = generate_header()
    if args.check:
        if HEADER_PATH.read_text() != generated:
            raise SystemExit("Generated fixture header is stale")
        print(f"PASS: {HEADER_PATH.name} matches generator ({len(generated)} bytes)")
    elif not args.self_test:
        HEADER_PATH.write_text(generated)
        print(f"Wrote {HEADER_PATH} ({len(generated)} bytes)")


if __name__ == "__main__":
    main()
