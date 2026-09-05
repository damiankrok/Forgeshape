const fs = require('fs');
const cpp = fs.readFileSync('app/src/main/cpp/forgeshape_jni.cpp', 'utf8');
const java = fs.readFileSync('app/src/main/java/com/forgeshape/app/NativeViewport.java', 'utf8');
// Java signatures
const sig = {};
for (const m of java.matchAll(/static native ([^\s]+(?:\[\])?) ([A-Za-z0-9_]+)\s*\(([^)]*)\)/gs)) {
  sig[m[2]] = { ret: m[1], args: m[3].replace(/\s+/g, ' ').trim() };
}
// C++ bodies
const re = /JNIEXPORT\s+([A-Za-z0-9_]+)\s+JNICALL\s+Java_com_forgeshape_app_NativeViewport_([A-Za-z0-9_]+)\s*\(([^)]*)\)\s*\{/g;
const starts = [];
let m;
while ((m = re.exec(cpp)) !== null) starts.push({ ret: m[1], name: m[2], at: m.index, bodyAt: re.lastIndex });
const rows = [];
for (let i = 0; i < starts.length; i++) {
  const s = starts[i];
  const end = i + 1 < starts.length ? starts[i + 1].at : cpp.length;
  const body = cpp.slice(s.bodyAt, end);
  const locked = /lock_guard<std::mutex>\s*lock\(g_stateMutex\)/.test(body) || /std::lock_guard<std::mutex> lock\(g_stateMutex\)/.test(body);
  const vpLock = /g_viewport\.mutex/.test(body);
  const debugOnly = /#ifndef NDEBUG|#ifdef NDEBUG|\bNDEBUG\b/.test(body);
  const guards = [];
  if (/hasProject\(\)/.test(body)) guards.push('hasProject');
  if (/inSculptMode\(\)/.test(body)) guards.push('inSculptMode');
  if (/editInProgress\(\)/.test(body)) guards.push('editInProgress');
  if (/sketchSession\(\)\.active\(\)/.test(body)) guards.push('sketch.active');
  if (/ExceptionCheck/.test(body)) guards.push('ExceptionCheck');
  if (/GetByteArrayRegion|GetArrayLength|SetDoubleArrayRegion|SetFloatArrayRegion|SetLongArrayRegion|SetIntArrayRegion|GetIntArrayRegion|GetFloatArrayRegion|NewByteArray|NewString/.test(body)) guards.push('array/string xfer');
  const ret = (body.match(/return ([^;]{1,60});/g) || []).map(x => x.replace(/^return /, '').replace(/;$/, '')).filter((v, i, a) => a.indexOf(v) === i).slice(0, 4).join(' | ');
  const js = sig[s.name] || { ret: '?', args: '?' };
  rows.push(`| \`${js.ret} ${s.name}(${js.args})\` | \`Java_..._${s.name}\` | ${locked ? 'g_stateMutex' : (vpLock ? 'g_viewport.mutex' : 'none')} | ${debugOnly ? 'debug-only' : ''} ${guards.join(', ')} | ${ret.replace(/\|/g, '\|')} |`);
}
console.log(rows.length);
fs.writeFileSync(process.argv[2], rows.join('\n'));
