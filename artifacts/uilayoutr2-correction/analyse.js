// Derives the correction verdicts from the measured rows and writes the owner
// index. Expected values come from the immutable OWNER AUTHORITY BLOCK only.
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
P('== A. RIGHT-HOST FRAME vs OWNER AUTHORITY BLOCK (tolerance 4 dp) ==');
P('rows: ' + d.frames.length + '  anchor PASS: ' + d.frames.filter((r) => r.pass === 'PASS').length +
  '  anchor FAIL: ' + d.frames.filter((r) => r.pass === 'FAIL').length);
let driftFail = 0;
CELLS.forEach((c) => {
  const rows = d.frames.filter((r) => r.cell === c);
  if (!rows.length) return;
  const u = (k) => Array.from(new Set(rows.map((r) => Math.round(r[k] * 100) / 100)));
  const dr = { top: u('top'), right: u('right'), width: u('width') };
  const ok = dr.top.length === 1 && dr.right.length === 1 && dr.width.length === 1;
  if (!ok) driftFail++;
  P(c + ' drift top=' + dr.top.join('|') + ' right=' + dr.right.join('|') +
    ' width=' + dr.width.join('|') + '  -> ' + (ok ? 'DRIFT 0 dp PASS' : 'DRIFT FAIL'));
  P('   deltas vs authority: dTop=' + rows[0].dTop + ' dRight=' + rows[0].dRight +
    ' dWidth=' + rows[0].dWidth + '  anchor=' + rows[0].pass);
  P('   host height across states: ' + u('height').join(' | ') + ' dp (content-driven)');
});
P('intra-cell drift failures: ' + (driftFail || 'NONE'));

// A2. the corrected defect, stated directly
P('');
P('== A2. EXACT / DETAILS NO LONGER TRANSLATE THE HOST ==');
const OPEN = ['EXACT_OPEN', 'EXACT_IME', 'SCULPT_DETAILS'];
CELLS.forEach((c) => {
  const rest = d.frames.filter((r) => r.cell === c && r.state === 'CONSTRUCTION_SHAPE_REST')[0];
  if (!rest) return;
  const moved = OPEN.map((s) => d.frames.filter((r) => r.cell === c && r.state === s)[0])
    .filter((r) => r && Math.abs(r.right - rest.right) > 0.01);
  P(c + ' resting right=' + rest.right + ' dp; with Exact/Details open: ' +
    OPEN.map((s) => {
      const r = d.frames.filter((x) => x.cell === c && x.state === s)[0];
      return s + '=' + (r ? r.right : '?');
    }).join(' ') + '  -> ' + (moved.length ? 'FAIL' : 'PASS'));
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
  ['global_toolbar', 'objects_capsule', 'history_group', 'objects_section',
    'brush_edge_controls', 'viewport_surface'].forEach((el) => {
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
    r.present === 'PRESENT').length === 0 ? 'ABSENT IN EVERY SCULPT STATE/CELL - PASS' : 'PRESENT - FAIL'));

// C. hit areas -- intrinsic, with clipped intersections reported separately.
// Re-derived here from the raw visible intersections in frames.json through the
// one shared rule in hits.js, and hit-areas.csv is rewritten from it, so the CSV
// and this verdict can never disagree.
P('');
P('== C. HIT AREA (48 dp floor, judged on INTRINSIC geometry) ==');
require('./hits').resolve(d.hits);
csv('hit-areas.csv', ['cell', 'state', 'id', 'intrinsicW', 'intrinsicH', 'intrinsicFrom',
  'visibleW', 'visibleH', 'clipped', 'pass'], d.hits);
const intrinsicFail = d.hits.filter((h) => h.pass === 'FAIL');
const clipped = d.hits.filter((h) => h.clipped === 'CLIPPED');
P('control rows measured: ' + d.hits.length);
P('intrinsic violations (< 48x48 dp at their full box): ' +
  (intrinsicFail.length
    ? Array.from(new Set(intrinsicFail.map((h) => h.id + ' ' + h.intrinsicW + 'x' + h.intrinsicH))).join(', ')
    : 'NONE'));
P('rows whose VISIBLE intersection was short because a ScrollView clipped them: ' + clipped.length);
const clipById = {};
clipped.forEach((h) => {
  const k = h.id + ' intrinsic ' + h.intrinsicW + 'x' + h.intrinsicH + ' (from ' + h.intrinsicFrom + ')';
  (clipById[k] = clipById[k] || []).push(h.cell + '/' + h.state);
});
Object.keys(clipById).sort().forEach((k) => P('  CLIPPED ' + k + ' dp in ' +
  clipById[k].length + ' row(s) (' +
  Array.from(new Set(clipById[k].map((s) => s.split('/')[0]))).join(',') + ')'));
const runResolved = Array.from(new Set(d.hits.filter((h) => h.intrinsicFrom === 'RUN')
  .map((h) => h.cell + '/' + h.id + ' ' + h.intrinsicW + 'x' + h.intrinsicH)));
P('controls the harness never caught unclipped in their own cell (floor argued from');
P('the same control elsewhere in the run; UILR2C-11 is the unclipped in-process authority): ' +
  (runResolved.length ? runResolved.join(', ') : 'NONE'));

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
      ? 'SAME SINGLE EXTERNAL HOST - PASS' : 'FAIL'));
});

function csv(file, cols, rows) {
  const lines = [cols.join(',')];
  rows.forEach((r) => lines.push(cols.map((c) => {
    const v = r[c]; return typeof v === 'number' ? (Math.round(v * 100) / 100) : v;
  }).join(',')));
  fs.writeFileSync(path.join(DIR, file), lines.join('\n') + '\n');
}

fs.writeFileSync(path.join(DIR, 'analysis.txt'), out.join('\n') + '\n');
fs.writeFileSync(path.join(DIR, 'frames.json'), JSON.stringify(d, null, 1));

// owner index
const md = ['# UI-LAYOUT-R2 correction round 1 - owner review index', '',
  'Green is the geometry of the immutable OWNER AUTHORITY BLOCK (Revision 1).',
  'Magenta is the measured runtime frame. Every number in an overlay comes from',
  '`frames.json` / `frame-measurements.csv`; nothing is drawn by eye. Height is',
  'content-driven under Revision 1, so only top, right and width are anchors.', '',
  '| Cell | State | Raw | Overlay | dTop | dRight | dWidth | Anchor |',
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
