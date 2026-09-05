# Security, manifest and file handling — DEEP-AUDIT-R1 (Part M)

## Manifest

`android:allowBackup="false"`; ONE exported component (the launcher Activity); no `<provider>`, `<service>`, `<receiver>`; **no `<uses-permission>` of any kind** (verified: `aapt2 dump permissions` returns only the package line; `aapt2 dump badging` shows no `uses-permission`). Vulkan feature `android.hardware.vulkan.version` required. So ForgeShape holds no network permission — "sends nothing anywhere" is a structural fact, confirmed at the APK.

## Attack surface

* The only inputs are `.forge` and `.glb` bytes (both through the fail-closed decoders above) and touch events. No IPC, no intent extras beyond the SAF `Uri` (which stops at `ProjectTransfer` and never reaches the domain).
* `ProjectTransfer` bounds every read at 256 MiB before allocating, uses `"wt"` (truncating) writes so a chosen larger file cannot leave a valid header in front of foreign trailing bytes, and swallows `SecurityException`/`IllegalArgumentException` from a hostile provider as a plain failure.
* `.forge` decode is bounds-first (counts checked before allocation, CRC over every payload, section lengths against the file); GLB parse bounds every accessor read and the JSON depth/value count. No `reinterpret_cast` over file bytes; no `system`/`exec`/`dlopen` of file-derived paths (grep: none).
* **F-03 was the one memory-safety-adjacent defect**: a file-supplied name (an emoji, a `😀` in a glTF node name) reached `NewStringUTF` as 4-byte UTF-8 and aborted a debuggable process via CheckJNI. Not an exploit (a controlled abort, debug-only), but a crash on attacker-influenced input — fixed by converting to UTF-16 at the boundary.

## Diagnostics / privacy

`DiagnosticLog` is a 200-record / 64 KiB-rendered in-memory ring of tokens; `sanitize` strips separators/newlines and truncates at 120 chars so no geometry, vertex, dimension, `.forge` byte, path or `Uri` fits. The only egress is the user picking a destination through the system document UI (Share Diagnostics). The user's email/identity never appears. Verified in `DiagnosticLog`/`Diagnostics` (JVM `DiagnosticLogTest` covers bounds + redaction).

## Licenses / third-party

No third-party runtime code: native NEEDED libraries are platform-only (`libvulkan`/`libandroid`/`liblog`/`libm`/`libdl`/`libc`); no GLM, no engine, no interchange library; the exporter/importer write and read the container and JSON themselves. Test-only deps (JUnit 4.13.2, androidx.test) are `test`/`androidTest` scope and packaged into the test APK only. One `SPDX`/license-ish string in the tree (`forgeshape_matcap.h`, the MatCap formula's own header comment) — first-party. No bundled Vulkan validation layer (CLAUDE rule; verified absent from `jniLibs` and the APK).

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| F-03 | P1 | Crash (CheckJNI abort, debuggable) on a GLB whose node name carries a supplementary character. | FIXED |
| — | OK | No permission, no exported IPC, no network, no path injection, no unbounded allocation from file input. Manifest and file-handling clean. |
