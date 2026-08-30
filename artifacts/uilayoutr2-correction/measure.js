// UI-LAYOUT-R2 correction round 1 measurement harness.
//
// Expected geometry is transcribed from the coordinator's immutable OWNER
// AUTHORITY BLOCK (the execution transmission of the OWNER-accepted UI-SPEC-R0
// Revision 1). No expected value is derived from dimens.xml, EditorControlStyles,
// runtime bounds or a screenshot, and no external spec file is read.
//
// The harness reads runtime bounds through existing semantic ids. Nothing is
// located by remembered screen coordinate: every tap resolves its target by id
// in the hierarchy captured immediately beforehand.
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ADB = 'C:/Users/damia/AppData/Local/Android/Sdk/platform-tools/adb.exe';
const SERIAL = process.env.FS_SERIAL || 'emulator-5580';
const PKG = 'com.forgeshape.app';
const ACT = PKG + '/.ForgeShapeActivity';
const DIR = __dirname;
const CMDLOG = [];

function adb(args, opts) {
  CMDLOG.push('adb -s ' + SERIAL + ' ' + args.join(' '));
  return execFileSync(ADB, ['-s', SERIAL].concat(args),
    Object.assign({ encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 }, opts || {}));
}
function sh(cmd) { return adb(['shell'].concat(cmd)); }
function sleep(ms) { execFileSync(process.execPath, ['-e', 'Atomics.wait(new Int32Array(new SharedArrayBuffer(4)),0,0,' + ms + ')']); }

// --- OWNER AUTHORITY BLOCK, Revision 1 (top / right / width in dp) ---
const CELLS = {
  C10: { wdp: 411, hdp: 914, font: 1.0, px: '822x1828', density: 320, top: 112, right: 8, width: 72 },
  C13: { wdp: 411, hdp: 914, font: 1.3, px: '822x1828', density: 320, top: 112, right: 8, width: 72 },
  L10: { wdp: 914, hdp: 411, font: 1.0, px: '1828x822', density: 320, top: 88, right: 8, width: 72 },
  L13: { wdp: 914, hdp: 411, font: 1.3, px: '1828x822', density: 320, top: 88, right: 8, width: 72 },
  E10: { wdp: 1280, hdp: 800, font: 1.0, px: '1280x800', density: 160, top: 108, right: 8, width: 72 },
  E13: { wdp: 1280, hdp: 800, font: 1.3, px: '1280x800', density: 160, top: 108, right: 8, width: 72 },
};
const TOLERANCE_DP = 4;

// The six visual-critical states that require a raw screenshot + overlay.
const SHOT_STATES = ['CONSTRUCTION_SHAPE_REST', 'TRANSFORM_MOVE_WORLD', 'TRANSFORM_SCALE',
  'EXACT_IME', 'SCULPT_REST', 'SCULPT_DETAILS'];

// --- uiautomator hierarchy ---
function dump(retries) {
  retries = retries === undefined ? 6 : retries;
  let last = null;
  for (let i = 0; i < retries; i++) {
    try {
      sh(['uiautomator', 'dump', '/sdcard/fs.xml']);
      const xml = sh(['cat', '/sdcard/fs.xml']);
      if (xml.indexOf('<hierarchy') >= 0) return xml;
    } catch (e) { last = e; }
    sleep(1000);
  }
  throw new Error('uiautomator dump failed: ' + (last && last.message));
}
function parseNodes(xml) {
  const nodes = [];
  const re = /<node\b([^>]*?)\/?>/g;
  let m;
  while ((m = re.exec(xml)) !== null) {
    const a = m[1];
    const attr = (n) => { const r = new RegExp(n + '="([^"]*)"').exec(a); return r ? r[1] : ''; };
    const b = /bounds="\[(-?\d+),(-?\d+)\]\[(-?\d+),(-?\d+)\]"/.exec(a);
    if (!b) continue;
    nodes.push({
      id: attr('resource-id').replace(PKG + ':id/', ''),
      cls: attr('class'), desc: attr('content-desc'), text: attr('text'),
      clickable: attr('clickable') === 'true', enabled: attr('enabled') === 'true',
      x1: +b[1], y1: +b[2], x2: +b[3], y2: +b[4],
    });
  }
  return nodes;
}
// A node scrolled out of its container is reported by uiautomator with a
// degenerate or inverted rectangle. It is not on screen, so it is neither a
// tappable target nor a visible hit area.
function visible(n) { return n && n.x2 > n.x1 && n.y2 > n.y1; }
function byId(nodes, id) { return nodes.filter((n) => n.id === id && visible(n))[0] || null; }

// A short window makes the right host taller than the window, so the accepted
// invariant applies: internal vertical scroll absorbs height pressure. Reaching
// a control by scrolling its own container is ordinary use, not a coordinate
// guess -- the target is still resolved by semantic id afterwards.
function scrollContainer(nodes, id) {
  const inner = ['field_', 'apply_', 'unit_', 'inspector_'].some((p) => id.indexOf(p) === 0);
  if (inner) return byId(nodes, 'inspector_scroll') || byId(nodes, 'property_inspector');
  return byId(nodes, 'tool_rail_scroll') || byId(nodes, 'workspace_trailing_host');
}
function tapId(id) {
  for (let attempt = 0; attempt < 5; attempt++) {
    const nodes = parseNodes(dump());
    const n = byId(nodes, id);
    if (n) {
      sh(['input', 'tap', String(Math.floor((n.x1 + n.x2) / 2)),
        String(Math.floor((n.y1 + n.y2) / 2))]);
      sleep(1400);
      return;
    }
    const c = scrollContainer(nodes, id);
    if (!c) break;
    const cx = Math.floor((c.x1 + c.x2) / 2), ch = c.y2 - c.y1;
    sh(['input', 'swipe', String(cx), String(Math.floor(c.y1 + ch * 0.75)),
      String(cx), String(Math.floor(c.y1 + ch * 0.25)), '350']);
    sleep(1300);
  }
  throw new Error('control not found by semantic id: ' + id);
}
function shot(name) {
  sh(['screencap', '-p', '/sdcard/fs.png']);
  adb(['pull', '/sdcard/fs.png', path.join(DIR, 'raw', name)]);
}

// --- per-cell run ---
const frames = [], resting = [], hits = [], config = [];
// The persistent binding set: chrome that must return to its exact resting
// bounds, plus the full-window Vulkan surface.
const PERSIST = ['global_toolbar', 'objects_capsule', 'history_group', 'objects_section',
  'brush_edge_controls', 'tool_rail', 'viewport_surface'];

function record(cell, state, nodes, scale) {
  const c = CELLS[cell];
  const h = byId(nodes, 'workspace_trailing_host');
  if (!h) throw new Error('workspace_trailing_host absent in ' + cell + '/' + state);
  const top = h.y1 / scale, right = c.wdp - h.x2 / scale;
  const width = (h.x2 - h.x1) / scale, height = (h.y2 - h.y1) / scale;
  const dTop = top - c.top, dRight = right - c.right, dWidth = width - c.width;
  const pass = Math.max(Math.abs(dTop), Math.abs(dRight), Math.abs(dWidth)) <= TOLERANCE_DP;
  frames.push({ cell, state, expTop: c.top, top, dTop, expRight: c.right, right, dRight,
    expWidth: c.width, width, dWidth, height, pass: pass ? 'PASS' : 'FAIL' });

  PERSIST.forEach((id) => {
    const n = byId(nodes, id);
    resting.push({ cell, state, element: id,
      x: n ? n.x1 / scale : '', y: n ? n.y1 / scale : '',
      w: n ? (n.x2 - n.x1) / scale : '', h: n ? (n.y2 - n.y1) / scale : '',
      present: n ? 'PRESENT' : 'ABSENT' });
  });

  // Raw visible intersections. Intrinsic size is resolved after the whole cell
  // has run, because a control clipped by a ScrollView in one state is reported
  // at its full box in another -- see resolveIntrinsic.
  nodes.filter((n) => n.clickable && n.id && visible(n)).forEach((n) => {
    hits.push({ cell, state, id: n.id,
      visibleW: (n.x2 - n.x1) / scale, visibleH: (n.y2 - n.y1) / scale });
  });
}

// Intrinsic-vs-clipped resolution lives in hits.js, so this harness and the
// analysis derive the floor verdict from exactly one rule.
const { resolve: resolveIntrinsic } = require('./hits');

function runCell(cell) {
  const c = CELLS[cell];
  sh(['wm', 'size', c.px]);
  sh(['wm', 'density', String(c.density)]);
  sh(['settings', 'put', 'system', 'font_scale', String(c.font)]);
  sleep(3000);
  const sizeOut = sh(['wm', 'size']), densOut = sh(['wm', 'density']);
  const scale = c.density / 160;

  sh(['am', 'force-stop', PKG]);
  sh(['pm', 'clear', PKG]);
  sleep(1000);
  sh(['am', 'start', '-n', ACT]);
  sleep(6000);
  tapId('start_option_construction');
  sleep(3000);

  const actual = /Override size: (\S+)/.exec(sizeOut) || /Physical size: (\S+)/.exec(sizeOut);
  const dens = /Override density: (\S+)/.exec(densOut) || /Physical density: (\S+)/.exec(densOut);
  const px = actual[1].split('x');
  config.push({ cell, reqPx: c.px, actPx: actual[1], reqDensity: c.density, actDensity: +dens[1],
    reqDp: c.wdp + 'x' + c.hdp, actDp: (+px[0] / scale) + 'x' + (+px[1] / scale),
    reqFont: c.font, actFont: sh(['settings', 'get', 'system', 'font_scale']).trim() });

  const step = (state, actions) => {
    actions.forEach((a) => a());
    sleep(900);
    const nodes = parseNodes(dump());
    record(cell, state, nodes, scale);
    if (SHOT_STATES.indexOf(state) >= 0) shot(cell + '-' + state + '.png');
    console.log(cell + ' ' + state + ' ok');
  };

  step('CONSTRUCTION_SHAPE_REST', [() => tapId('tool_rail_shape')]);
  step('TRANSFORM_MOVE_WORLD', [() => tapId('tool_rail_place'),
    () => tapId('transform_mode_move'), () => tapId('transform_space_world')]);
  step('TRANSFORM_ROTATE_WORLD', [() => tapId('transform_mode_rotate')]);
  step('TRANSFORM_SCALE', [() => tapId('transform_mode_scale')]);
  step('TRANSFORM_MOVE_LOCAL', [() => tapId('transform_mode_move'),
    () => tapId('transform_space_local')]);
  step('EXACT_OPEN', [() => tapId('precision_toggle')]);
  step('EXACT_IME', [() => tapId('field_pos_x'), () => sleep(2000)]);
  step('EXACT_CLOSE', [() => sh(['input', 'keyevent', '4']), () => sleep(1500),
    () => tapId('precision_toggle')]);
  step('SCULPT_REST', [() => tapId('freeze_to_sculpt'), () => sleep(8000)]);
  step('SCULPT_DETAILS', [() => tapId('precision_toggle')]);
  step('SCULPT_CLOSE', [() => tapId('precision_toggle')]);
}

function csv(file, cols, rows) {
  const out = [cols.join(',')];
  rows.forEach((r) => out.push(cols.map((c) => {
    const v = r[c]; return typeof v === 'number' ? (Math.round(v * 100) / 100) : v;
  }).join(',')));
  fs.writeFileSync(path.join(DIR, file), out.join('\n') + '\n');
}

const only = process.argv[2] ? process.argv[2].split(',') : Object.keys(CELLS);
let failure = null;
try {
  only.forEach(runCell);
} catch (e) {
  failure = e;
  console.error('RUN ERROR: ' + e.message);
} finally {
  // Restoration always runs, including on the failure path.
  try {
    sh(['wm', 'size', 'reset']); sh(['wm', 'density', 'reset']);
    sh(['settings', 'put', 'system', 'font_scale', '1.0']);
  } catch (e) { console.error('restore failed: ' + e.message); }
}
resolveIntrinsic(hits);
const tag = process.argv[3] ? '-' + process.argv[3] : '';
csv('frame-measurements' + tag + '.csv', ['cell', 'state', 'expTop', 'top', 'dTop', 'expRight',
  'right', 'dRight', 'expWidth', 'width', 'dWidth', 'height', 'pass'], frames);
csv('resting-bounds' + tag + '.csv', ['cell', 'state', 'element', 'x', 'y', 'w', 'h', 'present'], resting);
csv('hit-areas' + tag + '.csv', ['cell', 'state', 'id', 'intrinsicW', 'intrinsicH', 'intrinsicFrom',
  'visibleW', 'visibleH', 'clipped', 'pass'], hits);
csv('configuration-matrix' + tag + '.csv', ['cell', 'reqPx', 'actPx', 'reqDensity', 'actDensity',
  'reqDp', 'actDp', 'reqFont', 'actFont'], config);
fs.writeFileSync(path.join(DIR, 'frames' + tag + '.json'), JSON.stringify({ frames, resting, hits, config }, null, 1));
fs.appendFileSync(path.join(DIR, 'commands.txt'), CMDLOG.join('\n') + '\n');
console.log('frames=' + frames.length + ' hits=' + hits.length);
if (failure) process.exit(1);
