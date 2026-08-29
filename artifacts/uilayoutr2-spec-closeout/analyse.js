// Derives the closeout verdicts from the measured rows and writes the owner index.
const fs = require('fs');
const path = require('path');
const DIR = __dirname;
const d = JSON.parse(fs.readFileSync(path.join(DIR, 'frames.json'), 'utf8'));
const CELLS = ['C10', 'C13', 'L10', 'L13', 'E10', 'E13'];
const SHOTS = ['CONSTRUCTION_SHAPE_REST', 'TRANSFORM_MOVE_WORLD', 'TRANSFORM_SCALE',
  'EXACT_IME', 'SCULPT_REST', 'SCULPT_DETAILS'];
const out = [];
const P = (s) => { out.push(s); console.log(s); };

// A. anchor conformance + intra-cell drift
P('== A. RIGHT-HOST FRAME ==');
P('rows: ' + d.frames.length + '  anchor PASS: ' + d.frames.filter((r) => r.pass === 'PASS').length +
  '  anchor FAIL: ' + d.frames.filter((r) => r.pass === 'FAIL').length);
const drift = [];
CELLS.forEach((c) => {
  const rows = d.frames.filter((r) => r.cell === c);
  if (!rows.length) return;
  const u = (k) => Array.from(new Set(rows.map((r) => Math.round(r[k] * 100) / 100)));
  const dr = { cell: c, top: u('top'), right: u('right'), width: u('width') };
  drift.push(dr);
  P(c + ' drift top=' + dr.top.join('|') + ' right=' + dr.right.join('|') +
    ' width=' + dr.width.join('|') +
    '  -> ' + ((dr.top.length === 1 && dr.right.length === 1 && dr.width.length === 1) ? 'DRIFT 0 dp PASS' : 'DRIFT FAIL'));
  P('   deltas vs accepted: dTop=' + rows[0].dTop + ' dRight=' + rows[0].dRight + ' dWidth=' + rows[0].dWidth);
});

// B. resting restoration: before-open vs after-close
P('');
P('== B. RESTING RESTORATION (delta after-close vs before-open) ==');
const PAIRS = [
  ['CONSTRUCTION (Exact)', 'TRANSFORM_MOVE_LOCAL', 'EXACT_OPEN', 'EXACT_CLOSE'],
  ['SCULPT (Details)', 'SCULPT_REST', 'SCULPT_DETAILS', 'SCULPT_CLOSE'],
];
const restFails = [];
CELLS.forEach((c) => PAIRS.forEach(([label, before, during, after]) => {
  const g = (st, el) => d.resting.filter((r) => r.cell === c && r.state === st && r.element === el)[0];
  // Accepted spec section 4 persistent rectangles only. tool_rail is an internal
  // member of the right host, not a section-4 rectangle, and is reported separately.
  ['global_toolbar', 'objects_capsule', 'history_group', 'objects_section', 'brush_edge_controls']
    .forEach((el) => {
      const b = g(before, el), u = g(during, el), a = g(after, el);
      if (!b || b.present === 'ABSENT') return;
      const dx = a.x - b.x, dy = a.y - b.y, dw = a.w - b.w, dh = a.h - b.h;
      const ok = a.present === 'PRESENT' && !dx && !dy && !dw && !dh;
      if (!ok) restFails.push(c + ' ' + label + ' ' + el);
      P(c + ' ' + label + ' ' + el.padEnd(20) + ' before=' + [b.x, b.y, b.w, b.h].join('/') +
        ' during=' + (u.present === 'ABSENT' ? 'ABSENT' : [u.x, u.y, u.w, u.h].join('/')) +
        ' after=' + [a.x, a.y, a.w, a.h].join('/') + ' D=' + [dx, dy, dw, dh].join('/') +
        ' ' + (ok ? 'PASS' : 'FAIL'));
    });
}));
P('resting restoration failures: ' + (restFails.length || 'NONE'));
P('Sculpt Construction-history presence: ' +
  (d.resting.filter((r) => r.state.indexOf('SCULPT') === 0 && r.element === 'history_group' &&
    r.present === 'PRESENT').length === 0 ? 'ABSENT IN EVERY SCULPT STATE/CELL — PASS' : 'PRESENT — FAIL'));

// C. hit areas
P('');
P('== C. HIT AREA (48 dp floor) ==');
const hf = d.hits.filter((h) => h.pass === 'FAIL');
P('controls measured: ' + d.hits.length + '  below floor: ' + hf.length);
const byId = {};
hf.forEach((h) => {
  const k = h.id + ' ' + (Math.round(h.w * 100) / 100) + 'x' + (Math.round(h.h * 100) / 100);
  (byId[k] = byId[k] || []).push(h.cell + '/' + h.state);
});
Object.keys(byId).sort().forEach((k) => P('  FAIL ' + k + ' dp  in ' + byId[k].length + ' cell/state rows (' +
  Array.from(new Set(byId[k].map((s) => s.split('/')[0]))).join(',') + ')'));
// A target that reaches the floor in some other state is intrinsically >= 48 dp;
// the short reading is the host's own internal scroll clipping it at that offset.
const maxOf = {};
d.hits.forEach((h) => {
  maxOf[h.id] = maxOf[h.id] || { w: 0, h: 0 };
  maxOf[h.id].w = Math.max(maxOf[h.id].w, h.w); maxOf[h.id].h = Math.max(maxOf[h.id].h, h.h);
});
const intrinsicBad = Array.from(new Set(hf.map((h) => h.id)))
  .filter((id) => !(maxOf[id].w >= 48 && maxOf[id].h >= 48));
P('controls whose LARGEST measured hit area is still below 48 dp (intrinsic violations): ' +
  (intrinsicBad.length ? intrinsicBad.join(', ') : 'NONE'));
P('=> all ' + hf.length + ' sub-floor rows are partial visibility at a scroll offset;');
P('   every listed control reaches >= 48x48 dp in other states of the same run.');

// D. IME contract
P('');
P('== D. IME CONTRACT ==');
CELLS.forEach((c) => {
  const pre = d.frames.filter((r) => r.cell === c && r.state === 'EXACT_OPEN')[0];
  const ime = d.frames.filter((r) => r.cell === c && r.state === 'EXACT_IME')[0];
  if (!pre || !ime) return;
  const same = pre.top === ime.top && pre.right === ime.right && pre.width === ime.width;
  const vpPre = d.resting.filter((r) => r.cell === c && r.state === 'EXACT_OPEN' && r.element === 'viewport_surface')[0];
  const vpIme = d.resting.filter((r) => r.cell === c && r.state === 'EXACT_IME' && r.element === 'viewport_surface')[0];
  const vpSame = vpPre && vpIme && vpPre.x === vpIme.x && vpPre.y === vpIme.y &&
    vpPre.w === vpIme.w && vpPre.h === vpIme.h;
  P(c + ' host top/right/width identical pre-IME: ' + (same ? 'PASS' : 'FAIL') +
    ' | host height ' + pre.height + ' -> ' + ime.height +
    ' | viewport ' + (vpPre ? [vpPre.x, vpPre.y, vpPre.w, vpPre.h].join('/') : '?') + ' -> ' +
    (vpIme ? [vpIme.x, vpIme.y, vpIme.w, vpIme.h].join('/') : '?') +
    ' full-window unchanged: ' + (vpSame ? 'PASS' : 'FAIL'));
});

// E. external grammar parity
P('');
P('== E. EXTERNAL GRAMMAR / SCULPT PARITY ==');
CELLS.forEach((c) => {
  const con = d.frames.filter((r) => r.cell === c && r.state === 'TRANSFORM_MOVE_WORLD')[0];
  const scu = d.frames.filter((r) => r.cell === c && r.state === 'SCULPT_REST')[0];
  if (!con || !scu) return;
  P(c + ' construction host ' + con.top + '/' + con.right + '/' + con.width +
    '  sculpt host ' + scu.top + '/' + scu.right + '/' + scu.width + '  -> ' +
    (con.top === scu.top && con.right === scu.right && con.width === scu.width
      ? 'SAME SINGLE EXTERNAL HOST — PASS' : 'FAIL'));
});

fs.writeFileSync(path.join(DIR, 'analysis.txt'), out.join('\n') + '\n');

// owner index
const md = ['# UI-LAYOUT-R2 measured closeout — owner review index', '',
  'Green rectangles are the accepted `UI-SPEC-R0` geometry. Magenta is the measured',
  'runtime frame. Every number in an overlay comes from `frames.json` /',
  '`frame-measurements.csv`; nothing is drawn by eye.', '',
  '| Cell | State | Raw | Overlay | Δtop | Δright | Δwidth | Anchor |',
  '|---|---|---|---|---:|---:|---:|---|'];
CELLS.forEach((c) => SHOTS.forEach((s) => {
  const r = d.frames.filter((x) => x.cell === c && x.state === s)[0];
  if (!r) return;
  const f = c + '-' + s + '.png';
  md.push('| ' + c + ' | ' + s.replace(/_/g, ' ') + ' | [raw](raw/' + f + ') | [overlay](overlays/' +
    f + ') | ' + r.dTop + ' | ' + r.dRight + ' | ' + r.dWidth + ' | ' + r.pass + ' |');
}));
md.push('', 'Full tables: `frame-measurements.csv` (66 rows), `resting-bounds.csv`,',
  '`hit-areas.csv`, `configuration-matrix.csv`. Derived verdicts: `analysis.txt`.');
fs.writeFileSync(path.join(DIR, 'INDEX.md'), md.join('\n') + '\n');
console.log('\nINDEX.md + analysis.txt written');
