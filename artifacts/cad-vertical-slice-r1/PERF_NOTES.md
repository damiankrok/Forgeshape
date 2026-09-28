# Performance notes — the boolean path, measured

This page is a focused regression guard for the new high-risk path (prompt
§19); it is not a global performance baseline. Every number below was measured,
not inferred from source.

- **Device** means the CI DEVICE emulator: API 36 x86_64 under KVM, rendering
  with SwiftShader (a software Vulkan).
- **Emulator timings are not phone timings.** The emulator is a slower, CPU-only
  renderer, and no physical-device performance gate is closed here.

## 1. The acceptance model, regenerated

The acceptance model is a 4 × 3 m rectangle-with-hole base, one Add and one Cut.
The `CAD_FEATURE` self-test regenerates it on every debug launch and reports the
median of five runs as `FORGESHAPE_CAD_FEATURE_PERFORMANCE`.

| Step | Host (Linux x86_64, `-O2`) | Device (CI DEVICE run `36333563367`) |
| --- | ---: | ---: |
| Base feature (region with a hole, no kernel) | 87–132 µs | **247 µs** |
| Base + Add (one kernel union) | 807–1225 µs | **1 607 µs** |
| Base + Add + Cut (two kernel booleans) | 2 143–3 292 µs | **4 149 µs** |
| `.forge` encode + decode round trip of that body | 4 432–6 816 µs | **8 759 µs** |
| Result size | 288 triangles | 288 triangles |

## 2. In the product, per preview

These are read from `CAD_EXTRUDE_PREVIEW_MICROS` by `CadVerticalSliceTest`, on
the device, in the same run.

| Preview | Time |
| --- | ---: |
| Owner's ring (4 × 2.8 − r0.8 disk), New Body, no kernel | **439 µs** |
| Add on a 2 × 2 × 1 body, as staged | **804 µs** |
| Add, after an arrow drag (the last drag sample's evaluation) | **510 µs** |
| Cut on the same body, as staged | **1 291 µs** |
| Cut, after an arrow drag | **644 µs** |

At 60 fps a frame is 16.7 ms, so a preview costs about 3–8 % of a frame even
on the software emulator.

## 3. How the work is bounded

- **Latest-only, never queued.** A drag sample writes the extrusion through the
  one writer and bumps `candidateRevision_`. Nothing is queued per MotionEvent.
  The render thread calls `evaluateCandidate()` at most once per frame. It
  regenerates only when the revision it holds is stale, so twenty drag samples
  inside one frame cost ONE evaluation, of the newest values.
- **Unchanged inputs do no kernel work.** An unchanged candidate is evaluated
  once and the same mesh is returned (`CADVS_SES_08`). A changed one gets a new
  revision and a new mesh (`CADVS_SES_09`).
- **The published body is cached.** `CadBody` caches its last regeneration,
  and every state change resets that cache. Drawing, picking and the support
  chooser therefore never re-run the kernel for a body whose chain did not
  change.
- **Commit re-validates once.** A commit runs one more `applyState`
  regeneration of the same chain (the kernel time in §1), inside the
  transaction. It happens once per user act, never per frame.
- **Save and load re-validate once.** Encode and decode regenerate a
  chain-bearing body once, to validate it: about 9 ms on the device for the
  acceptance model. Autosave runs on its own worker thread, so no frame pays
  it.
- **Hard bounds:**
  - 16 features per body;
  - 16 regions per feature;
  - 64 holes per region;
  - 256 entities per sketch;
  - `kMaxCadKernelTriangles` = 262 144 per kernel result.
- **Peak intermediate meshes.** One prism per feature (per selected region)
  plus the running solid. In the acceptance model the largest intermediate is
  under 300 triangles, and the committed ring body is 144.
- **Nothing derived is serialized.** No mesh cache, face table or kernel
  output is written to `.forge`.

## 4. Release safety

- **Guard.** `scripts/ci-release-selftest-guard.sh` now also matches `CADVS_`.
  Release has 0 `SelfTests` symbols and 0 self-test strings on both ABIs; debug
  has 23 symbols and 391 strings (local build, and CI FAST).
- **Kernel build.** Manifold is built without `MANIFOLD_DEBUG`, so no
  debug/throw paths are compiled. It is single-threaded, so there is no thread
  pool.

## 5. Size

See `KERNEL_GATE.md` §5. The kernel adds +727 672 B (arm64) and +852 400 B
(x86_64) to the native library. The release APK grows by +1 607 028 B, from
2.90 MB to 4.51 MB.

## 6. Not measured here

- Physical-device frame times.
- A 16-feature chain on a phone. It runs bounded on the host
  (`CADVS_OPS_27`), but was not timed on a device.
- Memory high-water mark. The emulator run records no RSS.
