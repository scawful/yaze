"""Pinned source extraction and artifact guards; standard library, no ROM/build."""
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch

TOOL = Path(__file__).resolve().parents[1] / 'dungeon_inventory.py'
spec = importlib.util.spec_from_file_location('dungeon_inventory', TOOL)
inv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inv)
ROOT = TOOL.parents[2]


class DungeonInventoryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = inv.read_json(inv.DEFAULT_CONTRACT)
        cls.inventory = inv.build_inventory(ROOT)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.root = self.home / 'checkout'
        self.root.mkdir()
        for item in self.contract['source_files']:
            target = self.root / item['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / item['path'], target)
        self.contract_path = self.home / 'contract.json'
        self.edited_contract = copy.deepcopy(self.contract)
        self.save_contract()

    def save_contract(self):
        self.contract_path.write_text(json.dumps(self.edited_contract))

    def modify(self, path, before, after, refresh=False):
        target = self.root / path
        text = target.read_text()
        self.assertIn(before, text)
        target.write_text(text.replace(before, after, 1))
        if refresh:
            for item in self.edited_contract['source_files']:
                if item['path'] == path:
                    item['sha256'] = inv.digest(target.read_bytes())
            self.save_contract()

    def build(self):
        return inv.build_inventory(self.root, self.contract_path)

    def binding_fixture(self):
        inventory = self.inventory
        profile_data = inv.read_json(TOOL.with_name('profiles') / 'oracle-sky-dungeon-family.json')
        profile = self.home / 'profile.json'
        profile.write_text(json.dumps(profile_data))
        rows = []
        for routine in inventory['routines']:
            rid = routine['id']
            rows.append({'routine_id': rid, 'routine_name': routine['name'],
                         'classification_status': 'UNCLASSIFIED_MULTIPURPOSE_PRIMITIVE',
                         'primary_family_id': None, 'secondary_family_ids': [],
                         'source_basis': {'function': routine['function'],
                                          'definition': routine['function_source'],
                                          'registration': routine['registration_source'],
                                          'source_commit': inventory['source_commit']},
                         'dispatch_exception_keys': sorted({key for obj in inventory['objects']
                             if obj['routine_id_default'] == rid or obj['routine_id_custom_enabled'] == rid
                             for key in obj['dispatch_exception_keys']}),
                         'actual_character_usage': 'NOT_MEASURED'})
        data = {'schema_version': 1, 'kind': 'routine_to_style_family_accounting',
                'source_commit': inventory['source_commit'], 'source_files': inventory['source_files'],
                'family_profile': profile.name, 'bindings': rows,
                'counts': {'registered_routines': 135, 'objects_joined': 448, 'serializable_objects_joined': 440}}
        path = self.home / 'bindings.json'
        path.write_text(json.dumps(data))
        return data, path

    def test_exact_population_aliases_and_known_objects(self):
        data = self.inventory
        objects = {obj['id']: obj for obj in data['objects']}
        self.assertEqual(len(objects), 448)
        self.assertEqual({obj['id'] for obj in data['objects'] if obj['room_stream_representable']},
                         set(range(248)) | set(range(0x100, 0x140)) | set(range(0xF80, 0x1000)))
        self.assertEqual({row['id'] for row in data['routines']}, set(range(135)))
        self.assertEqual([objects[i]['routine_id_default'] for i in (0x100, 0xF98, 0xF99, 0xFB1)], [116, 92, 39, 114])
        self.assertEqual([objects[i]['parser_payload_word_count'] for i in (0xF99, 0xF9A, 0xFB1, 0xFB2)], [8, 4, 24, 12])
        for oid in range(248, 256):
            self.assertFalse(objects[oid]['room_stream_representable'])
            self.assertIn('rejected', objects[oid]['status'])
        self.assertEqual(objects[0xFF3]['dispatch_exception_keys'], ['full_layer_mask'])
        self.assertEqual(objects[0xF98]['dispatch_exception_keys'], ['big_key_lock_dispatch'])
        self.assertIn('big_chest_dispatch', objects[0xFB1]['dispatch_exception_keys'])
        self.assertIsNone(data['vanilla_disassembly_source'])
        self.assertTrue(all(obj['vanilla_disassembly'] is None for obj in objects.values()))

    def test_custom_families_and_layer_semantics(self):
        data = self.inventory
        objects = {obj['id']: obj for obj in data['objects']}
        self.assertEqual([len(row['default_slots']) for row in data['custom_families']], [16, 3, 2])
        for oid in (0x31, 0x32, 0x54):
            self.assertEqual((objects[oid]['routine_id_default'], objects[oid]['routine_id_custom_enabled']), (38, 130))
        routines = {row['id']: row for row in data['routines']}
        self.assertFalse(routines[3]['draws_to_both_bgs_metadata'])  # Historical BothBG function name differs.
        self.assertEqual(data['layer_semantics']['YazeBG1'], '$7E2000 / hardware BG2')
        self.assertEqual(data['actual_character_usage'], 'NOT_MEASURED')

    def test_deterministic_portable_and_source_unchanged(self):
        before = {p: inv.digest((self.root / p).read_bytes()) for p in inv.Sources(self.root, self.edited_contract).raw}
        first, second = self.build(), self.build()
        self.assertEqual(first, second)
        text = json.dumps(first)
        self.assertNotIn(str(self.root), text)
        self.assertNotIn(str(Path.home()) + '/', text)
        self.assertNotIn('file://', text)
        for path, pin in before.items():
            self.assertEqual(inv.digest((self.root / path).read_bytes()), pin)

    def test_fingerprint_drift_precedes_all_parsing_and_no_output(self):
        self.modify(inv.REG, '#include', '// changed\n#include')
        with patch.object(inv, 'parse_constants', side_effect=RuntimeError('must not parse')) as parse:
            with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_FINGERPRINT_MISMATCH'):
                self.build()
            parse.assert_not_called()
        output = self.home / 'result.json'
        with redirect_stderr(io.StringIO()):
            self.assertEqual(inv.main(['--source-root', str(self.root), '--contract', str(self.contract_path),
                                      '--output', str(output)]), 2)
        self.assertFalse(output.exists())

    def test_missing_source_fails_before_parsing(self):
        (self.root / inv.REG).unlink()
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_FINGERPRINT_MISMATCH'):
            self.build()

    def test_refreshed_pin_does_not_accept_conditional_mapping(self):
        self.modify(inv.REG, 'object_to_routine_map_[0x00] = 0;',
                    'if (true) { object_to_routine_map_[0x00] = 0; }', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_refreshed_pin_does_not_accept_unsupported_loop(self):
        self.modify(inv.REG, 'id <= 0x02', 'id < 0x03', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_duplicate_mapping_fails(self):
        self.modify(inv.REG, 'object_to_routine_map_.clear();',
                    'object_to_routine_map_.clear(); object_to_routine_map_[0x00] = 0;', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'DUPLICATE_OBJECT_MAPPING'):
            self.build()

    def test_extra_builder_module_fails(self):
        self.modify(inv.REG, 'draw_routines::RegisterSpecialRoutines(routines_);',
                    'draw_routines::RegisterSpecialRoutines(routines_); MoreRoutines(routines_);', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'registry builder/module set'):
            self.build()

    def test_unknown_registration_statement_and_field_fail(self):
        path = inv.MODULES['Rightwards']
        for before, after in [('registry.push_back(DrawRoutineInfo{', 'Unknown(); registry.push_back(DrawRoutineInfo{'),
                              ('.base_width = 2,', '.unknown_field = 2, .base_width = 2,')]:
            original = (self.root / path).read_bytes()
            self.modify(path, before, after, refresh=True)
            with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
                self.build()
            (self.root / path).write_bytes(original)

    def test_duplicate_routine_id_fails(self):
        path = inv.MODULES['Rightwards']
        self.modify(path, '.id = DrawRoutineIds::kRightwards2x4_1to15or26,',
                    '.id = DrawRoutineIds::kRightwards2x2_1to15or32,', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'DUPLICATE_ROUTINE_ID'):
            self.build()

    def test_unconsumed_count_statements_fail(self):
        self.modify(inv.PARSER, 'int ObjectParser::GetSubtype2TileCount(int16_t object_id) const {',
                    'int ObjectParser::GetSubtype2TileCount(int16_t object_id) const { Unexpected();', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_arbitrary_count_expression_not_executed(self):
        self.modify(inv.PARSER, 'object_id >= 0x100 && object_id <= 0x10F', 'dangerous_call(object_id)', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_unconsumed_name_array_expression_fails(self):
        self.modify(inv.NAMES, '"Corner (top, concave)', 'Unrecognized(), "Corner (top, concave)', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_changed_subtype1_helper_is_not_silently_reinterpreted(self):
        self.modify(inv.PARSER, 'if (count > 0)', 'if (count > 1)', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_changed_custom_lookup_is_not_silently_reinterpreted(self):
        self.modify(inv.REG, 'object_id == 0x54', 'object_id == 0x55', refresh=True)
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_GRAMMAR'):
            self.build()

    def test_profile_source_range_id_status_and_palette_guards(self):
        _, bindings = self.binding_fixture()
        path = self.home / 'profile.json'
        original = inv.read_json(path)
        mutations = [lambda d: d['sources']['S1'].update(sha256='0' * 64),
                     lambda d: d['sources']['S1'].update(lines=[1, 999999]),
                     lambda d: d['families'][0].update(source_refs=['MISSING']),
                     lambda d: d['draft_modules'][0].update(family_ids=['MISSING']),
                     lambda d: d['families'][0].update(engine_status='COMPLETE'),
                     lambda d: d['palette_contract']['per_family_bindings'].pop(),
                     lambda d: d['palette_contract']['per_family_bindings'][0].update(tilemap_bank=3)]
        for mutation in mutations:
            changed = copy.deepcopy(original)
            mutation(changed)
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(inv.InventoryError, 'FAMILY_'):
                inv.validate_families(self.inventory, bindings)

    def test_profile_family_module_backlink_is_bidirectional(self):
        _, bindings = self.binding_fixture()
        path = self.home / 'profile.json'
        profile = inv.read_json(path)
        family = profile['families'][0]
        other_module = next(module for module in profile['draft_modules'] if family['id'] not in module['family_ids'])
        family['draft_modules'].append(other_module['id'])
        path.write_text(json.dumps(profile))
        with self.assertRaisesRegex(inv.InventoryError, 'family/module backlink mismatch'):
            inv.validate_families(self.inventory, bindings)

    def test_profile_external_source_is_explicitly_unbound_without_opt_in(self):
        _, bindings = self.binding_fixture()
        result = inv.validate_families(self.inventory, bindings)
        self.assertEqual(result['profile_validation']['checked_yaze_source_refs'], 40)
        self.assertEqual(result['profile_validation']['unbound_optional_usdasm_refs'], ['S38', 'S39'])

    def test_output_exclusive_symlink_and_source_tree_guards(self):
        data = b'{"kind":"artifact"}\n'
        output = self.home / 'new.json'
        inv.publish(output, data, self.root)
        self.assertEqual(output.read_bytes(), data)
        with self.assertRaisesRegex(inv.InventoryError, 'OUTPUT_EXISTS'):
            inv.publish(output, b'changed', self.root)
        link = self.home / 'alias.json'
        link.symlink_to(output)
        with self.assertRaisesRegex(inv.InventoryError, 'OUTPUT_EXISTS'):
            inv.publish(link, b'changed', self.root)
        self.assertEqual(output.read_bytes(), data)
        with self.assertRaisesRegex(inv.InventoryError, 'OUTPUT_PATH'):
            inv.publish(self.root / 'do-not-write.json', data, self.root)
        with self.assertRaisesRegex(inv.InventoryError, 'OUTPUT_PATH'):
            inv.publish(self.home / 'out.sfc', data, self.root)

    def test_contract_duplicate_key_and_source_path_escape(self):
        self.contract_path.write_text('{"schema_version":1,"schema_version":2}')
        with self.assertRaisesRegex(inv.InventoryError, 'DUPLICATE_JSON_KEY'):
            self.build()
        self.edited_contract['source_files'][0]['path'] = '../outside.cc'
        self.save_contract()
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_CONTRACT'):
            self.build()

    def test_optional_usdasm_is_explicit_and_pinned(self):
        # Default extraction succeeded without creating any external source.
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_FINGERPRINT_MISMATCH'):
            inv.build_inventory(self.root, self.contract_path, self.home / 'absent-external')
        external = self.home / 'external'
        external.mkdir()
        (external / 'bank_01.asm').write_text('not the pinned disassembly')
        with self.assertRaisesRegex(inv.InventoryError, 'SOURCE_FINGERPRINT_MISMATCH'):
            inv.build_inventory(self.root, self.contract_path, external)

    def test_family_join_and_missing_duplicate_unknown_status(self):
        data, path = self.binding_fixture()
        result = inv.validate_families(self.inventory, path)
        self.assertEqual((result['registered_routines'], result['objects_joined'], result['serializable_objects_joined']), (135, 448, 440))
        mutations = [lambda d: d['bindings'].pop(), lambda d: d['bindings'].append(d['bindings'][0]),
                     lambda d: d['bindings'][0].update(classification_status='ALL_ART_COMPLETE'),
                     lambda d: d['bindings'][0].update(primary_family_id='MISSING'),
                     lambda d: d['bindings'][0].update(actual_character_usage='COMPLETE'),
                     lambda d: d['counts'].update(objects_joined=447)]
        for mutation in mutations:
            changed = copy.deepcopy(data)
            mutation(changed)
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(inv.InventoryError, 'FAMILY_BINDING'):
                inv.validate_families(self.inventory, path)

    def test_family_source_drift_and_wrapper_omission_fail(self):
        data, path = self.binding_fixture()
        for change in ['source', 'wrapper']:
            changed = copy.deepcopy(data)
            if change == 'source':
                changed['source_files'][0]['sha256'] = '0' * 64
            else:
                next(row for row in changed['bindings'] if row['routine_id'] == 130)['dispatch_exception_keys'] = []
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(inv.InventoryError, 'FAMILY_'):
                inv.validate_families(self.inventory, path)


if __name__ == '__main__':
    unittest.main()
