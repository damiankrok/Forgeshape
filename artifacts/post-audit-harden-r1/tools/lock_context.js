// Classifies every call of a process-scoped accessor in forgeshape_jni.cpp by
// whether it executes inside a scope that holds g_stateMutex. Brace tracking is
// textual after stripping comments and string literals line by line.
//
//   node lock_context.js <path/to/forgeshape_jni.cpp> [pattern ...]
//
// Default patterns: `sculptSession()` (rebinds the borrowed sculpt target, so
// it must run under the lock) and `logSculptStateLocked(` (a helper that
// requires the lock from its caller, so its call sites must be locked).
//
// Convention: a helper whose name ends in "Locked" requires g_stateMutex from
// its caller; its own body counts as locked and its CALL SITES are checked.
// A call marked with a trailing "!" is outside every g_stateMutex scope.
const fs = require('fs');
const path = process.argv[2];
const orig = fs.readFileSync(path, 'utf8').split('\n');
const patterns = process.argv.slice(3).length ? process.argv.slice(3)
  : ['sculptSession()', 'logSculptStateLocked('];
let inBlock = false;
const lines = orig.map(l => {
  let o = '';
  for (let i = 0; i < l.length; i++) {
    if (inBlock) { if (l[i] === '*' && l[i + 1] === '/') { inBlock = false; i++; } continue; }
    if (l[i] === '/' && l[i + 1] === '*') { inBlock = true; i++; continue; }
    if (l[i] === '/' && l[i + 1] === '/') break;
    if (l[i] === '"' || l[i] === "'") {
      const q = l[i]; i++;
      while (i < l.length && l[i] !== q) { if (l[i] === '\\') i++; i++; }
      continue;
    }
    o += l[i];
  }
  return o;
});
const stack = [];
let fn = '(file scope)';
const out = [];
for (let i = 0; i < lines.length; i++) {
  const line = lines[i];
  const jm = orig[i].match(/^Java_com_forgeshape_app_NativeViewport_(\w+)\(/);
  const fm = orig[i].match(/^(?:static\s+|inline\s+|constexpr\s+)?[\w:<>]+(?:\s*[&*])?\s+([A-Za-z_]\w*)\(/);
  if (jm) fn = jm[1]; else if (fm && !/^(if|for|while|switch|return|else)$/.test(fm[1])) fn = fm[1];
  const lockRequired = /Locked$/.test(fn);
  const lockHere = /(lock_guard|unique_lock)<std::mutex>\s+\w+\(g_stateMutex\)/.test(line);
  if (lockHere && stack.length) stack[stack.length - 1].locked = true;
  const lockedNow = lockRequired || stack.some(b => b.locked);
  for (const p of patterns) {
    let idx = line.indexOf(p);
    while (idx >= 0) { out.push({ line: i + 1, fn, pattern: p, locked: lockedNow }); idx = line.indexOf(p, idx + 1); }
  }
  for (const c of line) { if (c === '{') stack.push({ locked: false }); else if (c === '}') stack.pop(); }
}
const byFn = {};
for (const o of out) (byFn[o.fn] = byFn[o.fn] || []).push(o);
let unlockedTotal = 0;
for (const [f, calls] of Object.entries(byFn)) {
  const unlocked = calls.filter(c => !c.locked);
  unlockedTotal += unlocked.length;
  console.log(`${unlocked.length ? 'UNLOCKED' : 'locked  '} ${f}: ` + calls.map(c => `${c.pattern.replace(/\(\)?$/, '')}@${c.line}${c.locked ? '' : '!'}`).join(' '));
}
console.log(`TOTAL calls=${out.length} unlocked=${unlockedTotal}`);
process.exitCode = unlockedTotal === 0 ? 0 : 1;
