#!/usr/bin/env python3
"""Read pinned C++ metadata; emit portable dungeon inventory, never ROM or art data."""
from __future__ import annotations

import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

BASE = "src/zelda3/dungeon/"
REG = BASE + "draw_routines/draw_routine_registry.cc"
HDR = REG[:-2] + "h"
NAMES = BASE + "room_object.h"
PARSER = BASE + "object_parser.cc"
CUSTOM = BASE + "custom_object.cc"
MODULES = {name: BASE + "draw_routines/" + name.lower() + "_routines.cc"
           for name in ("Rightwards", "Downwards", "Diagonal", "Corner", "Special")}
DEFAULT_CONTRACT = Path(__file__).with_name("contracts") / "dungeon_inventory.sources.json"
STATUS = {"SOURCE_SEMANTICS", "SOURCE_NAMED_USE", "UNCLASSIFIED_MULTIPURPOSE_PRIMITIVE",
          "BASE_NO_DRAW_WITH_POSSIBLE_OVERRIDES", "EXTERNAL_CUSTOM_DATA_UNCLASSIFIED"}
NUM = r"(?:0x[0-9A-Fa-f]+|[0-9]+)"
VALUE = rf"(?:{NUM}|DrawRoutineIds::\w+)"


class InventoryError(ValueError):
    """Stable error code followed by a bounded explanation."""


def require(value, code, detail):
    if not value:
        raise InventoryError(f"{code}: {detail}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unique_pairs(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, "DUPLICATE_JSON_KEY", key)
        value[key] = item
    return value


def read_json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique_pairs)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise InventoryError(f"JSON_INPUT: {Path(path).name}: {error.__class__.__name__}") from error


def uncomment(text):
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0])
                  if m[0].startswith(('//', '/*')) else m[0], text)


def close_at(text, start, opening='{', closing='}'):
    require(start < len(text) and text[start] == opening, "SOURCE_GRAMMAR", "missing opener")
    depth, quote, index = 0, None, start
    while index < len(text):
        char = text[index]
        if quote:
            if char == '\\':
                index += 2
                continue
            if char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif char == opening:
            depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise InventoryError("SOURCE_GRAMMAR: unterminated definition")


def chunks(text):
    """Outer comma fields, preserving nested lambda expressions and offsets."""
    last, stack, quote, index = 0, [], None, 0
    while index < len(text):
        char = text[index]
        if quote:
            if char == '\\':
                index += 2
                continue
            if char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif char in '([{':
            stack.append(char)
        elif char in ')]}':
            require(stack and stack.pop() == {')': '(', ']': '[', '}': '{'}[char],
                    "SOURCE_GRAMMAR", "unbalanced field")
        elif char == ',' and not stack:
            yield last, text[last:index]
            last = index + 1
        index += 1
    require(not stack and quote is None, "SOURCE_GRAMMAR", "unbalanced fields")
    if text[last:].strip():
        yield last, text[last:]


def relative_source(path):
    value = Path(path)
    require(isinstance(path, str) and not value.is_absolute() and '..' not in value.parts
            and path.startswith(BASE) and value.suffix in ('.cc', '.h'),
            "SOURCE_CONTRACT", "invalid source path")
    return path


class Sources:
    def __init__(self, root, contract, usdasm_root=None):
        self.root = Path(root).resolve()
        require(contract.get('schema_version') == 1 and
                contract.get('kind') == 'yaze-dungeon-inventory-source-contract',
                "SOURCE_CONTRACT", "unsupported contract")
        self.contract = contract
        self.raw, self.text, self.clean = {}, {}, {}
        failures = []
        for entry in contract['source_files']:
            name = relative_source(entry['path'])
            require(name not in self.raw, "SOURCE_CONTRACT", "duplicate source path")
            path = (self.root / name).resolve()
            require(path.is_relative_to(self.root), "SOURCE_CONTRACT", "source escapes root")
            try:
                self.raw[name] = path.read_bytes()
            except OSError:
                failures.append(name + ' (missing)')
                continue
            if digest(self.raw[name]) != entry['sha256']:
                failures.append(name)
        self.usdasm = None
        if usdasm_root is not None:
            pin = contract['optional_usdasm_source']
            require(pin['path'] == 'bank_01.asm', "SOURCE_CONTRACT", "unexpected external source")
            try:
                self.usdasm = (Path(usdasm_root) / 'bank_01.asm').read_bytes()
            except OSError:
                failures.append('usdasm/bank_01.asm (missing)')
            if self.usdasm is not None and digest(self.usdasm) != pin['sha256']:
                failures.append('usdasm/bank_01.asm')
        # No source parsing occurs until *all* fingerprints, including an opted-in
        # external source, pass. Parse only these captured immutable bytes below.
        require(not failures, "SOURCE_FINGERPRINT_MISMATCH", ', '.join(failures))
        try:
            self.text = {name: data.decode('utf-8') for name, data in self.raw.items()}
        except UnicodeError as error:
            raise InventoryError("SOURCE_GRAMMAR: source is not UTF-8") from error
        self.clean = {name: uncomment(text) for name, text in self.text.items()}

    def line(self, path, position):
        return self.text[path].count('\n', 0, position) + 1

    def ref(self, path, position, end=None):
        result = {'path': path, 'line': self.line(path, position)}
        if end is not None:
            result['end_line'] = self.line(path, end)
        return result

    def body(self, path, signature):
        text = self.clean[path]
        require(text.count(signature) == 1, "SOURCE_GRAMMAR", f"unique definition required: {signature}")
        start = text.index('{', text.index(signature))
        end = close_at(text, start)
        return text[start + 1:end], start + 1, end

    def array(self, path, pattern):
        matches = list(re.finditer(pattern, self.clean[path]))
        require(len(matches) == 1, "SOURCE_GRAMMAR", "unique array required")
        start = matches[0].end() - 1
        end = close_at(self.clean[path], start)
        return self.clean[path][start + 1:end], start + 1, end

    def check_refs(self, value):
        if isinstance(value, dict):
            if 'path' in value and 'line' in value:
                path = value['path']
                require(path in self.text and 1 <= value['line'] <= len(self.text[path].splitlines()),
                        "SOURCE_REFERENCE", path)
                require(value['line'] <= value.get('end_line', value['line']) <= len(self.text[path].splitlines()),
                        "SOURCE_REFERENCE", "invalid end line")
            for child in value.values():
                self.check_refs(child)
        elif isinstance(value, list):
            for child in value:
                self.check_refs(child)


def parse_constants(source):
    body, _, _ = source.body(HDR, 'namespace DrawRoutineIds {')
    constants, pos = {}, 0
    while body[pos:].strip():
        match = re.match(r'\s*constexpr int (\w+)\s*=\s*(' + NUM + r')\s*;', body[pos:])
        require(match, "SOURCE_GRAMMAR", "unsupported routine constant")
        require(match[1] not in constants, "DUPLICATE_CONSTANT", match[1])
        constants[match[1]] = int(match[2], 0) if match[2].startswith('0x') else int(match[2])
        pos += match.end()
    return constants


def number(value, constants):
    value = value.strip()
    if re.fullmatch(NUM, value):
        return int(value, 0) if value.startswith('0x') else int(value)
    if value.startswith('DrawRoutineIds::') and value.split('::')[-1] in constants:
        return constants[value.split('::')[-1]]
    raise InventoryError('SOURCE_GRAMMAR: unsupported numeric expression')


def parse_mappings(source, constants):
    body, start, _ = source.body(REG, 'void DrawRoutineRegistry::BuildObjectMapping()')
    clear = re.match(r'\s*object_to_routine_map_\.clear\(\);', body)
    require(clear, "SOURCE_GRAMMAR", "mapping clear required")
    pos, mappings = clear.end(), {}
    loop = re.compile(r'\s*for\s*\(int id\s*(?:=\s*(' + NUM + r')\s*;\s*id\s*<=\s*(' + NUM +
                      r')\s*;\s*id\+\+|:\s*\{([^}]+)\})\s*\)\s*\{\s*'
                      r'object_to_routine_map_\[id\]\s*=\s*(' + VALUE + r');\s*\}')
    literal = re.compile(r'\s*object_to_routine_map_\[(' + NUM + r')\]\s*=\s*(' + VALUE + r');')
    while body[pos:].strip():
        match = loop.match(body, pos)
        if match:
            if match[1]:
                low, high = number(match[1], constants), number(match[2], constants)
                require(0 <= low <= high < 4096, "SOURCE_GRAMMAR", "invalid mapping range")
                ids = range(low, high + 1)
            else:
                ids = [number(value, constants) for _, value in chunks(match[3])]
            rid = number(match[4], constants)
        else:
            match = literal.match(body, pos)
            require(match, "SOURCE_GRAMMAR", "unsupported mapping statement/control flow")
            ids, rid = [number(match[1], constants)], number(match[2], constants)
        # Reference the statement's first token, not preceding whitespace.
        token = pos + len(body[pos:match.end()]) - len(body[pos:match.end()].lstrip())
        end = match.end() if match.re is loop else None
        ref = source.ref(REG, start + token, start + end if end is not None else None)
        for oid in ids:
            require(oid not in mappings, "DUPLICATE_OBJECT_MAPPING", hex(oid))
            mappings[oid] = (rid, ref)
        pos = match.end()
    return mappings


def parse_routines(source, constants):
    body, _, _ = source.body(REG, 'void DrawRoutineRegistry::BuildRegistry()')
    expected = ('routines_.clear(); routine_map_.clear(); ' +
                ' '.join(f'draw_routines::Register{name}Routines(routines_);' for name in MODULES) +
                ' for (auto& info : routines_) { routine_map_[info.id] = &info; } BuildObjectMapping();')
    require(re.sub(r'\s+', '', body) == re.sub(r'\s+', '', expected),
            "SOURCE_GRAMMAR", "unsupported registry builder/module set")
    routines = {}
    allowed = {'id', 'name', 'function', 'draws_to_both_bgs', 'base_width', 'base_height', 'min_tiles', 'category'}
    for module, path in MODULES.items():
        body, start, _ = source.body(path, 'void Register' + module + 'Routines(')
        alias = re.match(r'\s*using Category = DrawRoutineInfo::Category;', body)
        pos = alias.end() if alias else 0
        while body[pos:].strip():
            match = re.compile(r'\s*registry\.push_back\(\s*DrawRoutineInfo\s*\{').match(body, pos)
            require(match, "SOURCE_GRAMMAR", f"unsupported registration statement/control flow in {module} at line {source.line(path, start + pos)}: {body[pos:pos + 65].strip()}")
            opening = match.end() - 1
            end = close_at(body, opening)
            suffix = re.match(r'\s*\)\s*;', body[end + 1:])
            require(suffix, "SOURCE_GRAMMAR", "registration terminator")
            fields = {}
            for offset, chunk in chunks(body[opening + 1:end]):
                field = re.fullmatch(r'\s*\.(\w+)\s*=\s*([\s\S]*?)\s*', chunk)
                require(field and field[1] in allowed, "SOURCE_GRAMMAR", "unknown registration field")
                require(field[1] not in fields, "SOURCE_GRAMMAR", "duplicate registration field")
                fields[field[1]] = (field[2], start + opening + 1 + offset)
            require(allowed - {'min_tiles'} <= fields.keys(), "SOURCE_GRAMMAR", "missing registration field")
            rid = number(fields['id'][0], constants)
            require(rid not in routines, "DUPLICATE_ROUTINE_ID", str(rid))
            fn = fields['function'][0]
            identifier = re.fullmatch(r'\w+', fn)
            # Function bodies are opaque metadata, not executed/interpreted C++.
            require(identifier or re.fullmatch(r'\[\]\s*\(const DrawContext& ctx\)\s*\{[\s\S]*\}', fn),
                    "SOURCE_GRAMMAR", "unsupported function metadata expression")
            definition = None
            if identifier:
                definitions = list(re.finditer(r'\bvoid\s+' + re.escape(fn) + r'\s*\(', source.clean[path]))
                require(len(definitions) == 1, "SOURCE_GRAMMAR", "routine function definition")
                definition = source.ref(path, definitions[0].start())
            if definition is None:
                definition = source.ref(path, fields['function'][1])
            require(fields['draws_to_both_bgs'][0] in ('true', 'false'), "SOURCE_GRAMMAR", "BG boolean")
            category_expr = fields['category'][0]
            category_match = re.fullmatch(r'(?:DrawRoutineInfo::Category|Category)::(Special|Rightwards|Downwards|Diagonal|Corner)', category_expr)
            require(category_match and (not category_expr.startswith('Category::') or alias),
                    "SOURCE_GRAMMAR", "routine category")
            category = category_match[1]
            require(re.fullmatch(r'"(?:\\.|[^"\\])*"', fields['name'][0]),
                    "SOURCE_GRAMMAR", "routine name string literal")
            token = pos + len(body[pos:match.end()]) - len(body[pos:match.end()].lstrip())
            routines[rid] = {
                'id': rid, 'symbol': next((key for key, value in constants.items() if value == rid), None),
                'name': json.loads(fields['name'][0]), 'category': category,
                'draws_to_both_bgs_metadata': fields['draws_to_both_bgs'][0] == 'true',
                'base_width_chars': number(fields['base_width'][0], constants),
                'base_height_chars': number(fields['base_height'][0], constants),
                'min_payload_words': number(fields.get('min_tiles', ('0',))[0], constants),
                'function': fn if identifier else '<inline lambda>',
                'registration_source': source.ref(path, start + token, start + end),
                'function_source': definition}
            pos = end + 1 + suffix.end()
    return routines


def string_array(source, path, pattern):
    body, start, end = source.array(path, pattern)
    values = []
    for offset, chunk in chunks(body):
        require(re.fullmatch(r'\s*"(?:\\.|[^"\\])*"\s*', chunk), "SOURCE_GRAMMAR", "unsupported string array")
        token = offset + len(chunk) - len(chunk.lstrip())
        values.append((json.loads(chunk), source.ref(path, start + token)))
    return values, source.ref(path, start - 1, end)


def condition(expression):
    text = ' '.join(expression.replace('&&', ' and ').replace('||', ' or ').split())
    try:
        tree = ast.parse(text, mode='eval')
    except SyntaxError as error:
        raise InventoryError('SOURCE_GRAMMAR: invalid count condition') from error
    allowed = (ast.Expression, ast.BoolOp, ast.And, ast.Or, ast.Compare, ast.Name,
               ast.Load, ast.Constant, ast.GtE, ast.LtE, ast.Eq, ast.Gt, ast.Lt, ast.NotEq)
    require(all(isinstance(node, allowed) for node in ast.walk(tree)), "SOURCE_GRAMMAR", "unsupported condition")
    require(all(not isinstance(node, ast.Name) or node.id == 'object_id' for node in ast.walk(tree)),
            "SOURCE_GRAMMAR", "unexpected condition variable")
    require(all(not isinstance(node, ast.Constant) or type(node.value) is int for node in ast.walk(tree)),
            "SOURCE_GRAMMAR", "noninteger condition constant")
    return tree.body


def matches(node, oid):
    if isinstance(node, ast.Name):
        return oid
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.BoolOp):
        values = [matches(child, oid) for child in node.values]
        return all(values) if isinstance(node.op, ast.And) else any(values)
    if isinstance(node, ast.Compare):
        left = matches(node.left, oid)
        for operator, child in zip(node.ops, node.comparators):
            right = matches(child, oid)
            value = {ast.Eq: left == right, ast.NotEq: left != right,
                     ast.GtE: left >= right, ast.LtE: left <= right,
                     ast.Gt: left > right, ast.Lt: left < right}[type(operator)]
            if not value:
                return False
            left = right
        return True
    raise InventoryError('SOURCE_GRAMMAR: unexpected condition node')


def count_branches(source, typ):
    body, start, _ = source.body(PARSER, 'int ObjectParser::GetSubtype' + str(typ) + 'TileCount(')
    branches, pos = [], 0
    while body[pos:].strip():
        first = re.match(r'\s*if\s*\(', body[pos:])
        if first:
            opening = pos + first.end() - 1
            end = close_at(body, opening, '(', ')')
            tail = re.match(r'\s*\{\s*return\s+(' + NUM + r');\s*\}', body[end + 1:])
            require(tail, "SOURCE_GRAMMAR", "count branch must return one literal")
            token = pos + first[0].index('if')
            branches.append((condition(body[opening + 1:end]), int(tail[1], 0), source.ref(PARSER, start + token)))
            pos = end + 1 + tail.end()
            continue
        last = re.fullmatch(r'\s*return\s+(' + NUM + r');\s*', body[pos:])
        require(last, "SOURCE_GRAMMAR", "unconsumed count-function statements")
        return branches, (int(last[1], 0), source.ref(PARSER, start + pos + last[0].index('return')))
    raise InventoryError('SOURCE_GRAMMAR: count function lacks default')


def parse_counts(source):
    body, start, _ = source.array(PARSER, r'kSubtype1TileLengths\[0xF8\]\s*=\s*\{')
    values = []
    for offset, chunk in chunks(body):
        require(re.fullmatch(r'\s*' + NUM + r'\s*', chunk), "SOURCE_GRAMMAR", "count array literal")
        token = offset + len(chunk) - len(chunk.lstrip())
        values.append((int(chunk.strip(), 0), source.ref(PARSER, start + token)))
    require(len(values) == 248, "SOURCE_POPULATION", "subtype1 count array")
    helper, helper_start, _ = source.body(PARSER, 'static inline int GetSubtype1TileCount(int object_id)')
    expected = """int index = object_id & 0xFF;
      if (index < 0xF8) { int count = kSubtype1TileLengths[index];
        if (count > 0) { return count; }
        if (IsSubtype1LogicOnlyZeroTileObject(index)) { return 0; }
        return 8; } return 8;"""
    logic, _, _ = source.body(PARSER, 'static inline bool IsSubtype1LogicOnlyZeroTileObject(int object_id)')
    require(re.sub(r'\s+', '', helper) == re.sub(r'\s+', '', expected) and
            re.sub(r'\s+', '', logic) == 'intindex=object_id&0xFF;returnindex>=0xD3&&index<=0xD6;',
            "SOURCE_GRAMMAR", "unsupported subtype1 count helper")
    alias = helper_start + helper.rindex('return 8;')
    branches = {typ: count_branches(source, typ) for typ in (2, 3)}

    def lookup(oid, typ):
        if typ == 1:
            return values[oid] if oid < 248 else (8, source.ref(PARSER, alias))
        cases, default = branches[typ]
        return next(((value, ref) for expr, value, ref in cases if matches(expr, oid)), default)
    return lookup


def id_set(ranges):
    return {item for low, high in ranges for item in range(low, high + 1)}


def vanilla_reference(source, objects):
    if source.usdasm is None:
        return None
    text = source.usdasm.decode('utf-8')
    symbols, tables = {}, {}
    for match in re.finditer(r'^#?([A-Za-z_]\w*):\s*$', text, re.M):
        following = re.search(r'^#_([0-9A-F]{6}):', text[match.end():], re.M)
        if following:
            symbols[match[1]] = (following[1], text.count('\n', 0, match.start()) + 1)
    for match in re.finditer(r'^#_([0-9A-F]{6}):\s*dw\s+([^;\n]+)', text, re.M):
        tables[int(match[1], 16)] = (match[2].strip(), text.count('\n', 0, match.start()) + 1)
    for obj in objects:
        if not obj['room_stream_representable']:
            continue
        typ, oid = obj['subtype'], obj['id']
        index = oid if typ == 1 else oid - 0x100 if typ == 2 else oid - 0xF80
        routine_addr = (0x018200, 0x018470, 0x0185F0)[typ - 1] + index * 2
        data_addr = (0x018000, 0x0183F0, 0x0184F0)[typ - 1] + index * 2
        require(routine_addr in tables and data_addr in tables, "SOURCE_GRAMMAR", "vanilla pointer-table gap")
        label, table_line = tables[routine_addr]
        require(label in symbols, "SOURCE_GRAMMAR", "vanilla label missing")
        obj['vanilla_disassembly'] = {
            'routine_table_cpu': f'{routine_addr:06X}', 'routine_label': label,
            'routine_cpu': symbols[label][0], 'routine_table_line': table_line,
            'routine_definition_line': symbols[label][1], 'tile_data_table_cpu': f'{data_addr:06X}',
            'tile_data_expression': tables[data_addr][0], 'tile_data_table_line': tables[data_addr][1],
            'asm_object_number': f'0x{oid if typ != 3 else 0x200 + index:03X}'}
    return source.contract['optional_usdasm_source']


def checkout_commit(root):
    result = subprocess.run(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def build_inventory(source_root, contract_path=DEFAULT_CONTRACT, usdasm_root=None):
    contract = read_json(contract_path)
    source = Sources(source_root, contract, usdasm_root)
    constants = parse_constants(source)
    custom_lookup, _, _ = source.body(REG, 'int DrawRoutineRegistry::GetRoutineIdForObject(\n    int16_t object_id, bool custom_objects_enabled)')
    expected_lookup = """if (object_id == 0x31 || object_id == 0x32 || object_id == 0x54) {
      return custom_objects_enabled ? DrawRoutineIds::kCustomObject : DrawRoutineIds::kNothing;
      } auto it = object_to_routine_map_.find(object_id);
      if (it != object_to_routine_map_.end()) { return it->second; } return -1;"""
    require(re.sub(r'\s+', '', custom_lookup) == re.sub(r'\s+', '', expected_lookup) and
            contract['custom_family_ids'] == [0x31, 0x32, 0x54],
            "SOURCE_GRAMMAR", "unsupported custom lookup semantics")
    mappings = parse_mappings(source, constants)
    routines = parse_routines(source, constants)
    require(set(mappings) == id_set(contract['registry_ranges']), "SOURCE_POPULATION", "registry ID set")
    require(set(routines) == id_set([contract['routine_range']]), "SOURCE_POPULATION", "routine ID set")
    require(not ({rid for rid, _ in mappings.values()} - routines.keys()), "SOURCE_POPULATION", "unregistered mapping")
    names, name_counts = {}, {}
    for typ, base in ((1, 0), (2, 0x100), (3, 0xF80)):
        values, _ = string_array(source, NAMES, r'Type' + str(typ) + r'RoomObjectNames\[\]\s*=\s*\{')
        name_counts[str(typ)] = len(values)
        names.update({base + index: value for index, value in enumerate(values)})
    valid = id_set(contract['serializable_ranges'])
    require(set(names) == valid, "SOURCE_POPULATION", "canonical name/stream ID set")
    annotations = contract['annotations']
    source.check_refs(annotations)
    count = parse_counts(source)
    objects = []
    for oid, (rid, ref) in sorted(mappings.items()):
        typ = 1 if oid < 0x100 else 2 if oid < 0x200 else 3
        words, words_ref = count(oid, typ)
        objects.append({
            'id': oid, 'hex': f'0x{oid:03X}', 'subtype': typ,
            'name': names.get(oid, (f'Unknown Type1 (0x{oid:02X})', None))[0],
            'name_source': names.get(oid, (None, None))[1], 'room_stream_representable': oid in valid,
            'status': 'stream-representable' if oid in valid else 'registry-only-alias-rejected-by-save-validator',
            'routine_id_default': rid, 'routine_name_default': routines[rid]['name'],
            'routine_id_custom_enabled': 130 if oid in contract['custom_family_ids'] else rid,
            'mapping_source': ref, 'parser_payload_word_count': words,
            'parser_payload_count_source': words_ref,
            'dispatch_exception_keys': [item['key'] for item in annotations['dispatch_exceptions'] if oid in item['ids']],
            'actual_character_usage': 'NOT_MEASURED', 'vanilla_disassembly': None})
    custom = []
    for oid, symbol in ((0x31, 'kSubtype1Filenames'), (0x32, 'kSubtype2Filenames'), (0x54, 'kSubtype54Filenames')):
        values, array_ref = string_array(source, CUSTOM, re.escape(symbol) + r'\s*=\s*\{')
        # Preserve the audit's declaration reference, not just the initializer.
        array_ref['line'] = source.line(CUSTOM, source.clean[CUSTOM].index(symbol))
        custom.append({'id': oid, 'hex': f'0x{oid:03X}', 'default_routine': 38, 'enabled_routine': 130,
                       'default_slots': [{'subtype': index, 'filename': value[0]} for index, value in enumerate(values)],
                       'source': array_ref,
                       'tile_word_transform': 'source_word==0 remains no-op; otherwise OR0x0300' if oid == 0x54
                       else 'nonzero source tile word unchanged'})
    used = {obj['routine_id_default'] for obj in objects}
    enabled = {obj['routine_id_custom_enabled'] for obj in objects}
    counts = {'mapped_registry_objects': len(objects), 'room_stream_representable_objects': len(valid),
              'registry_only_aliases': len(objects) - len(valid), 'name_array_counts': name_counts,
              'registered_routines': len(routines), 'default_mapping_routines_used': len(used),
              'custom_enabled_mapping_routines_used': len(enabled), 'unregistered_mapped_routine_ids': sorted(used - routines.keys()),
              'registered_not_used_by_default_mapping': sorted(routines.keys() - used),
              'mapped_to_nothing_default': sum(obj['routine_id_default'] == 38 for obj in objects),
              'canonical_stream_unmapped_ids': sorted(valid - mappings.keys()),
              'unmapped_metadata_range_within_12bit': {'first': '0x140', 'last': '0xF7F', 'count': 3648},
              'custom_runtime_families': len(custom), 'custom_default_subtype_slots': sum(len(item['default_slots']) for item in custom)}
    require(counts == contract['expected_counts'], "SOURCE_POPULATION", "counts differ from reviewed contract")
    inventory = {'schema_version': 1, 'kind': 'portable-source-backed-dungeon-object-metadata-inventory',
                 'source_root': '.', 'source_repository': 'yaze', 'source_commit': contract['source_commit'],
                 'checkout_commit': checkout_commit(source.root), 'source_files': contract['source_files'],
                 'method': 'Pinned source metadata; complete finite mapping/registration/array/count grammar; reviewed dispatcher annotations. No C++ execution, ROM or artwork reads.',
                 'counts': counts, **annotations, 'custom_families': custom,
                 'routines': [routines[rid] for rid in sorted(routines)], 'objects': objects,
                 'vanilla_disassembly_source': vanilla_reference(source, objects),
                 'layer_semantics': {'YazeBG1': '$7E2000 / hardware BG2', 'YazeBG2': '$7E4000 / hardware BG1',
                                     'rule': 'BothBG metadata, ordered stream list, fixed/mixed dispatcher routing, flags and context all matter; names alone do not establish layer coverage.'},
                 'actual_character_usage': 'NOT_MEASURED'}
    source.check_refs(inventory)
    return inventory


def validate_profile(profile, source_root, usdasm_root=None):
    """Check profile provenance and draft ID accounting, not pixel or hardware parity."""
    require(profile.get('schema_version') == 1, "FAMILY_PROFILE", "unsupported profile")
    roots = {'yaze': Path(source_root).resolve()}
    if usdasm_root is not None:
        roots['usdasm'] = Path(usdasm_root).resolve()
    checked, unbound, cached = Counter(), [], {}
    sources = profile['sources']
    require(isinstance(sources, dict) and sources, "FAMILY_PROFILE", "source references required")
    for ref_id, item in sources.items():
        root_name, relative = item['root'], Path(item['path'])
        require(root_name in ('yaze', 'usdasm') and not relative.is_absolute() and
                '..' not in relative.parts, "FAMILY_PROFILE", "invalid source root/path")
        require((root_name == 'yaze' and relative.as_posix().startswith('src/') and relative.suffix in ('.cc', '.h')) or
                (root_name == 'usdasm' and relative.as_posix() == 'bank_01.asm'),
                "FAMILY_PROFILE", "unsupported profile source")
        require(re.fullmatch(r'[0-9a-f]{64}', item['sha256']), "FAMILY_PROFILE", "invalid source hash")
        low, high = item['lines']
        require(type(low) is int and type(high) is int and 1 <= low <= high,
                "FAMILY_PROFILE", "invalid source line range")
        if root_name not in roots:
            unbound.append(ref_id)
            continue
        key = (root_name, relative.as_posix())
        if key not in cached:
            path = (roots[root_name] / relative).resolve()
            require(path.is_relative_to(roots[root_name]), "FAMILY_PROFILE", "source escapes root")
            try:
                cached[key] = path.read_bytes()
            except OSError as error:
                raise InventoryError('FAMILY_SOURCE_MISMATCH: missing ' + relative.as_posix()) from error
        data = cached[key]
        require(digest(data) == item['sha256'], "FAMILY_SOURCE_MISMATCH", relative.as_posix())
        require(high <= len(data.decode('utf-8').splitlines()), "FAMILY_PROFILE", "source range exceeds file")
        checked[root_name] += 1

    def refs(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == 'source_refs':
                    require(isinstance(child, list) and set(child) <= sources.keys(),
                            "FAMILY_PROFILE", "unknown source reference ID")
                refs(child)
        elif isinstance(value, list):
            for child in value:
                refs(child)
    refs(profile)
    families = profile['families']
    ids = {item['id'] for item in families}
    require(len(ids) == len(families) == 24, "FAMILY_PROFILE", "expected24 unique family IDs")
    modules = profile['draft_modules']
    module_ids = {item['id'] for item in modules}
    modules_by_id = {item['id']: item for item in modules}
    require(len(module_ids) == len(modules), "FAMILY_PROFILE", "duplicate draft module ID")
    for family in families:
        require(set(family['draft_modules']) <= module_ids and family['source_refs'],
                "FAMILY_PROFILE", "family module/source references required")
        require(all(family['id'] in modules_by_id[mid]['family_ids'] for mid in family['draft_modules']),
                "FAMILY_PROFILE", "family/module backlink mismatch")
        require(family['engine_status'] == 'UNMAPPED_UNVERIFIED' and
                family['art_status'] in ('DRAWN_SAMPLES_ONLY', 'MISSING'),
                "FAMILY_PROFILE", "unverified family statuses required")
    for module in modules:
        require(module['family_ids'] and set(module['family_ids']) <= ids,
                "FAMILY_PROFILE", "unknown draft module family")
        require(module['engine_status'] == 'UNMAPPED_UNVERIFIED' and module['join_status'] == 'UNVERIFIED' and
                module['art_status'] == 'DRAWN', "FAMILY_PROFILE", "draft status mismatch")
        require(all(module['id'] in family['draft_modules'] for family in families
                    if family['id'] in module['family_ids']), "FAMILY_PROFILE", "module/family backlink mismatch")
    palettes = profile['palette_contract']
    bindings = palettes['per_family_bindings']
    require(len(bindings) == len(ids) and {item['family_id'] for item in bindings} == ids,
            "FAMILY_PROFILE", "one palette binding per family required")
    unallocated = ('palette_set', 'main_palette_id', 'tilemap_bank', 'sheet_slot', 'half', 'index_to_cgram')
    require(not palettes['production_palette_sets'] and all(item['status'] == 'UNALLOCATED' and
            all(item[key] is None for key in unallocated) for item in bindings),
            "FAMILY_PROFILE", "production palette remains unallocated")
    budget = profile['tile_budget']
    require(budget['status'] == 'UNALLOCATED' and budget['production_max_characters'] is None and
            budget['production_max_unique_characters'] is None,
            "FAMILY_PROFILE", "production tile budget remains unallocated")
    return {'status': 'PASS_SOURCE_REFS_AND_DRAFT_IDS_ONLY', 'checked_yaze_source_refs': checked['yaze'],
            'checked_usdasm_source_refs': checked['usdasm'], 'unbound_optional_usdasm_refs': unbound,
            'families': len(ids), 'draft_modules': len(module_ids), 'production_palette_bindings': 'UNALLOCATED',
            'actual_character_usage': 'NOT_MEASURED'}


def validate_families(inventory, bindings_path, profile_path=None, source_root=None, usdasm_root=None):
    bindings_path = Path(bindings_path)
    data = read_json(bindings_path)
    require(data.get('schema_version') == 1 and data.get('kind') == 'routine_to_style_family_accounting',
            "FAMILY_BINDING", "unsupported bindings")
    require(data.get('source_commit') == inventory['source_commit'] and data.get('source_files') == inventory['source_files'],
            "FAMILY_SOURCE_MISMATCH", "binding source provenance differs")
    profile = read_json(profile_path or bindings_path.parent / data['family_profile'])
    profile_result = validate_profile(profile, source_root or Path(__file__).resolve().parents[2], usdasm_root)
    families = profile['families']
    ids = {item['id'] for item in families}
    rows = {}
    routines = {row['id']: row for row in inventory['routines']}
    for row in data['bindings']:
        rid = row['routine_id']
        require(rid not in rows and rid in routines, "FAMILY_BINDING", "duplicate/unknown routine ID")
        require(row['routine_name'] == routines[rid]['name'], "FAMILY_BINDING", "routine name mismatch")
        require(row['classification_status'] in STATUS, "FAMILY_BINDING", "unknown classification status")
        require(row['primary_family_id'] is None or row['primary_family_id'] in ids, "FAMILY_BINDING", "unknown primary family")
        require(set(row['secondary_family_ids']) <= ids and
                len(set(row['secondary_family_ids'])) == len(row['secondary_family_ids']),
                "FAMILY_BINDING", "unknown/duplicate secondary family")
        require(row['classification_status'] not in ('SOURCE_SEMANTICS', 'SOURCE_NAMED_USE') or
                row['primary_family_id'] in ids, "FAMILY_BINDING", "source classification needs a family")
        expected_source = {'function': routines[rid]['function'], 'definition': routines[rid]['function_source'],
                           'registration': routines[rid]['registration_source'], 'source_commit': inventory['source_commit']}
        require(row['source_basis'] == expected_source, "FAMILY_BINDING", "routine source metadata mismatch")
        expected_exceptions = {key for obj in inventory['objects']
                               if obj['routine_id_default'] == rid or obj['routine_id_custom_enabled'] == rid
                               for key in obj['dispatch_exception_keys']}
        require(set(row['dispatch_exception_keys']) == expected_exceptions, "FAMILY_BINDING", "dispatch exception union mismatch")
        for field, key, serializable in [('default_object_ids', 'routine_id_default', False),
                                         ('custom_enabled_object_ids', 'routine_id_custom_enabled', False),
                                         ('default_serializable_object_ids', 'routine_id_default', True)]:
            expected = [obj['id'] for obj in inventory['objects'] if obj[key] == rid
                        and (not serializable or obj['room_stream_representable'])]
            if field in row:
                require(row[field] == expected, "FAMILY_BINDING", field + ' mismatch')
        require(row.get('actual_character_usage', data.get('actual_character_usage')) == 'NOT_MEASURED', "FAMILY_BINDING", "character usage must remain unmeasured")
        rows[rid] = row
    require(rows.keys() == routines.keys(), "FAMILY_BINDING", "missing routine bindings")
    for obj in inventory['objects']:
        require(obj['routine_id_default'] in rows and obj['routine_id_custom_enabled'] in rows,
                "FAMILY_BINDING", "unresolved object route")
    expected_counts = {'registered_routines': len(rows), 'objects_joined': len(inventory['objects']),
                       'serializable_objects_joined': sum(obj['room_stream_representable'] for obj in inventory['objects'])}
    require(all(data['counts'].get(key) == value for key, value in expected_counts.items()),
            "FAMILY_BINDING", "declared join counts differ")
    if 'classification_statuses' in data['counts']:
        require(dict(Counter(row['classification_status'] for row in rows.values())) == data['counts']['classification_statuses'],
                "FAMILY_BINDING", "classification counts differ")
    return {'status': 'PASS_METADATA_JOIN_ONLY', **expected_counts, 'custom_enabled_objects_joined': len(inventory['objects']),
            'families': len(ids), 'profile_validation': profile_result, 'actual_character_usage': 'NOT_MEASURED'}


def publish(output, payload, source_root):
    output = Path(output).absolute()
    require(output.suffix == '.json', "OUTPUT_PATH", "output must be a JSON artifact")
    require(not output.resolve().is_relative_to(Path(source_root).resolve()), "OUTPUT_PATH", "output inside source tree refused")
    require(output.parent.is_dir(), "OUTPUT_PATH", "parent directory must already exist")
    try:
        # Exclusive creation rejects existing files, symlinks, and output races.
        with output.open('xb') as stream:
            stream.write(payload)
    except FileExistsError as error:
        raise InventoryError('OUTPUT_EXISTS: choose a fresh artifact path') from error


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--contract', type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument('--usdasm-root', type=Path, help='explicit optional pinned vanilla source checkout')
    parser.add_argument('--family-bindings', type=Path)
    parser.add_argument('--family-profile', type=Path)
    parser.add_argument('--output', type=Path, help='new JSON artifact outside source tree; existing paths refused')
    parser.add_argument('--check', action='store_true', help='print validation summary instead of full inventory')
    args = parser.parse_args(argv)
    try:
        require(not args.family_profile or args.family_bindings, "FAMILY_BINDING", "--family-profile requires --family-bindings")
        inventory = build_inventory(args.source_root, args.contract, args.usdasm_root)
        if args.family_bindings:
            inventory['family_binding_validation'] = validate_families(inventory, args.family_bindings, args.family_profile, args.source_root, args.usdasm_root)
        payload = (json.dumps(inventory, indent=2, ensure_ascii=False) + '\n').encode('utf-8')
        if args.output:
            publish(args.output, payload, args.source_root)
        if args.check:
            print(json.dumps({'status': 'PASS_SOURCE_METADATA_ONLY', 'counts': inventory['counts'],
                              'inventory_sha256': digest(payload), 'inventory_bytes': len(payload),
                              'family_binding_validation': inventory.get('family_binding_validation'),
                              'actual_character_usage': 'NOT_MEASURED'}, indent=2))
        elif not args.output:
            sys.stdout.buffer.write(payload)
        return 0
    except (InventoryError, OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(str(error) if isinstance(error, InventoryError) else 'INVALID_INPUT: ' + error.__class__.__name__, file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
