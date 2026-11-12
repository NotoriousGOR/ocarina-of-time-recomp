import importlib.util
from pathlib import Path
import sys

sys.dont_write_bytecode = True
source = Path(__file__).resolve().parents[1] / "mod-src/gopher64-cheats/build.py"
spec = importlib.util.spec_from_file_location("cheat_builder", source)
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

definitions = {
    "Always Have Wallet Size": {
        "data": ["8011A832 00??"],
        "options": {"Giant": "67", "Adult": "57"},
    },
    "Code Patch": {"data": ["81000200 1234"], "options": None},
    "Conditional": {"data": ["D0000001 0002", "81000002 1234"], "options": None},
}
catalog = builder.make_catalog("oot", definitions, [(0x80000200, 0x80000204)])
wallet, patch, conditional = catalog
assert wallet["options"] == ["Adult", "Giant"]
assert wallet["choices"][1] == [(0x82, 0x11A832, 0x67)]
assert patch["unsupported"] and not patch["choices"]
assert conditional["choices"][0] == [(0xD0, 1, 2), (0x81, 2, 0x1234)]
groups = builder.menu_groups(catalog)
assert [len(group) for group in groups] == [2, 1, 1]
assert [choice["options"] for choice in groups[0]] == [["On"], ["On"]]
assert groups[0][1]["choices"] == [wallet["choices"][1]]
assert groups[0][0]["id"] != groups[0][1]["id"]
assert "conflicting variations are ignored" in groups[0][0]["description"]
assert groups[1][0]["unsupported"] and not groups[1][0]["choices"]
choice_patch = builder.make_catalog("mm", {
    "Address Choice": {
        "data": ["81?????? 0000"],
        "options": {"A Safe": "200000", "Z Code": "100000"},
    },
}, [(0x80100000, 0x80100002)])[0]
assert choice_patch["unsupported"] and not choice_patch["choices"]
try:
    builder.decode_codes("Invalid", ["FF000000 0001"], None)
except ValueError:
    pass
else:
    raise AssertionError("Unsupported opcodes must be rejected")
