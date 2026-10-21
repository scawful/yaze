import copy
import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "footprint_proposal", Path(__file__).parents[1] / "footprint_proposal.py")
fp = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fp)


def fixture():
    bound = {"game_id": "synthetic", "adapter_id": "test",
             "rom_sha256": "a" * 64, "definition_fingerprint": "b" * 64,
             "profile_fingerprint": "c" * 64, "tile_count": 8}
    target = {"schema_version": 1, "units": "tile16", "binding": bound,
              "rows": [[1] * 6 for _ in range(6)], "protected": []}
    prefab = {"schema_version": 1, "units": "tile16", "binding": copy.deepcopy(bound),
              "semantic_id": "synthetic.inlay", "review_state": "reviewed",
              "rows": [[2, None], [0, 3]], "anchor": [0, 0],
              "overlap_policy": "reject", "clearance": [], "ports": [],
              "unresolved_behavior": ["synthetic example has no engine behavior"]}
    prefab["content_id"] = fp.content_id(prefab)
    return target, prefab


class FootprintProposalTest(unittest.TestCase):
    def test_lyra_bundle_retains_complete_inputs_and_read_only_report(self):
        target, prefab = fixture()
        target["rows"] = [[1] * 32 for _ in range(32)]
        target["binding"].update(game_id="oracle-of-secrets", adapter_id="lyra-sidecar-v3")
        prefab["binding"] = copy.deepcopy(target["binding"])
        prefab["content_id"] = fp.content_id(prefab)
        before = copy.deepcopy((target, prefab))
        result = fp.lyra_bundle(target, prefab, [[2, 2]], 0xA1)
        self.assertEqual(result["map_id"], 0xA1)
        self.assertEqual(result["target"], target)
        self.assertEqual(result["footprint"], prefab)
        self.assertEqual(result["report"], fp.plan(target, prefab, [[2, 2]]))
        self.assertFalse(result["report"]["writes_enabled"])
        self.assertEqual((target, prefab), before)
        for map_id in (True, 0x9F, 0xC0):
            with self.assertRaises(ValueError):
                fp.lyra_bundle(target, prefab, [[2, 2]], map_id)
        with self.assertRaisesRegex(ValueError, "1..64"):
            fp.lyra_bundle(target, prefab, [[2, 2]] * 65, 0xA0)

    def test_lyra_bundle_refuses_unbound_or_nonmap_input(self):
        target, prefab = fixture()
        with self.assertRaisesRegex(ValueError, "32x32"):
            fp.lyra_bundle(target, prefab, [[2, 2]], 0xA0)

    def test_versioned_map_assets_adapter_does_not_rebind_footprints(self):
        target, prefab = fixture()
        target["rows"] = [[1] * 32 for _ in range(32)]
        target["binding"].update(game_id="oracle-of-secrets",
                                 adapter_id="lyra-sidecar-v3-map-assets-v1")
        with self.assertRaisesRegex(ValueError, "bindings differ"):
            fp.lyra_bundle(target, prefab, [[2, 2]], 0xB0)
        prefab["binding"] = copy.deepcopy(target["binding"])
        prefab["content_id"] = fp.content_id(prefab)
        self.assertEqual(fp.lyra_bundle(target, prefab, [[2, 2]], 0xB0)["map_id"], 0xB0)
        target["binding"]["adapter_id"] = "unknown-adapter"
        prefab["binding"] = copy.deepcopy(target["binding"])
        prefab["content_id"] = fp.content_id(prefab)
        with self.assertRaisesRegex(ValueError, "32x32"):
            fp.lyra_bundle(target, prefab, [[2, 2]], 0xB0)

    def test_sparse_skip_and_explicit_zero_do_not_mutate_inputs(self):
        target, prefab = fixture()
        before = copy.deepcopy((target, prefab))
        result = fp.plan(target, prefab, [[2, 2]])
        self.assertEqual([(c["x"], c["y"], c["after"]) for c in result["changes"]],
                         [(2, 2, 2), (2, 3, 0), (3, 3, 3)])
        self.assertEqual(result["touched_tile32"][0]["after"], [2, 1, 0, 3])
        self.assertEqual((target, prefab), before)
        self.assertFalse(result["writes_enabled"])
        self.assertEqual(result, fp.plan(target, prefab, [[2, 2]]))

    def test_odd_anchor_reports_all_four_blocks(self):
        target, prefab = fixture()
        prefab["rows"] = [[2, 2], [2, 2]]
        prefab["content_id"] = fp.content_id(prefab)
        result = fp.plan(target, prefab, [[1, 1]])
        self.assertEqual(len(result["changes"]), 4)
        self.assertEqual([(b["x"], b["y"]) for b in result["touched_tile32"]],
                         [(0, 0), (1, 0), (0, 1), (1, 1)])

    def test_complete_inventory_reuse_and_capacity(self):
        target, prefab = fixture()
        target["tile32_inventory"] = {"complete": True, "capacity": 2,
                                      "entries": [{"id": 0, "tiles16": [1] * 4}]}
        result = fp.plan(target, prefab, [[2, 2]])
        self.assertEqual(result["capacity"]["new_tuples"], 1)
        target["tile32_inventory"]["entries"].append({"id": 1, "tiles16": [2, 1, 0, 3]})
        result = fp.plan(target, prefab, [[2, 2]])
        self.assertEqual(result["capacity"]["new_tuples"], 0)
        self.assertEqual(result["touched_tile32"][0]["existing_ids"], [1])
        target["tile32_inventory"]["entries"].pop()
        target["tile32_inventory"]["capacity"] = 1
        with self.assertRaisesRegex(ValueError, "capacity exceeded"):
            fp.plan(target, prefab, [[2, 2]])

    def test_incomplete_inventory_never_claims_capacity(self):
        target, prefab = fixture()
        target["tile32_inventory"] = {"complete": False, "capacity": 999, "entries": []}
        self.assertEqual(fp.plan(target, prefab, [[2, 2]])["capacity"]["status"], "unknown")
        target["tile32_inventory"]["complete"] = True
        with self.assertRaisesRegex(ValueError, "omits"):
            fp.plan(target, prefab, [[2, 2]])

    def test_each_binding_dimension_is_required(self):
        for key in fp.BINDING_FIELDS:
            target, prefab = fixture()
            prefab["binding"][key] = 9 if key == "tile_count" else "d" * 64
            prefab["content_id"] = fp.content_id(prefab)
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "bindings differ"):
                fp.plan(target, prefab, [[2, 2]])

    def test_review_and_content_identity(self):
        target, prefab = fixture()
        prefab["rows"][0][0] = 4
        with self.assertRaisesRegex(ValueError, "content_id"):
            fp.plan(target, prefab, [[2, 2]])
        prefab["review_state"] = "suggested"
        prefab["content_id"] = fp.content_id(prefab)
        with self.assertRaisesRegex(ValueError, "reviewed"):
            fp.plan(target, prefab, [[2, 2]])

    def test_protection_bounds_overlap_and_anchor(self):
        target, prefab = fixture()
        target["protected"] = [[2, 3]]
        with self.assertRaisesRegex(ValueError, "protected"):
            fp.plan(target, prefab, [[2, 2]])
        target["protected"] = []
        with self.assertRaisesRegex(ValueError, "outside"):
            fp.plan(target, prefab, [[5, 5]])
        with self.assertRaisesRegex(ValueError, "overlap"):
            fp.plan(target, prefab, [[2, 2], [2, 2]])
        prefab["overlap_policy"] = "same_only"
        prefab["anchor"] = [1, 1]
        prefab["content_id"] = fp.content_id(prefab)
        result = fp.plan(target, prefab, [[3, 3], [3, 3]])
        self.assertEqual(len(result["changes"]), 3)
        self.assertEqual(result["changes"][0]["x"], 2)

    def test_ports_and_clearance_check_final_combined_proposal(self):
        target, prefab = fixture()
        prefab["ports"] = [{"name": "east", "at": [2, 0], "allowed_tile16": [1]}]
        prefab["clearance"] = [{"at": [1, 0], "allowed_tile16": [1]}]
        prefab["content_id"] = fp.content_id(prefab)
        fp.plan(target, prefab, [[0, 0]])
        with self.assertRaisesRegex(ValueError, "ports"):
            fp.plan(target, prefab, [[0, 0], [2, 0]])
        prefab["clearance"][0]["allowed_tile16"] = [2]
        prefab["content_id"] = fp.content_id(prefab)
        with self.assertRaisesRegex(ValueError, "clearance"):
            fp.plan(target, prefab, [[0, 0]])

    def test_malformed_tiles_and_boolean_ids_refused(self):
        for value in (-1, 8, True, "1"):
            target, prefab = fixture()
            prefab["rows"][0][0] = value
            prefab["content_id"] = fp.content_id(prefab)
            with self.subTest(value=value), self.assertRaises(ValueError):
                fp.plan(target, prefab, [[2, 2]])

    def test_nonobject_inputs_and_inventory_are_refused(self):
        target, prefab = fixture()
        for t, p in (([], prefab), (target, [])):
            with self.assertRaisesRegex(ValueError, "JSON objects"):
                fp.plan(t, p, [[2, 2]])
        target["tile32_inventory"] = []
        with self.assertRaisesRegex(ValueError, "JSON object"):
            fp.plan(target, prefab, [[2, 2]])


if __name__ == "__main__":
    unittest.main()
