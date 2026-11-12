#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tomllib


ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent
GAMES = {
    "oot": (
        "D43DA81F-021E1E19-C:45",
        ROOT / "OoTRecompSyms/oot.ntsc-1.1.syms.toml",
        ROOT / "OoTRecompSyms/oot.ntsc-1.1.datasyms.toml",
    ),
    "mm": (
        "5354631C-03A2DEF0-C:45",
        ROOT / "Zelda64RecompSyms/mm.us.rev1.syms.toml",
        ROOT / "Zelda64RecompSyms/mm.us.rev1.datasyms.toml",
    ),
}
DISABLED = "Disabled"


def cheat_id(name):
    return "cheat_" + hashlib.sha256(name.encode("utf-8")).hexdigest()[:16]


def code_ranges(symbols, data_symbols):
    sections = tomllib.loads(symbols.read_text())["section"]
    ranges = [
        (func["vram"], func["vram"] + func["size"])
        for section in sections
        for func in section.get("functions", [])
        if func["size"]
    ]
    ranges.extend(
        (section["vram"], section["vram"] + section["size"])
        for section in sections if section["name"] in ("..makerom", "..makerom_ent", "..boot")
    )
    text = {}
    pattern = re.compile(r'name = "_codeSegmentText(Start|End)", vram = (0x[0-9A-Fa-f]+)')
    for line in data_symbols.read_text().splitlines():
        match = pattern.search(line)
        if match:
            text[match.group(1)] = int(match.group(2), 16)
    if set(text) != {"Start", "End"} or text["Start"] >= text["End"]:
        raise ValueError(f"Missing code segment bounds in {data_symbols}")
    ranges.append((text["Start"], text["End"]))
    return ranges


def decode_codes(name, lines, option):
    result = []
    for line in lines:
        if "?" in line:
            if option is None or not re.fullmatch(r"[0-9A-Fa-f]+", option):
                raise ValueError(f"Missing or invalid option in {name}")
            line = re.sub(r"\?+", option, line)
        fields = line.split()
        if len(fields) != 2 or not re.fullmatch(r"[0-9A-Fa-f]{8}", fields[0]):
            raise ValueError(f"Invalid cheat code in {name}: {line}")
        encoded, value = (int(field, 16) for field in fields)
        kind, address = encoded >> 24, encoded & 0xFFFFFF
        if kind not in (0x80, 0x81, 0xD0) or not 0 <= value <= 0xFFFF:
            raise ValueError(f"Unsupported cheat code in {name}: {line}")
        if address + (kind == 0x81) >= 0x800000:
            raise ValueError(f"Cheat address outside RDRAM in {name}: {line}")
        result.append((kind, address, value))
    if not result:
        raise ValueError(f"Cheat has no codes: {name}")
    return result


def make_catalog(game, definitions, ranges):
    catalog = []
    for name, definition in sorted(definitions.items()):
        raw_options = definition.get("options")
        options = sorted(raw_options.items()) if raw_options else [("On", None)]
        if any(label == DISABLED for label, _ in options):
            raise ValueError(f"Cheat option conflicts with {DISABLED}: {name}")
        choices = [decode_codes(name, definition["data"], value) for _, value in options]
        patches_code = any(
            kind in (0x80, 0x81)
            and any(start < 0x80000000 + address + (2 if kind == 0x81 else 1)
                    and 0x80000000 + address < end for start, end in ranges)
            for choice in choices for kind, address, _ in choice
        )
        description = definition.get("note", "").strip()
        if patches_code:
            description = (
                "Unavailable in this recompilation: the original cheat changes executable instructions. "
                "Use Gopher64 for this cheat."
            )
            if name == "Widescreen":
                description += " Widescreen is available under Graphics."
            options, choices = [], []
        elif game == "oot" and name == "Always Have Wallet Size":
            description = "Changes only the wallet capacity, preserving other upgrades."
            choices = [[(0x82 if kind == 0x80 else kind, address, value)
                        for kind, address, value in codes] for codes in choices]
        catalog.append({
            "id": cheat_id(name),
            "name": name,
            "description": description,
            "options": [label for label, _ in options],
            "choices": choices,
            "unsupported": patches_code,
        })
    return catalog


def menu_groups(catalog):
    groups = []
    for cheat in catalog:
        if not cheat["choices"] or cheat["options"] == ["On"]:
            groups.append([cheat])
            continue
        variants = []
        for label, choice in zip(cheat["options"], cheat["choices"]):
            name = f'{label} - {cheat["name"]}'
            description = f'{cheat["name"]}: {label}.'
            if len(cheat["choices"]) > 1:
                description += " Enable only one variation of this cheat; conflicting variations are ignored."
            if cheat["description"]:
                description += " " + cheat["description"]
            variants.append({
                **cheat,
                "id": cheat_id(cheat["name"] + "\0" + label),
                "name": name if len(name) <= 64 else name[:61] + "...",
                "description": description,
                "options": ["On"],
                "choices": [choice],
            })
        groups.append(variants)
    return groups


def write_catalog(path, groups):
    catalog = [cheat for group in groups for cheat in group]
    lines = []
    for i, cheat in enumerate(catalog):
        if cheat["choices"]:
            lines.append(f"static const CheatCode codes_{i}[] = {{")
            for choice in cheat["choices"]:
                for kind, address, value in choice:
                    lines.append(f"    {{ 0x{address:06X}, 0x{value:04X}, 0x{kind:02X} }},")
            lines.append("};")
    lines.append("static const Cheat cheats[] = {")
    for i, cheat in enumerate(catalog):
        choices = cheat["choices"]
        codes = f"codes_{i}" if choices else "0"
        count = len(choices[0]) if choices else 0
        lines.append(f'    {{ "{cheat["id"]}", {codes}, {count}, {len(choices)} }},')
    lines.extend(["};", "static const CheatGroup cheat_groups[] = {"])
    first = 0
    for group in groups:
        lines.append(f"    {{ {first}, {len(group)} }},")
        first += len(group)
    lines.extend(["};", "#define CHEAT_GROUP_COUNT (sizeof(cheat_groups) / sizeof(cheat_groups[0]))", ""])
    path.write_text("\n".join(lines))


def write_manifest(path, game, symbols, elf, groups):
    def quote(value):
        return json.dumps(value, ensure_ascii=False)

    def option(cheat):
        options = [DISABLED] + cheat["options"]
        return (
            "{ id = " + quote(cheat["id"]) + ", name = " + quote(cheat["name"])
            + ", description = " + quote(cheat["description"])
            + ', type = "Enum", options = ' + quote(options)
            + ", default = " + quote(DISABLED) + " }"
        )

    mod_id = f"{game}_gopher64_cheats"
    text = [
        "[manifest]",
        f"id = {quote(mod_id)}",
        'version = "1.0.0"',
        'display_name = "Gopher64 Cheats"',
        'description = "Optional cheats imported from Gopher64. Back up saves before using them."',
        'short_description = "Optional game-specific cheats."',
        'authors = ["Gopher64 contributors"]',
        f"game_id = {quote(game)}",
        'minimum_recomp_version = "1.2.2"',
        "config_options = [",
        *(f"  {option(cheat)}," for group in groups for cheat in group),
        "]",
        "",
        "[inputs]",
        f"elf_path = {quote(str(elf))}",
        f"mod_filename = {quote(mod_id)}",
        f"func_reference_syms_file = {quote(str(symbols))}",
        "data_reference_syms_files = []",
        "additional_files = []",
        "",
    ]
    path.write_text("\n".join(text))


def build_mod(game, definitions, symbols, data_symbols, output, tool):
    catalog = make_catalog(game, definitions, code_ranges(symbols, data_symbols))
    groups = menu_groups(catalog)
    directory = output / game
    package = directory / "package"
    package.mkdir(parents=True, exist_ok=True)
    header = directory / "cheat_catalog.h"
    obj = directory / "mod.o"
    elf = directory / "mod.elf"
    manifest = directory / "mod.toml"
    write_catalog(header, groups)
    write_manifest(manifest, game, symbols, elf, groups)

    subprocess.run([
        os.environ.get("CC", "clang"), "-target", "mips", "-mips2", "-mabi=32", "-O2",
        "-G0", "-mno-abicalls", "-mno-odd-spreg", "-mno-check-zero-division",
        "-fomit-frame-pointer", "-fno-builtin-memset", "-nostdinc", "-ffunction-sections",
        "-I", str(SOURCE), "-I", str(directory), "-I", str(ROOT / "mod-src/fast-songs/include"),
        "-c", str(SOURCE / "main.c"), "-o", str(obj),
    ], check=True)
    subprocess.run([
        os.environ.get("LD", "ld.lld"), str(obj), "-nostdlib", "-T",
        str(ROOT / "mod-src/fast-songs/mod.ld"), "--unresolved-symbols=ignore-all",
        "--emit-relocs", "-e", "0", "--no-nmagic", "-gc-sections", "-o", str(elf),
    ], check=True)
    subprocess.run([str(tool), str(manifest), str(package)], check=True)
    unsupported = [cheat["name"] for cheat in catalog if cheat["unsupported"]]
    print(f"{game}: {len(catalog) - len(unsupported)} usable, {len(unsupported)} unavailable")
    for name in unsupported:
        print(f"  unavailable: {name}")


def main():
    parser = argparse.ArgumentParser(description="Build Zelda cheat mods from Gopher64's cheat database.")
    parser.add_argument("database", type=Path, help="Path to Gopher64's data/cheats.json")
    parser.add_argument("--output", type=Path, default=ROOT / "build-gopher64-cheats")
    parser.add_argument("--mod-tool", type=Path, default=ROOT / "build-n64-tools/RecompModTool")
    args = parser.parse_args()
    database = json.loads(args.database.read_text())
    output = args.output.resolve()
    tool = args.mod_tool.resolve()
    for game, (crc, symbols, data_symbols) in GAMES.items():
        build_mod(game, database[crc], symbols, data_symbols, output, tool)


if __name__ == "__main__":
    main()
