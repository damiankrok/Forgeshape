// Mechanically ties every drawn rectangle to the accepted UI-SPEC-R0 tables and
// to the measured rows in frames.json. Nothing here is eyeballed.
const fs = require('fs');
const path = require('path');
const png = require('./pnglib');

const DIR = __dirname;
const data = JSON.parse(fs.readFileSync(path.join(DIR, 'frames.json'), 'utf8'));

// Accepted UI-SPEC-R0 section 1 (cells), 4 (persistent rects) and 6 (Exact/Details).
const SPEC = {
  C10: { wdp: 411, hdp: 914, density: 320, top: 88, right: 12, width: 132,
    toolbar: [12, 12, 387, 56], objC: [12, 850, 152, 52], hist: [247, 850, 152, 52],
    objS: [12, 850, 152, 52], brush: null, dock: null, exact: [16, 624, 379, 274] },
  C13: { wdp: 411, hdp: 914, density: 320, top: 88, right: 12, width: 140,
    toolbar: [12, 12, 387, 56], objC: [12, 850, 152, 52], hist: [247, 850, 152, 52],
    objS: [12, 850, 152, 52], brush: null, dock: null, exact: [16, 594, 379, 304] },
  L10: { wdp: 914, hdp: 411, density: 320, top: 72, right: 12, width: 132,
    toolbar: [12, 12, 890, 48], objC: [12, 347, 164, 52], hist: [738, 347, 164, 52],
    objS: [12, 347, 164, 52], brush: null, dock: null, exact: [180, 143, 554, 256] },
  L13: { wdp: 914, hdp: 411, density: 320, top: 72, right: 12, width: 140,
    toolbar: [12, 12, 890, 48], objC: [12, 347, 164, 52], hist: [738, 347, 164, 52],
    objS: [12, 347, 164, 52], brush: null, dock: null, exact: [172, 123, 570, 276] },
  E10: { wdp: 1280, hdp: 800, density: 160, top: 96, right: 16, width: 144,
    toolbar: [16, 16, 1248, 56], objC: null, hist: [1096, 732, 168, 52],
    objS: [16, 732, 168, 52], brush: [16, 120, 56, 248], dock: [16, 96, 184, 560],
    exact: [280, 512, 720, 272] },
  E13: { wdp: 1280, hdp: 800, density: 160, top: 96, right: 16, width: 152,
    toolbar: [16, 16, 1248, 56], objC: null, hist: [1096, 732, 168, 52],
    objS: [16, 732, 168, 52], brush: [16, 120, 56, 248], dock: [16, 96, 184, 560],
    exact: [260, 480, 760, 304] },
};
const EXP_H = {
  CONSTRUCTION_SHAPE_REST: 160, TRANSFORM_MOVE_WORLD: 328, TRANSFORM_SCALE: 272,
  EXACT_IME: 328, SCULPT_REST: 272, SCULPT_DETAILS: 272,
};
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

  // Accepted persistent rectangles for this state (dashed green).
  const sculpt = state.indexOf('SCULPT') === 0;
  const rects = [['TOOLBAR', s.toolbar]];
  if (sculpt) { rects.push(['OBJECTS S', s.objS]); if (s.brush) rects.push(['BRUSH', s.brush]); }
  else {
    if (s.objC) rects.push(['OBJECTS C', s.objC]);
    rects.push(['HISTORY', s.hist]);
    if (s.dock) rects.push(['OBJ DOCK', s.dock]);
  }
  if (state === 'EXACT_IME') rects.push(['EXACT', s.exact]);
  if (state === 'SCULPT_DETAILS') rects.push(['DETAILS', s.exact]);
  rects.forEach(([label, r]) => {
    if (!r) return;
    png.strokeRect(img, D(r[0]), D(r[1]), D(r[2]), D(r[3]), GREEN, t, Math.round(7 * sc));
    png.drawText(img, label, D(r[0]) + 3 * sc, D(r[1]) + 3 * sc, GREEN, Math.max(1, Math.round(sc)));
  });

  // Accepted right-host frame (solid green) vs measured right-host frame (solid magenta).
  const ex = s.wdp - s.right - s.width, ey = s.top, ew = s.width, eh = EXP_H[state];
  png.strokeRect(img, D(ex), D(ey), D(ew), D(eh), GREEN, t + 1);
  png.drawText(img, 'EXPECTED HOST', D(ex) + 3 * sc, D(ey) - 10 * sc, GREEN, Math.max(1, Math.round(sc)));
  const mx = s.wdp - row.right - row.width;
  png.strokeRect(img, D(mx), D(row.top), D(row.width), D(row.height), MAG, t + 1);
  png.drawText(img, 'MEASURED HOST', D(mx) - 84 * sc, D(row.top) + D(row.height) + 4 * sc, MAG,
    Math.max(1, Math.round(sc)));

  // Anchor delta callouts, drawn from the measured row itself.
  const fs2 = Math.max(1, Math.round(sc));
  const lines = [
    cell + ' / ' + state.replace(/_/g, ' '),
    'WINDOW ' + s.wdp + 'X' + s.hdp + ' DP  DENSITY ' + s.density,
    'TOP      EXP ' + s.top + '  MEAS ' + row.top + '  D ' + fmt(row.dTop),
    'RIGHT    EXP ' + s.right + '  MEAS ' + row.right + '  D ' + fmt(row.dRight),
    'WIDTH    EXP ' + s.width + '  MEAS ' + row.width + '  D ' + fmt(row.dWidth),
    'HEIGHT   EXP ' + eh + '  MEAS ' + row.height,
    'ANCHOR VERDICT: ' + row.pass + ' (TOLERANCE 4 DP)',
  ];
  const lh = 10 * fs2, pad = 6 * fs2;
  const bw = Math.max.apply(null, lines.map((l) => png.textWidth(l, fs2))) + pad * 2;
  const bh = lines.length * lh + pad * 2;
  // Sit the readout above the accepted Objects rectangle so it never hides a
  // persistent element the reviewer is meant to check.
  const anchor = (s.objC || s.objS)[1];
  const bx = D(12), by = Math.max(D(12), D(anchor) - bh - D(12));
  png.fillRect(img, bx, by, bw, bh, DARK, 0.82);
  png.strokeRect(img, bx, by, bw, bh, row.pass === 'PASS' ? GREEN : RED, fs2 * 2);
  lines.forEach((l, i) => png.drawText(img, l, bx + pad, by + pad + i * lh,
    i === lines.length - 1 ? (row.pass === 'PASS' ? GREEN : RED) : (i === 0 ? YELL : WHITE), fs2));
  // Legend
  png.drawText(img, 'GREEN = ACCEPTED UI-SPEC-R0   MAGENTA = MEASURED RUNTIME',
    bx + pad, by - lh - 2 * fs2, WHITE, fs2);

  fs.writeFileSync(path.join(DIR, 'overlays', file), png.encode(img));
  return true;
}
function fmt(v) { return (v > 0 ? '+' : '') + (Math.round(v * 100) / 100); }

let n = 0;
data.frames.filter((r) => EXP_H[r.state] !== undefined).forEach((r) => { if (overlay(r)) n++; });
console.log('overlays written: ' + n);
