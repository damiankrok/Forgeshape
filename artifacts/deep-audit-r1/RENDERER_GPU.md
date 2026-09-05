# Renderer / GPU — DEEP-AUDIT-R1 (Part K)

## What was read

`forgeshape_renderer.{h,cpp}` (2980 lines), `forgeshape_render_mesh.{h,cpp}`, `forgeshape_grid.{h,cpp}`, `forgeshape_matcap.{h,cpp}`, `forgeshape_display.{h,cpp}`, `forgeshape_render_recovery.{h,cpp}`, `forgeshape_selection_pulse.{h,cpp}`, the shaders (`surface`/`grid`/`gizmo` .vert/.frag), and the render/upload paths in `forgeshape_jni.cpp`. Render-shading and render-recovery run on the device launch (`FORGESHAPE_RENDER_SHADING_SELFTEST_OK 329`, `FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK 24`); they are excluded from the platform-neutral standalone runner because they touch the render seam.

## Confirmed

* **Full-bleed surface, identity preTransform.** `FORGESHAPE_SURFACE_CONFIG` on the device: `window=1080x2400 chosenExtent=1080x2400 preTransform=0x1 currentTransform=0x1`. `chosenExtent` always equals the window; the frame loop does not rebuild on `VK_SUBOPTIMAL_KHR` (only `VK_ERROR_OUT_OF_DATE_KHR` and the explicit resize). No layout inset ever reaches the `SurfaceView` (verified in `EditorWorkspaceView` — insets go on the chrome container).
* **Per-body GPU resources** keyed by `ObjectId`; `RenderMeshCache` (crease 40°) derives normals one-way from a published `RuntimeMesh`; render-only vertex duplication for hard edges (render verts ≥ source, indices equal). `releaseBodiesAbsentFromScene` frees a departed body's buffers after a bounded (100 ms) in-flight wait — the fix ARCH-HEALTH-01/IMPORT-01B added. `GIZMO_UPLOAD_OK` appears once per device (verified once in the launch log).
* **Push constants** 128-byte blocks; one descriptor set; static viewport/scissor; `RenderRecoveryPolicy` 2 rebuilds then `RestartRequired`, self-tested without a GPU.
* **Shading is presentation.** No display setting mints a revision; Studio/MatCap/Faceted rebuild only the derived render mesh; `FORGESHAPE_RENDER_MESH_BUILD` is per-rebuild, never per-frame (2 rebuilds over 4448 frames in the shading cost record).

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| — | OK | No new renderer defect found. The device-loss recovery path is exercised only through the debug injection seam (a real loss is forbidden on the authoritative emulator) — an accepted, unavoidable coverage limit. |
| existing | P3 | Carried debt re-confirmed, not changed: one redundant swapchain rebuild at startup and per resume (`surfaceChanged` right after `surfaceCreated`); `MeshStore::publish` validates twice; retired buffers freed inline after the fence wait; colour still travels in `RenderVertex` only for the debug source-colour mode; the identity-preTransform convention costs one compositor rotation while rotated (the one renderer decision a future perf stage might revisit). All are in PROJECT_STATUS Technical Debt. |
| — | OK | The `createSwapchain` non-identity-transform fallback is still UNVERIFIED (`emulator-5558`/`5580` report `supportedTransforms=0x1ff`, so the branch never executes) — recorded in PROJECT_STATUS, unchanged. |
