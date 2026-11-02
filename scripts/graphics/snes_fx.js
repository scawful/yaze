#!/usr/bin/env node
'use strict';

// SNES-safe effect frames: an indexed painter, hardware checks, and an indexed PNG strip for `z3ed gfx-import`.
// Kept from pixelfx (MIT, github.com/patch-gg/pixelfx): seeded closed-form particles, ordered-dither fades,
// dark-to-bright ramps, a check pass. Dropped: translucency, additive light, rotation, 120 Hz simulation,
// free-size canvases. Every pixel here is an OBJ palette index; 0 is transparent.
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const crypto = require('node:crypto');
const { execFileSync } = require('node:child_process');

const fail = message => { throw new Error(message); };
const integer = n => Number.isSafeInteger(n);
const sha256 = bytes => crypto.createHash('sha256').update(Buffer.from(bytes)).digest('hex');
const hex2 = n => n.toString(16).toUpperCase().padStart(2, '0');

// Graphics sheets are 3bpp, so OBJ colors 1..7 are all a frame can use.
const MAX_INDEX = 7;
// A sheet holds 16 blocks of 16x16 (zelda3/gfx_sheet_png.h); one block is one 16x16 OBJ cell.
const SHEET_BLOCKS = 16;
// A spriteset slot is one sheet: 64 8x8 tiles in the second OBJ name table (OAM prop bit 0).
const SLOT_TILES = 64;
// A ROM color further than this (redmean RGB distance, 0..765) from the effect color it replaces gets a warning.
const FAR_COLOR = 80;
// 4x4 ordered (Bayer) thresholds: a pixel at alpha a is drawn when a is above its cell's threshold.
const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5].map(v => (v + 0.5) / 16);
const TAU = Math.PI * 2;

// mulberry32: a fresh rng with the same seed for every frame, so particle i is the same particle in each.
function rng(seed) {
  let s = seed >>> 0;
  const next = () => {
    s = (s + 0x6D2B79F5) >>> 0;
    let t = Math.imul(s ^ (s >>> 15), 1 | s);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  return { next, range: (a, b) => a + (b - a) * next(), sign: () => (next() < 0.5 ? -1 : 1) };
}

// SNES color: 5 bits per channel, BGR order. Same rule as gfx::ConvertRgbToSnes (channel / 8).
function toBgr555(hexColor) {
  if (!/^#[0-9a-fA-F]{6}$/.test(hexColor)) fail(`palette: ${hexColor} is not #rrggbb`);
  const v = parseInt(hexColor.slice(1), 16), rgb = [(v >> 16) & 255, (v >> 8) & 255, v & 255];
  const c5 = rgb.map(c => c >> 3), back = c5.map(c => (c << 3) | (c >> 2));
  return { source: hexColor, word: (c5[2] << 10) | (c5[1] << 5) | c5[0], rgb: back, exact: back.every((c, i) => c === rgb[i]) };
}

function fromBgr555(word) {
  const rgb = [word & 31, (word >> 5) & 31, (word >> 10) & 31].map(c => (c << 3) | (c >> 2));
  return { source: `#${rgb.map(hex2).join('')}`, word, rgb, exact: true };
}

// Redmean distance: cheap and closer to how far apart two colors look than plain RGB distance.
function colorDistance(a, b) {
  const r = (a[0] + b[0]) / 2, [dr, dg, db] = a.map((c, i) => c - b[i]);
  return Math.sqrt((2 + r / 256) * dr * dr + 4 * dg * dg + (2 + (255 - r) / 256) * db * db);
}

// Up to 7 colors of a ROM palette, read with `z3ed palette-get-colors`. spec is GROUP:INDEX[:FIRST]; FIRST picks
// where the 7 colors start in a group with longer palettes (global_sprites has 60). Color k becomes OBJ index k+1.
function readRomPalette(rom, spec, z3ed = 'z3ed') {
  const m = /^([a-z0-9_]+):(\d+)(?::(\d+))?$/.exec(spec);
  if (!m) fail(`--palette: expected GROUP:INDEX[:FIRST], got ${spec}`);
  const [, group, index, first = '0'] = m;
  let out;
  try {
    out = execFileSync(z3ed, ['palette-get-colors', `--rom=${rom}`, '--group', group, '--index', index, '--format', 'json'], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
  } catch (error) {
    fail(`--palette: ${z3ed} palette-get-colors failed: ${(error.stderr || error.message).trim().split('\n')[0]}`);
  }
  const words = (JSON.parse(out)['Palette Colors']?.colors ?? []).map(c => /SNES=0x([0-9A-Fa-f]{4})/.exec(c)?.[1]);
  if (!words.length || words.some(w => !w)) fail(`--palette: no colors in z3ed output for ${group}:${index}`);
  const colors = words.slice(Number(first), Number(first) + MAX_INDEX).map(w => fromBgr555(parseInt(w, 16)));
  if (!colors.length) fail(`--palette: ${group}:${index} has ${words.length} colors; FIRST ${first} is past the end`);
  return { group, index: Number(index), first: Number(first), colors };
}

// Map each effect color to the nearest ROM color, so the frames use colors the game already loads.
function remapToRom(palette, rom) {
  return palette.map((c, i) => {
    const d = rom.colors.map(r => colorDistance(c.rgb, r.rgb)), j = d.indexOf(Math.min(...d));
    return { from: i + 1, source: c.source, to: j + 1, rom: rom.colors[j].source, distance: Math.round(d[j]) };
  });
}

// Draws palette indices at local coordinates around the effect's origin. No blending: alpha below 1 is an
// ordered-dither fade. Pixels that land outside the canvas are counted as spill, because the OBJ cuts them off.
class Painter {
  constructor(width, height, origin, maxIndex, flip = false) {
    Object.assign(this, { width, height, ox: origin[0], oy: origin[1], maxIndex, flip, spill: 0 });
    this.pixels = new Uint8Array(width * height);
  }
  put(x, y, index, alpha = 1) {
    if (!integer(index) || index < 0 || index > this.maxIndex) fail(`index ${index}: outside 0..${this.maxIndex}`);
    if (index === 0 || alpha <= 0) return;
    const sx = this.flip ? this.ox - 1 - Math.floor(x) : this.ox + Math.floor(x), sy = this.oy + Math.floor(y);
    if (alpha < 1 && alpha <= BAYER[((sy & 3) << 2) | (sx & 3)]) return;
    if (sx < 0 || sy < 0 || sx >= this.width || sy >= this.height) { this.spill++; return; }
    this.pixels[sy * this.width + sx] = index;
  }
  disc(cx, cy, r, index, alpha = 1) {
    if (r < 0.5) return this.put(cx, cy, index, alpha);
    for (let y = Math.floor(cy - r); y <= Math.ceil(cy + r); y++) {
      for (let x = Math.floor(cx - r); x <= Math.ceil(cx + r); x++) {
        if ((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r) this.put(x, y, index, alpha);
      }
    }
  }
  ring(cx, cy, r, width, index, alpha = 1) {
    for (let y = Math.floor(cy - r); y <= Math.ceil(cy + r); y++) {
      for (let x = Math.floor(cx - r); x <= Math.ceil(cx + r); x++) {
        const d = Math.hypot(x + 0.5 - cx, y + 0.5 - cy);
        if (d <= r && d > r - width) this.put(x, y, index, alpha);
      }
    }
  }
  // Fill transparent pixels that touch the silhouette (4-neighbours). Stays inside the canvas.
  outline(index) {
    const { width: w, height: h } = this, src = this.pixels.slice();
    const on = (x, y) => x >= 0 && y >= 0 && x < w && y < h && src[y * w + x] !== 0;
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        if (!src[y * w + x] && (on(x - 1, y) || on(x + 1, y) || on(x, y - 1) || on(x, y + 1))) this.pixels[y * w + x] = index;
      }
    }
  }
}

// 8x8 tile keys, plus the keys of its H, V and HV flips, so OAM flip bits can share one tile.
function tileOf(pixels, width, tx, ty) {
  const rows = [];
  for (let y = 0; y < 8; y++) rows.push(Array.from(pixels.subarray((ty + y) * width + tx, (ty + y) * width + tx + 8)));
  const key = r => r.map(row => row.join('')).join('/');
  const h = rows.map(row => [...row].reverse()), v = [...rows].reverse(), hv = [...h].reverse();
  return { empty: rows.every(row => row.every(p => p === 0)), key: key(rows), flips: [key(h), key(v), key(hv)] };
}

function validateEffect(fx) {
  if (typeof fx.name !== 'string' || !/^[a-z0-9_-]+$/.test(fx.name)) fail('name: expected lowercase id');
  const [w, h] = fx.size ?? [];
  if (![16, 32].includes(w) || ![16, 32].includes(h)) fail('size: each side must be 16 or 32 (16x16 OBJ cells)');
  if (!Array.isArray(fx.origin) || fx.origin.length !== 2 || !fx.origin.every(integer)) fail('origin: expected integer [x,y]');
  if (!Array.isArray(fx.holds) || !fx.holds.length || !fx.holds.every(n => integer(n) && n >= 1 && n <= 255)) {
    fail('holds: expected 1..255 game frames (60 Hz) per frame');
  }
  if (!Array.isArray(fx.palette) || fx.palette.length < 1 || fx.palette.length > MAX_INDEX) {
    fail(`palette: expected 1..${MAX_INDEX} colors for OBJ indices 1..${MAX_INDEX}`);
  }
  if (!integer(fx.seed)) fail('seed: expected an integer');
  if (typeof fx.draw !== 'function') fail('draw: expected draw(p, k, t, rng)');
}

function renderFrames(fx, flip) {
  const n = fx.holds.length;
  return fx.holds.map((hold, k) => {
    const p = new Painter(fx.size[0], fx.size[1], fx.origin, fx.palette.length, flip);
    fx.draw(p, k, n === 1 ? 0 : k / (n - 1), rng(fx.seed));
    return { hold, pixels: p.pixels, spill: p.spill };
  });
}

// Cell i goes to sheet block firstBlock + i. Block b holds 8x8 tiles n, n+1, n+16, n+17 (GfxSheetBlockTiles).
function layoutBlocks(firstBlock, count) {
  if (firstBlock + count > SHEET_BLOCKS) {
    fail(`--block ${firstBlock}: ${count} cells need blocks ${firstBlock}..${firstBlock + count - 1}; a sheet has 0..${SHEET_BLOCKS - 1}`);
  }
  return Array.from({ length: count }, (_, i) => {
    const block = firstBlock + i, n = (block >> 3) * 32 + (block & 7) * 2;
    return { block, tiles: [n, n + 1, n + 16, n + 17] };
  });
}

function evaluateEffect(fx, { firstBlock = 0, romPalette = null } = {}) {
  validateEffect(fx);
  const errors = [], warnings = [];
  const fxPalette = fx.palette.map(toBgr555);
  const frames = renderFrames(fx, false);
  const again = renderFrames(fx, false);
  frames.forEach((f, k) => {
    if (f.spill) errors.push(`frame ${k}: ${f.spill} pixels outside the ${fx.size.join('x')} OBJ (they would be cut off)`);
    if (sha256(f.pixels) !== sha256(again[k].pixels)) errors.push(`frame ${k}: not deterministic (unseeded randomness)`);
  });
  let palette = fxPalette, remap = null;
  if (romPalette) {
    remap = remapToRom(fxPalette, romPalette);
    const lut = Uint8Array.from([0, ...remap.map(r => r.to)]);
    frames.forEach(f => { f.pixels = f.pixels.map(i => lut[i]); });
    palette = romPalette.colors;
    remap.forEach(r => {
      if (r.distance > FAR_COLOR) warnings.push(`palette ${r.from}: ${r.source} -> ROM ${r.to} ${r.rom} is far (distance ${r.distance})`);
    });
    for (let to = 1; to <= palette.length; to++) {
      const merged = remap.filter(r => r.to === to).map(r => r.from);
      if (merged.length > 1) warnings.push(`palette ${merged.join(', ')}: all map to ROM ${to}, so their shading merges`);
    }
  } else {
    palette.forEach((c, i) => { if (!c.exact) warnings.push(`palette ${i + 1}: ${c.source} snaps to #${c.rgb.map(hex2).join('')}`); });
  }
  const [w, h] = fx.size, seen = new Map();
  let tiles = 0, emptyTiles = 0, edgeFrames = 0;
  const report = frames.map((f, k) => {
    const used = [...new Set(f.pixels)].filter(i => i).sort();
    const objects = [];
    for (let cy = 0; cy < h; cy += 16) {
      for (let cx = 0; cx < w; cx += 16) {
        let empty = true;
        for (let ty = cy; ty < cy + 16; ty += 8) {
          for (let tx = cx; tx < cx + 16; tx += 8) {
            const t = tileOf(f.pixels, w, tx, ty);
            tiles++;
            if (t.empty) { emptyTiles++; continue; }
            empty = false;
            if (![t.key, ...t.flips].some(key => seen.has(key))) seen.set(t.key, `${k}:${tx},${ty}`);
          }
        }
        objects.push({ x: cx, y: cy, empty });
      }
    }
    let edge = false;
    for (let i = 0; i < w; i++) edge ||= !!(f.pixels[i] || f.pixels[(h - 1) * w + i]);
    for (let i = 0; i < h; i++) edge ||= !!(f.pixels[i * w] || f.pixels[i * w + w - 1]);
    if (edge) edgeFrames++;
    if (objects.every(o => o.empty)) warnings.push(`frame ${k}: empty`);
    return { frame: k, hold: f.hold, colors: used, oam: objects.filter(o => !o.empty).length, touchesEdge: edge, pixelSha256: sha256(f.pixels), objects };
  });
  if (edgeFrames) warnings.push(`${edgeFrames} frame(s) touch the OBJ edge: an outline there is cut`);
  const cells = frames.flatMap((f, k) => report[k].objects.map(o => ({ frame: k, cx: o.x, cy: o.y, empty: o.empty })));
  layoutBlocks(firstBlock, cells.length).forEach((at, i) => Object.assign(cells[i], at));
  return {
    frames,
    cells,
    result: {
      name: fx.name,
      size: fx.size,
      origin: fx.origin,
      seed: fx.seed,
      gameFrames: fx.holds.reduce((a, b) => a + b, 0),
      palette: palette.map((c, i) => ({ index: i + 1, source: c.source, bgr555: `$${c.word.toString(16).toUpperCase().padStart(4, '0')}`, exact: c.exact })),
      ...(romPalette && { romPalette: { group: romPalette.group, index: romPalette.index, first: romPalette.first }, remap }),
      tiles: { total: tiles, empty: emptyTiles, uniqueWithFlips: seen.size },
      frames: report.map(({ objects, ...r }) => r),
      cells: cells.map(c => ({ frame: c.frame, x: c.cx, y: c.cy, empty: c.empty, block: c.block, tiles: c.tiles.map(t => `$${hex2(t)}`) })),
      errors,
      warnings,
    },
  };
}

const CRC_TABLE = Array.from({ length: 256 }, (_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});
function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (const b of bytes) c = CRC_TABLE[(c ^ b) & 255] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}
function pngChunk(type, data) {
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]), out = Buffer.alloc(body.length + 8);
  out.writeUInt32BE(data.length, 0);
  body.copy(out, 4);
  out.writeUInt32BE(crc32(body), body.length + 4);
  return out;
}

// 8-bit indexed PNG (color type 3), the form util::DecodePng keeps index for index. `palette` is [r,g,b,a]
// entries; tRNS is written up to the last entry that is not opaque.
function encodeIndexedPng(width, height, indices, palette) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;
  ihdr[9] = 3;
  const raw = Buffer.alloc((width + 1) * height);
  for (let y = 0; y < height; y++) raw.set(indices.subarray(y * width, (y + 1) * width), y * (width + 1) + 1);
  const alphaEnd = palette.reduce((end, c, i) => (c[3] !== 255 ? i + 1 : end), 0);
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    pngChunk('IHDR', ihdr),
    pngChunk('PLTE', Buffer.from(palette.flatMap(c => c.slice(0, 3)))),
    ...(alphaEnd ? [pngChunk('tRNS', Buffer.from(palette.slice(0, alphaEnd).map(c => c[3])))] : []),
    pngChunk('IDAT', zlib.deflateSync(raw)),
    pngChunk('IEND', Buffer.alloc(0)),
  ]);
}

// Index 0 transparent, 1..n the effect's colors as the SNES shows them, unused entries magenta.
function objPalette(paletteRgb) {
  return Array.from({ length: MAX_INDEX + 1 }, (_, i) => (i === 0 ? [0, 0, 0, 0] : [...(paletteRgb[i - 1] ?? [255, 0, 255]), 255]));
}

// One row of 16x16 cells in block order. gfx-import reads every block in the PNG, so there is no padding.
function blockStrip(fx, frames, cells, paletteRgb) {
  const w = fx.size[0], width = cells.length * 16, indices = new Uint8Array(width * 16);
  cells.forEach((c, i) => {
    const px = frames[c.frame].pixels;
    for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) indices[y * width + i * 16 + x] = px[(c.cy + y) * w + c.cx + x];
  });
  return encodeIndexedPng(width, 16, indices, objPalette(paletteRgb));
}

// Frames side by side at `scale`, 8x8 tile grid in the gutter color, a dark backdrop for transparency.
function contactSheet(fx, frames, paletteRgb, scale) {
  const GUTTER = 8, GRID = 9, BACKDROP = 10;
  const [w, h] = fx.size, gap = 2 * scale, width = frames.length * (w * scale + gap) + gap, height = h * scale + 2 * gap;
  const indices = new Uint8Array(width * height).fill(GUTTER);
  frames.forEach((f, k) => {
    const x0 = gap + k * (w * scale + gap), y0 = gap;
    for (let y = 0; y < h * scale; y++) {
      for (let x = 0; x < w * scale; x++) {
        const i = f.pixels[Math.floor(y / scale) * w + Math.floor(x / scale)];
        const grid = scale >= 4 && (x % (8 * scale) === 0 || y % (8 * scale) === 0);
        indices[(y0 + y) * width + x0 + x] = i || (grid ? GRID : BACKDROP);
      }
    }
  });
  const palette = [...objPalette(paletteRgb)];
  palette[0] = [24, 24, 30, 255];
  palette.push([70, 70, 78, 255], [40, 40, 48, 255], [24, 24, 30, 255]);
  return encodeIndexedPng(width, height, indices, palette);
}

// Draw tables for Oracle's %DrawSprite() (Core/sprite_macros.asm): frame k is SprGfx+SprFrame = k, one 16x16 OAM
// object per non-empty cell. Offsets are from the sprite position, where a 16x16 body sits at 0,0, so the
// effect origin lands on the body's center.
function drawTables(fx, cells, result, { slot, oamPalette, priority }) {
  const sx = n => (n < 0 ? `-${-n}` : `${n}`);
  const props = `$${hex2((priority << 4) | (oamPalette << 1) | 1)}`;
  const frames = fx.holds.map((_, k) => cells.filter(c => c.frame === k && !c.empty));
  const empty = frames.flatMap((f, k) => (f.length ? [] : [k]));
  if (empty.length) fail(`--slot: frame(s) ${empty.join(', ')} have no tiles; %DrawSprite needs at least one object per frame`);
  const all = frames.flat(), starts = frames.map((_, k) => frames.slice(0, k).reduce((n, f) => n + f.length, 0));
  const first = cells[0].block, last = cells[cells.length - 1].block;
  const row = (dir, values) => `  ${dir} ${values.join(', ')}`;
  return [
    `; Generated by yaze scripts/graphics/snes_fx.js from ${fx.name}; do not edit by hand.`,
    `; Tiles: sheet blocks ${first}..${last} in spriteset slot ${slot} (chr $${hex2(slot * SLOT_TILES)}+).`,
    `; OAM palette ${oamPalette}, priority ${priority}${result.romPalette ? `, colors from ROM ${result.romPalette.group}:${result.romPalette.index}` : ''}.`,
    `; incsrc right after %DrawSprite(). Frame k = SprGfx+SprFrame k; hold frame k for .holds[k] game frames.`,
    row('.start_index  db', starts.map(n => `$${hex2(n)}`)),
    row('.nbr_of_tiles db', frames.map(f => f.length - 1)),
    row('.x_offsets    dw', all.map(c => sx(c.cx - fx.origin[0] + 8))),
    row('.y_offsets    dw', all.map(c => sx(c.cy - fx.origin[1] + 8))),
    row('.chr          db', all.map(c => `$${hex2(slot * SLOT_TILES + c.tiles[0])}`)),
    row('.properties   db', all.map(() => props)),
    row('.sizes        db', all.map(() => '$02')),
    row('.holds        db', fx.holds),
    '',
  ].join('\n');
}

function usage() {
  return `usage: snes_fx.js --fx scripts/graphics/fx/poof.js --out DIR [--block 0] [--scale 8]
                  [--rom game.sfc --palette GROUP:INDEX[:FIRST] [--z3ed z3ed]]
                  [--slot 0..3 --oam-palette 0..7 [--priority 3]]

Renders an effect module to indexed frames, checks them against SNES OBJ limits, and writes into a new DIR:
  <name>.png      one row of 16x16 blocks for: z3ed gfx-import --sheet <id> --png <name>.png --block <n> --dry-run
  contact.png     frames side by side at --scale, for review
  <name>.json     checks, palette as BGR555, and the sheet block and 8x8 tiles of every cell
  <name>_oam.asm  with --slot: %DrawSprite() tables (Oracle Core/sprite_macros.asm) for the frames
--palette reads a ROM palette with z3ed palette-get-colors (e.g. sprites_aux1:0) and moves every effect
color to the nearest ROM color, so the frames use colors the game already loads. Warns on far colors
and on effect colors that merge into one ROM color.
Exit 1 when a check fails: index outside 1..${MAX_INDEX}, pixels outside the OBJ, unseeded randomness,
more cells than the sheet has blocks after --block, or (with --slot) a frame with no tiles.\n`;
}

function main(argv) {
  const options = { scale: '8', block: '0', priority: '3', z3ed: process.env.Z3ED || 'z3ed' };
  const FLAGS = ['--fx', '--out', '--block', '--scale', '--rom', '--palette', '--z3ed', '--slot', '--oam-palette', '--priority'];
  for (let i = 0; i < argv.length; i++) {
    const flag = argv[i];
    if (flag === '--help' || flag === '-h') { process.stdout.write(usage()); return; }
    if (!FLAGS.includes(flag)) fail(`Unknown option: ${flag}`);
    if (!argv[i + 1] || argv[i + 1].startsWith('--')) fail(`${flag}: missing value`);
    options[flag.slice(2)] = argv[++i];
  }
  if (!options.fx || !options.out) fail('--fx and --out are required; use --help');
  const scale = Number(options.scale), firstBlock = Number(options.block);
  if (!integer(scale) || scale < 1 || scale > 32) fail('--scale: expected integer 1..32');
  if (!/^\d+$/.test(options.block) || firstBlock >= SHEET_BLOCKS) fail(`--block: expected 0..${SHEET_BLOCKS - 1}`);
  if (!!options.rom !== !!options.palette) fail('--rom and --palette go together');
  const oam = options.slot === undefined ? null : { slot: Number(options.slot), oamPalette: Number(options['oam-palette']), priority: Number(options.priority) };
  if (oam) {
    if (!/^[0-3]$/.test(options.slot)) fail('--slot: expected 0..3');
    if (!/^[0-7]$/.test(options['oam-palette'] ?? '')) fail('--oam-palette: expected 0..7 (required with --slot)');
    if (!/^[0-3]$/.test(options.priority)) fail('--priority: expected 0..3');
  } else if (options['oam-palette'] !== undefined) fail('--oam-palette needs --slot');
  const outputPath = path.resolve(options.out);
  if (fs.existsSync(outputPath)) fail('--out: path already exists; choose a new directory');
  const fx = require(fs.realpathSync(options.fx));
  const romPalette = options.rom ? readRomPalette(options.rom, options.palette, options.z3ed) : null;
  const { frames, cells, result } = evaluateEffect(fx, { firstBlock, romPalette });
  const asm = oam && drawTables(fx, cells, result, oam);
  const paletteRgb = (romPalette ? romPalette.colors : result.palette.map(c => toBgr555(c.source))).map(c => c.rgb);
  fs.mkdirSync(outputPath); // No recursive or replace mode.
  fs.writeFileSync(path.join(outputPath, `${fx.name}.png`), blockStrip(fx, frames, cells, paletteRgb), { flag: 'wx' });
  fs.writeFileSync(path.join(outputPath, 'contact.png'), contactSheet(fx, frames, paletteRgb, scale), { flag: 'wx' });
  fs.writeFileSync(path.join(outputPath, `${fx.name}.json`), JSON.stringify(result, null, 2) + '\n', { flag: 'wx' });
  if (asm) fs.writeFileSync(path.join(outputPath, `${fx.name}_oam.asm`), asm, { flag: 'wx' });
  process.stdout.write(JSON.stringify({ output: outputPath, gameFrames: result.gameFrames, blocks: cells.length, tiles: result.tiles, errors: result.errors, warnings: result.warnings }, null, 2) + '\n');
  if (result.errors.length) process.exitCode = 1;
}

// Exports come before the CLI runs: effect modules require this file for TAU while main() loads them.
module.exports = { Painter, rng, toBgr555, fromBgr555, readRomPalette, remapToRom, evaluateEffect, layoutBlocks, drawTables, encodeIndexedPng, TAU, BAYER, main };
if (require.main === module) {
  try { main(process.argv.slice(2)); } catch (error) { process.stderr.write(`snes_fx: ${error.message}\n`); process.exitCode = 1; }
}
