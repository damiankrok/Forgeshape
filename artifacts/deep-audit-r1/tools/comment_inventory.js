// Comment inventory for hand-written C++/Java sources.
// Usage: node comment_inventory.js <repoRoot> [--json out.json]
// Counts per file: total, blank, code, comment lines (// and /* */; a line with
// both code and a trailing comment counts as code). Comment blocks are runs of
// consecutive comment-only lines. Reports >8/>15/>30 block counts and top-N.
const fs = require('fs');
const path = require('path');
const root = process.argv[2] || '.';
const { execSync } = require('child_process');
const files = execSync('git ls-files', { cwd: root }).toString().split('\n')
  .filter(f => /\.(cpp|h|java)$/.test(f));
const results = [];
const blocks = [];
for (const rel of files) {
  const text = fs.readFileSync(path.join(root, rel), 'utf8');
  const lines = text.split('\n');
  let inBlock = false;
  let total = 0, blank = 0, code = 0, comment = 0;
  let run = 0, runStart = 0;
  const flush = (endLine) => {
    if (run > 0) blocks.push({ file: rel, start: runStart, len: run });
    run = 0;
  };
  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    if (i === lines.length - 1 && raw === '') break;
    total++;
    const s = raw.trim();
    let isCommentOnly = false;
    if (inBlock) {
      isCommentOnly = true;
      if (s.includes('*/')) {
        inBlock = false;
        const after = s.slice(s.indexOf('*/') + 2).trim();
        if (after.length > 0 && !after.startsWith('//')) isCommentOnly = false;
      }
    } else if (s === '') {
      blank++;
      flush(i);
      continue;
    } else if (s.startsWith('//')) {
      isCommentOnly = true;
    } else if (s.startsWith('/*')) {
      isCommentOnly = true;
      if (!s.includes('*/')) inBlock = true;
      else {
        const after = s.slice(s.indexOf('*/') + 2).trim();
        if (after.length > 0 && !after.startsWith('//')) isCommentOnly = false;
      }
    } else {
      // code line; may open a block comment at its end
      const bi = s.indexOf('/*');
      if (bi >= 0 && s.indexOf('*/', bi) < 0 && s.indexOf('//') < 0) inBlock = true;
    }
    if (isCommentOnly) { comment++; if (run === 0) runStart = i + 1; run++; }
    else { code++; flush(i); }
  }
  flush(lines.length);
  results.push({ file: rel, total, blank, code, comment,
    pct: total ? +(100 * comment / (code + comment)).toFixed(1) : 0 });
}
const gt = n => blocks.filter(b => b.len > n).length;
const isTest = f => /selftest|\/test\/|\/androidTest\//.test(f);
const sum = (arr, k) => arr.reduce((a, r) => a + r[k], 0);
const prod = results.filter(r => !isTest(r.file));
const test = results.filter(r => isTest(r.file));
const cpp = results.filter(r => /\.(cpp|h)$/.test(r.file));
const java = results.filter(r => /\.java$/.test(r.file));
const line = (label, arr) => `${label}: files=${arr.length} total=${sum(arr,'total')} code=${sum(arr,'code')} comment=${sum(arr,'comment')} blank=${sum(arr,'blank')} pct=${(100*sum(arr,'comment')/(sum(arr,'code')+sum(arr,'comment'))).toFixed(1)}%`;
console.log(line('ALL', results));
console.log(line('PRODUCTION', prod));
console.log(line('TEST', test));
console.log(line('CPP+H', cpp));
console.log(line('JAVA', java));
console.log(line('CPP+H production', cpp.filter(r => !isTest(r.file))));
console.log(line('JAVA production', java.filter(r => !isTest(r.file))));
console.log(`BLOCKS: total=${blocks.length} >8=${gt(8)} >15=${gt(15)} >30=${gt(30)}`);
const prodBlocks = blocks.filter(b => !isTest(b.file));
console.log(`PRODUCTION BLOCKS: total=${prodBlocks.length} >8=${prodBlocks.filter(b=>b.len>8).length} >15=${prodBlocks.filter(b=>b.len>15).length} >30=${prodBlocks.filter(b=>b.len>30).length}`);
console.log('--- TOP 50 LONGEST BLOCKS ---');
blocks.sort((a, b) => b.len - a.len).slice(0, 50).forEach(b => console.log(`${b.len}\t${b.file}:${b.start}`));
console.log('--- TOP 30 FILES BY COMMENT DENSITY (>=200 lines) ---');
results.filter(r => r.total >= 200).sort((a, b) => b.pct - a.pct).slice(0, 30)
  .forEach(r => console.log(`${r.pct}%\t${r.comment}/${r.total}\t${r.file}`));
console.log('--- TOP 30 FILES BY COMMENT LINES ---');
results.sort((a, b) => b.comment - a.comment).slice(0, 30)
  .forEach(r => console.log(`${r.comment}\t${r.pct}%\t${r.total}\t${r.file}`));
const jsonIdx = process.argv.indexOf('--json');
if (jsonIdx > 0) fs.writeFileSync(process.argv[jsonIdx + 1], JSON.stringify({ results, blocks }, null, 1));
