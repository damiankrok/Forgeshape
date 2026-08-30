// Mechanically ties every drawn rectangle to the immutable OWNER AUTHORITY BLOCK
// and to the measured rows in frames.json. Nothing here is eyeballed, and no
// rectangle comes from a product resource.
//
// The superseded 132-152 dp R0 host geometry is deliberately absent: it is no
// longer correction authority and must not be drawn as if it were.
const fs = require('fs');
const path = require('path');
const png = require('./pnglib');

const DIR = __dirname;
const data = JSON.parse(fs.readFileSync(path.join(DIR, 'frames.json'), 'utf8'));

// OWNER AUTHORITY BLOCK, Revision 1.
const SPEC = {
  C10: { wdp: 411, hdp: 914, density: 320, top: 112, right: 8, width: 72 },
  C13: { wdp: 411, hdp: 914, density: 320, top: 112, right: 8, width: 72 },
  L10: { wdp: 914, hdp: 411, density: 320, top: 88, right: 8, width: 72 },
  L13: { wdp: 914, hdp: 411, density: 320, top: 88, right: 8, width: 72 },
  E10: { wdp: 1280, hdp: 800, density: 160, top: 108, right: 8, width: 72 },
  E13: { wdp: 1280, hdp: 800, density: 160, top: 108, right: 8, width: 72 },
};
const SHOT_STATES = ['CONSTRUCTION_SHAPE_REST', 'TRANSFORM_MOVE_WORLD', 'TRANSFORM_SCALE',
  'EXACT_IME', 'SCULPT_REST', 'SCULPT_DETAILS'];
const GREEN = [60, 235, 130], MAG = [255, 60, 160], YELL = [255, 190, 70],
  WHITE = [245, 248, 252], DARK = [12, 14, 17], RED = [255, 90, 90];

function overlay(row) {
  const cell = row.cell, state = row.state;
  const s = SPEC[cell], sc = s.density / 160;
  const file = cell + '-' + state + '.png';
  const src = path.join(DIR, 'raw', file);
  if (!fs.existsSync(src)) { console.log('missing raw ' + file); return false; }
  const img = png.decode(fs.readFileSync(src));
  const D = (v) => Math.round(v * sc);           // dp -> device px
  const t = Math.max(2, Math.round(2 * sc));

  // Accepted trailing inset, drawn as the band the host's right edge must land
  // in. Solid green is the accepted anchor; the dashed pair is the +/-4 dp
  // tolerance the block allows.
  const exRight = s.wdp - s.right;
  [-4, 4].forEach((d) => png.strokeRect(img, D(exRight + d), 0, 1, img.h,
    GREEN, Math.max(1, Math.round(sc)), Math.round(7 * sc)));

  // Accepted host frame (solid green) vs measured host frame (solid magenta).
  // Height is content-driven under Revision 1, so the accepted box is drawn at
  // the measured height: only top, right and width are anchors.
  const ex = s.wdp - s.right - s.width;
  png.strokeRect(img, D(ex), D(s.top), D(s.width), D(row.height), GREEN, t + 1);
  png.drawText(img, 'EXPECTED HOST', D(ex) + 3 * sc, Math.max(0, D(s.top) - 10 * sc), GREEN,
    Math.max(1, Math.round(sc)));
  const mx = s.wdp - row.right - row.width;
  png.strokeRect(img, D(mx), D(row.top), D(row.width), D(row.height), MAG, t + 1);
  png.drawText(img, 'MEASURED HOST', D(mx) - 84 * sc, D(row.top) + D(row.height) + 4 * sc, MAG,
    Math.max(1, Math.round(sc)));

  // Anchor delta callouts, drawn from the measured row itself.
  const fs2 = Math.max(1, Math.round(sc));
  const lines = [
    cell + ' / ' + state.replace(/_/g, ' '),
    'WINDOW ' + s.wdp + 'X' + s.hdp + ' DP  DENSITY ' + s.density,
    'TOP      EXP ' + s.top + '  MEAS ' + round(row.top) + '  D ' + fmt(row.dTop),
    'RIGHT    EXP ' + s.right + '  MEAS ' + round(row.right) + '  D ' + fmt(row.dRight),
    'WIDTH    EXP ' + s.width + '  MEAS ' + round(row.width) + '  D ' + fmt(row.dWidth),
    'HEIGHT   CONTENT-DRIVEN  MEAS ' + round(row.height),
    'ANCHOR VERDICT: ' + row.pass + ' (TOLERANCE 4 DP)',
  ];
  const lh = 10 * fs2, pad = 6 * fs2;
  const bw = Math.max.apply(null, lines.map((l) => png.textWidth(l, fs2))) + pad * 2;
  const bh = lines.length * lh + pad * 2;
  // Low on the leading edge, clear of the right host the reviewer is checking.
  const bx = D(12), by = Math.max(D(24), img.h - bh - D(24));
  png.fillRect(img, bx, by, bw, bh, DARK, 0.82);
  png.strokeRect(img, bx, by, bw, bh, row.pass === 'PASS' ? GREEN : RED, fs2 * 2);
  lines.forEach((l, i) => png.drawText(img, l, bx + pad, by + pad + i * lh,
    i === lines.length - 1 ? (row.pass === 'PASS' ? GREEN : RED) : (i === 0 ? YELL : WHITE), fs2));
  png.drawText(img, 'GREEN = OWNER AUTHORITY BLOCK   MAGENTA = MEASURED RUNTIME',
    bx + pad, by - lh - 2 * fs2, WHITE, fs2);

  fs.writeFileSync(path.join(DIR, 'overlays', file), png.encode(img));
  return true;
}
function fmt(v) { return (v > 0 ? '+' : '') + round(v); }
function round(v) { return Math.round(v * 100) / 100; }

let n = 0;
data.frames.filter((r) => SHOT_STATES.indexOf(r.state) >= 0).forEach((r) => { if (overlay(r)) n++; });
console.log('overlays written: ' + n);
