# Stage 020R3 — runtime evidence

Taken on `ForgeShape_Stage006` / `emulator-5580`, compact portrait, with
ForgeShape confirmed as the resumed activity before any input. Every control was
located by its **stable semantic id** and its tap point derived from that
control's own runtime bounds; no screen coordinate appears in the driving
script.

## What the numbers mean

`FORGESHAPE_SCULPT_STROKE_BEGIN` carries three lengths, all in world meters:

| field | meaning |
| --- | --- |
| `radiusWorld` | what the authored screen-pixel radius resolved to at the hit depth |
| `maxWorld` | how far the affected set reaches in the **displayed** metric |
| `maxLocal` | how far it reaches in the body's **own stretched** coordinates |

A brush measuring in the world metric keeps `maxWorld` inside `radiusWorld`
whatever the Scale is. `maxLocal` is free to differ — and on a non-uniformly
scaled body it does, which is exactly the set the pre-020R3 kernel would have
chosen instead.

## Result

| body | brush | radiusWorld | maxWorld | maxLocal |
| --- | --- | --- | --- | --- |
| Scale (1,1,1) | Grab | 0.4447 | 0.4052 | **0.4052** |
| Scale (1,1,1) | Clay | 0.4483 | 0.4472 | **0.4472** |
| Scale (1,1,1) | Smooth | 0.4392 | 0.4390 | **0.4390** |
| Scale (1,1,1) | Inflate | 0.4416 | 0.4411 | **0.4411** |
| Scale (3,1,1) | Grab | 0.4446 | 0.4420 | 0.3998 |
| Scale (3,1,1) | Clay | 0.4473 | 0.4376 | 0.4346 |
| Scale (3,1,1) | Smooth | 0.4384 | 0.4369 | 0.4228 |
| Scale (3,1,1) | Inflate | 0.4418 | 0.4392 | 0.4350 |
| Scale (3,1,1) + rotY 35° | Grab | 0.4446 | 0.4405 | 0.3882 |
| Scale (3,1,1) + rotY 35° | Clay | 0.4483 | 0.4474 | 0.4393 |
| Scale (3,1,1) + rotY 35° | Smooth | 0.4392 | 0.4261 | 0.4066 |
| Scale (3,1,1) + rotY 35° | Inflate | 0.4418 | 0.4413 | 0.4408 |

Three things this says, all of them the acceptance criteria:

1. **`maxWorld < radiusWorld` in every row.** The footprint is inside the world
   ball, so it is round in the displayed metric, for all four brushes, scaled
   and scaled-and-turned alike.
2. **Unscaled: `maxWorld == maxLocal` exactly, in all four rows.** At
   S = (1,1,1) the two metrics are the same metric, so nothing about an
   unscaled body changed. Scaled: they differ in all eight rows, so the metric
   is genuinely scale-aware rather than measuring nothing.
3. **`radiusWorld ≈ 0.44 m` throughout, unchanged by the body's Scale.** The
   screen-pixel radius contract survives: Scale sizes the BODY, never the
   instrument.

The placement read back after all of it, from the precision surface:
`pos = (0, 0, 0)`, `rot = (0, 35, 0)`, `scale = (3, 1, 1)` — exactly what was
typed, untouched by sculpting. Resume Sculpt returned the same 482-vertex
sculpt mesh.

## Files

| file | what it shows |
| --- | --- |
| `S020R3-R00-runtime-transcript.txt` | the whole driven run, tap by tap, with every log line |
| `S020R3-R01-unscaled.png` | the sphere at Scale (1,1,1) |
| `S020R3-R02-unscaled-sculpted.png` | after one stroke of each brush |
| `S020R3-R03-scale-3-1-1.png` | the same body at Scale (3,1,1) |
| `S020R3-R04-scale-3-1-1-sculpted.png` | after one stroke of each brush on the stretched body |
| `S020R3-R05-scaled-and-rotated.png` | Scale (3,1,1) with a 35° Y rotation, sculpted |
| `S020R3-R06-resumed.png` | Resume Sculpt, same mesh |

Screenshots are supporting context. The numeric table above is the evidence.
