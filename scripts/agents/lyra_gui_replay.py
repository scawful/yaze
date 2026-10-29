#!/usr/bin/env python3
"""Generate a normal-action replay from a copied bundle's Lyra sidecar.

Does not copy, edit, launch or save a project. The admitted acceptance runner
owns isolation, producer selection, process teardown and full-bundle hashing.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

MESSAGE = "LyraMapProperties/A0/input_scalar:MessageId"
TREE = "LyraMapProperties/A0/tree:MusicMessage"
APPLY = "LyraMapProperties/button:Apply"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(sidecar, output):
    original = json.loads(sidecar.read_text())
    maps = original["maps"]
    matches = [i for i, item in enumerate(maps) if item["map_id"] == 0xA0]
    if len(matches) != 1:
        raise ValueError("Expected exactly one Lyra map $A0")
    index = matches[0]
    baseline = maps[index]["authoring"]["properties"]["message_id"]
    if type(baseline) is not int or not 0 <= baseline <= 0xFFFF:
        raise ValueError("Invalid message_id")
    edited = baseline ^ 1
    expected = copy.deepcopy(original)
    expected["maps"][index]["authoring"]["properties"]["message_id"] = edited

    def step(action, **kwargs):
        return dict(action=action, **kwargs,
                    expect={"success": True, "status": "passed"})

    def assertion(value):
        return step("assert", widget_key=MESSAGE,
                    condition=f"value_equals:{value:04X}")

    # A fresh app starts this tree closed. The caller selects Overworld map A0
    # and exposes Lyra properties through the normal editor layout first.
    opening = [step("wait", widget_key=TREE, timeout_ms=10000),
               step("click", widget_key=TREE),
               step("wait", widget_key=MESSAGE, timeout_ms=5000)]
    edit = opening + [
        assertion(baseline),
        step("type", widget_key=MESSAGE, text=f"{edited:04X}", clear_first=True),
        step("click", widget_key=APPLY),
        assertion(edited),
        step("click", target="shortcut:Undo"),
        assertion(baseline),
        step("click", target="shortcut:Redo"),
        assertion(edited),
        step("click", target="shortcut:Save ROM"),
    ]
    reopen = opening + [assertion(edited)]
    # Refuse to replace any earlier attempt, including failed evidence.
    output.mkdir(parents=True, exist_ok=False)
    for name, steps in (("edit-save", edit), ("reopen", reopen)):
        (output / f"{name}.json").write_text(json.dumps(
            {"schema_version": 1, "name": f"Current bundle Lyra {name}",
             "steps": steps}, indent=2) + "\n")
    (output / "expected-sidecar.json").write_text(
        json.dumps(expected, sort_keys=True, indent=2) + "\n")
    (output / "binding.json").write_text(json.dumps({
        "source_sidecar": str(sidecar.resolve()),
        "source_sha256": sha256(sidecar), "map_id": 0xA0,
        "before_message_id": baseline, "after_message_id": edited,
        "expected_sha256": sha256(output / "expected-sidecar.json"),
    }, indent=2) + "\n")


def verify(sidecar, output):
    binding = json.loads((output / "binding.json").read_text())
    expected_path = output / "expected-sidecar.json"
    if sha256(expected_path) != binding["expected_sha256"]:
        raise ValueError("Expected sidecar receipt changed")
    # Python equality aliases True/1 and 1.0/1. The native sidecar parser
    # rejects those type changes; preserve them while ignoring key order.
    def canonical(path):
        return json.dumps(json.loads(path.read_text()), sort_keys=True,
                          separators=(",", ":"), allow_nan=False)

    if canonical(sidecar) != canonical(expected_path):
        raise ValueError("Saved Lyra sidecar differs from the single message edit")
    return {"sidecar_sha256": sha256(sidecar), "map_id": binding["map_id"],
            "message_id": binding["after_message_id"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "verify"))
    parser.add_argument("--sidecar", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.mode == "prepare":
        prepare(args.sidecar, args.output)
    else:
        print(json.dumps(verify(args.sidecar, args.output), sort_keys=True))


if __name__ == "__main__":
    main()
