# Performance — SEL-OUT-R1 (the selection outline)

Measured on the isolated AVD `ForgeShape_Stage006` (`emulator-5580`, 1080×2400)
from the debug build, through `SelectionOutlineTest` and the renderer's own
bounded counters (`NativeViewport.selectionOutlineStats`).

**This is a measurement, not a benchmark.** The emulator is not a phone and no
absolute frame time is claimed for one. What the numbers exist to show is the
*shape* of the cost: that a selection change allocates nothing and rebuilds
nothing, and that turning the outline off removes the work rather than hiding
it.

---

## The one measured run

`seloutr1_09_10_27_measuresTheSelectionLoopForTheEvidencePackage` drives forty
selection changes between two bodies — twenty with the outline on, waiting for a
drawn frame after each, and twenty with it off — and prints one line:

```
FORGESHAPE_SELOUTR1_PERFORMANCE selectionChanges=40
  outlineAllocationsDuringLoop=0
  compositeDrawsWithOutlineOn=44 compositeDrawsWithOutlineOff=0
  meshRevisionBefore=25 meshRevisionAfter=25 fingerprintMoved=false
  maskExtent=1080x2400 bandHalfWidthPx=3.02 twentyOutlinedSwitchesMillis=814
```

| What §24 asks for | Measured |
| --- | --- |
| body mesh uploads on a selection switch | **0.** The mesh revision is 25 before and 25 after forty changes. `syncBody` uploads only when a body's source revision or the surface shading changed, so an unmoved revision is an unmoved buffer. |
| CAD tessellations / regenerations on a selection switch | **0.** No `.forge` value moves and the semantic fingerprint does not move, which it would if any CAD body had been regenerated. `SelectionOutlineTest.seloutr1_09_10_17_18_19` compares the encoded bytes directly and finds them identical. |
| outline resource allocations over an A↔B loop | **0** over twenty outlined switches. The whole twelve-frame visual journey — imports, a freeze, a real sculpt stroke, a CAD extrude, five palette changes, a Delete and its Undo — also reports `renderer_mask_allocations = 2` in **every** frame, unchanged from the first to the last. |
| frame/render overhead from current diagnostics | 20 outlined switches took **814 ms** wall clock, ~41 ms each. That figure is dominated by the test's own 30 ms frame-wait poll, not by render cost; it is reported because it is the only frame-scale number the existing instrumentation can produce, and it is not a frame time. |
| memory bound for the mask images | Two images at the render extent, freed with the swapchain — see below. |

`compositeDrawsWithOutlineOff = 0` is the sharpest number here: with the toggle
off, the renderer records **neither** the mask pass nor the composite draw. Off
costs one boolean per frame, not a pass that paints nothing.

---

## What the outline costs when it is on

Per frame, and only while a drawable body is selected:

* **one extra render pass** over the scene's bodies with a position-only vertex
  shader and a one-instruction fragment shader, into a single-channel image.
  Every body is drawn, because the unselected ones are what write the depth that
  occludes the selected one;
* **one full-screen triangle** whose fragment stage does 1 texture fetch for an
  interior pixel (it discards immediately) and 13 for the rest — a ring of 8 at
  the band radius and a ring of 4 at half it, offset by half a step;
* **no vertex buffer, no index buffer and no upload of any kind.** The mask pass
  binds the body's *own* device-local buffers, the ones its shaded draw binds a
  few instructions later; the composite has no vertex buffer at all and comes
  from `gl_VertexIndex`.

CPU cost per frame is two `vkCmdPushConstants` per body plus one bind and one
draw for the composite. There is no per-frame allocation, no map, no readback
and no CPU geometry work of any kind.

---

## Resource bound

Created with the swapchain, destroyed with it, and sized from the render extent:

| Resource | Size at 1080×2400 |
| --- | --- |
| mask image, `R8_UNORM` | 1 byte/px ≈ **2.6 MB** |
| mask pass depth image, the main pass's depth format | ≈ **10 MB** (D24S8) |
| mask render pass, framebuffer, two pipelines | fixed, one each |
| sampler, descriptor set layout, pool, **one** descriptor set | device-scoped, allocated once for the life of the device |

The descriptor set is **rewritten** rather than reallocated when the extent
changes, so the descriptor allocation count is fixed for the life of the device
no matter how many rotations or device rebuilds occur.

`renderer_mask_allocations` moves on exactly two events — a swapchain extent
change and a device rebuild — and the suite asserts it moves on neither a
selection change, a toggle, nor a palette change
(`seloutr1_27_outlineResourcesAreBoundedAndDrivenOnlyByTheExtent`), and that it
*does* move across an injected device loss
(`seloutr1_26_aDeviceRebuildRestoresTheOutline`).

The mask pass carries its own depth image rather than sharing the main pass's.
Sharing would save ~10 MB and would put a write-after-write hazard between the
mask pass's depth writes and the main pass's depth clear, which the main render
pass's existing external subpass dependency does not cover. Widening a
dependency in the one render pass every frame already depends on, to save an
image that is freed with the swapchain, is the wrong trade — and it is recorded
here rather than left as an unexplained extra allocation.

---

## Not optimized beyond a demonstrated issue

Two obvious optimizations were considered and **not** taken, because nothing
measured asks for them:

* **a scissor around the selected body's screen bounds**, so the composite
  shades only the region that can contain a band. It would need the CPU to
  project the body's bounds each frame, which is new coupling between the
  renderer and the geometry it deliberately only rasterises;
* **a half-resolution mask**, which would quarter both images and the mask pass's
  fill. It costs accuracy on thin geometry, and the band is 3 px wide.

Both are recorded so a future stage with a real frame-time problem knows where
to look first.
