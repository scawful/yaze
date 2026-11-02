'use strict';

// A cartoon poof in one 16x16 OBJ: a white pop, a lumpy top-lit cloud, then the balls shrink away.
// Ball layering (shade, body, cap) follows pixelfx spawn/poof (MIT), redrawn for 7 colors and 5 frames.
const { TAU } = require('../snes_fx');

// OBJ colors 1..7: outline, shade, body dark, body, cap, white, star. Each is exact in BGR555.
const OUTLINE = 1, SHADE = 2, DARK = 3, BODY = 4, CAP = 5, WHITE = 6, STAR = 7;

// Per frame: ring distance, ball radius, core radius, lift (pixels up).
const DIST = [0, 2.4, 3.3, 4.2, 4.6];
const RADIUS = [0, 2.4, 2.9, 2.2, 1.1];
const CORE = [0, 3.0, 2.8, 1.6, 0];
const LIFT = [0, 0, 0, 1, 2];

function ball(p, x, y, r, layer, low) {
  if (r < 0.6) return;
  if (layer === 'shade') p.disc(x + r * 0.2, y + r * 0.25, r, SHADE);
  else if (layer === 'body') p.disc(x - r * 0.06, y - r * 0.1, r * 0.85, low ? DARK : BODY);
  else if (!low) p.disc(x - r * 0.3, y - r * 0.35, r * 0.4, r > 2 ? CAP : WHITE);
}

module.exports = {
  name: 'poof',
  size: [16, 16],
  origin: [8, 8],
  holds: [2, 3, 4, 4, 3],
  palette: ['#211831', '#524a6b', '#8c84ad', '#bdbdde', '#e7e7f7', '#ffffff', '#f7d652'],
  seed: 21,
  draw(p, k, t, R) {
    // draw every random number in the same order each frame, so ball i keeps its angle and size
    const balls = Array.from({ length: 6 }, (_, i) => ({ a: (i / 6) * TAU + R.range(-0.25, 0.25), sp: R.range(0.9, 1.05), rf: R.range(0.85, 1.1) }));
    const stars = Array.from({ length: 3 }, () => ({ a: R.range(-Math.PI * 0.9, -Math.PI * 0.1), d: R.range(4.8, 5.6) }));
    if (k === 0) {
      p.disc(0, 0, 3.6, WHITE);
      // a solid ring: dithering a 1px ring at this size reads as noise
      p.ring(0, 0, 5.5, 1, CAP);
      return;
    }
    const lift = LIFT[k];
    const live = balls.map(b => ({ x: Math.cos(b.a) * DIST[k] * b.sp, y: Math.sin(b.a) * DIST[k] * b.sp * 0.8 - lift, r: RADIUS[k] * b.rf }));
    if (CORE[k]) live.push({ x: 0, y: -lift, r: CORE[k] });
    // shade, body, then caps, so neighbouring balls merge into one lumpy cloud lit from above
    for (const layer of ['shade', 'body', 'cap']) for (const b of live) ball(p, b.x, b.y, b.r, layer, b.y > 1.5);
    if (k < 4) p.outline(OUTLINE);
    if (k === 1 || k === 2) {
      for (const s of stars) {
        const x = Math.cos(s.a) * s.d * (k === 1 ? 0.85 : 1), y = Math.sin(s.a) * s.d * (k === 1 ? 0.85 : 1);
        p.put(x, y, k === 1 ? WHITE : STAR);
      }
    }
  },
};
