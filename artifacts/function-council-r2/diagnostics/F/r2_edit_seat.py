p="/home/user/Forgeshape/artifacts/function-council-r2/SEAT_F.md"
t=open(p).read()
def rep(old,new):
    global t
    assert t.count(old)==1, old[:80]
    t=t.replace(old,new)
rep("  - `evidence/local/{host_selftests.txt (HOST_SELFTESTS_OK 4392/0), release_guard.txt (RELEASE_SELFTEST_GUARD=PASS), run_status.txt}`;",
    "  - `evidence/local/{host_selftests.txt (HOST_SELFTESTS_OK 4392/0), release_guard.txt (RELEASE_SELFTEST_GUARD=PASS), run_status.txt}`. Guard provenance is incomplete: `run_status.txt` records `guard_exit=2` at 00:54:48 for the first invocation; the PASS file was written at 00:57:29 by a rerun whose command and exit code are not recorded (R2 objection 15). Graded \"E1, provenance incomplete\";")
rep("- `SketchUxTest.cadUxR1_11_12_13` asserts the navigator's **native bits only**.",
    "- `SketchUxTest.cadUxR1_11_12_13_rotationTurnsTheViewAndNotTheSketch` asserts the navigator's **native bits only** (`SketchUxTest.java:217-232`); its name promises a view rotation that nothing in it reads.")
rep("""- Back with a surface or palette open inside the bootstrap;""",
"""- Back with a surface or palette open inside the bootstrap;
- System Back through the real platform route for any surface or phase other than Add Primitive in a Construction project (only `uilr106` sends `KEYCODE_BACK`; every other Back test calls `dismissTopmostSurface()` directly and bypasses the API ≥ 33 arming predicate);
- occlusion of the status line (`uilr120` and the visibility ledger pass over the overlapped Sculpt state);
- a camera pose after a navigator Flip or ±90°;""")
# F-01
rep("### F-01 — P2 — Several chrome controls fall below the 48 dp hit-area floor in width",
    "### F-01 — P2 — Several chrome controls fall below the 48 dp hit-area floor in width\n\nOne severity for every instance: P2 (`req_F.tsv` UI-011..UI-014; UI-014 corrected from P3 in Round 2).")
rep("  - **`sketch_dimension_value`** (container `sketch_dimension_label`): 114x126 px = **43.4 dp** wide. Source: capture 07.",
    "  - **`sketch_dimension_label`** (the `FrameLayout` container, `SketchDimensionLabelView.java:79`; measured at `SketchUxVisualEvidenceTest.java:202`): 114x126 px = **43.4 dp** wide. Source: capture 07. The value chip `sketch_dimension_value` (`:82`) was not measured; its hit area is ≤ 114 px wide by inference (a child receives touches only inside its parent).")
# F-02
rep("- **Fix:** frame `viewFrame()` in `beginSketchView`.",
    "- **False-green test:** `SketchUxTest.cadUxR1_11_12_13_rotationTurnsTheViewAndNotTheSketch` (`SketchUxTest.java:209-233`) asserts only `SKETCH_VIEW_QUARTER_TURNS` and unchanged entity values (`:217-232`); `cadUxR1_08_09_10` asserts only `SKETCH_VIEW_FLIPPED` for Flip (`:199-205`). Neither reads the camera, so both pass while the behaviour the first one's name promises is absent. Listed under F-09.\n- **Fix:** frame `viewFrame()` in `beginSketchView`.")
# F-03
rep("- **Impact:** the user loses every drawn entity by pressing Back to close a panel. The sketch is volatile, so this is not project data loss, but there is no undo. The code's own comment (`:1150-1154`, \"innermost outward\") and `uilr106` (\"Back dismisses the topmost surface before it leaves\") state the contract this breaks.",
    "- **Impact:** the user loses every drawn entity by pressing Back to close a panel. The sketch is volatile, so this is not project data loss, but there is no undo.\n- **Contract broken:** `ForgeShapeActivity.java:68-72` (\"'works' means the innermost thing the user opened closes first\") and the name of `uilr106` (\"Back dismisses the topmost surface before it leaves\"). The comment at `EditorWorkspaceView.java:1150-1154` does NOT support this finding: it documents the current order on purpose (\"The questions first, innermost outward … Then the CAD bootstrap\").\n- **Conflicting documentation:** `PRODUCT.md:196-198` says \"System Back always goes one step: … takes a New-CAD sketch back to Home\", which the current order delivers literally. Two owner records disagree, so the panel-open bootstrap Back order is also listed under owner decision 1.")
rep("""  - `HomeFlowTest.e2eAppH1_11` tests bootstrap Back with nothing open.""",
    """  - `HomeFlowTest.e2eAppH1_11` (`:449-482`) tests bootstrap Back with nothing open, by calling `dismissTopmostSurface()` directly.""")
# F-04
rep("### F-04 — P2 — On API 33+, Back during an in-project Revolve backgrounds the app; below API 33 it returns to Extrude",
    "### F-04 — P2 — On API 36 (targetSdk 36), Back during an in-project Revolve backgrounds the app; on the legacy route it returns to Extrude")
rep("- **Verdict:** DEFECT-CONFIRMED. **Grade:** E2.\n- **Location:**\n  - `dismissTopmostSurface` handles the case",
    "- **Verdict:** DEFECT-CONFIRMED, scoped to API 36 with targetSdk 36. **Grade:** E2, plus the `uilr106` route evidence that the dispatcher is the live route there (`EditorWorkspaceLegibilityTest.java:341-347`, fresh PASS). **API 33-35: UNVERIFIED (E5).** The manifest has no `android:enableOnBackInvokedCallback`, so on an API 33-35 device the legacy `onBackPressed` route (`ForgeShapeActivity.java:104-110`) may still be the effective one, and there the revolve Back works.\n- **Location:**\n  - `dismissTopmostSurface` handles the case")
rep("  2. Press System Back on an API 33+ device. The CI emulator is API 36 and `targetSdk` is 36.",
    "  2. Press System Back on an API 36 device. The CI emulator is API 36 and `targetSdk` is 36.\n- **Tests:** none sends Back in this state. `CadRevolveOwnerTest` presses `sketch_revolve_back_to_extrude` (`:464`) and sends no Back key (UI-010 tests column corrected in Round 2).")
# F-05
rep("- **Verdict:** DEFECT-CONFIRMED. **Grade:** E2 (key-listener sets) + E1 (`LengthUnitTest.acceptsACommaDecimalSeparatorAndASignedValue` PASS, fresh JVM run: the parser accepts `\"1,5\"`).",
    "- **Verdict:** DEFECT-CONFIRMED. **Grade:** defect E2; parser behaviour E1 (`LengthUnitTest.acceptsACommaDecimalSeparatorAndASignedValue` PASS, fresh JVM run: the parser accepts `\"1,5\"`). No field-level runtime evidence; J-F4 is the runtime reproducer owed.")
# F-06
rep("- **Verdict:** DEFECT-CONFIRMED against \"a surface may never partially cover another live control\". **Grade:** E1, fresh captures.",
    "- **Verdict:** DEFECT-CONFIRMED against `CLAUDE.md` \"Chrome reports, it does not instruct\": a status verdict nobody can read is not reported. (Round 2: \"a surface may never partially cover another live control\" does not apply; the status pill is not a live control and the brush column is not an anchored surface.) **Grade:** E1, fresh captures.")
rep("- **Fix:** in Sculpt, bound the pill's width between the brush column's trailing edge and the rail's leading edge, or offset the brush column below the pill.",
    "- **Tests pass over this state:** `uilr120` asserts brush top equals rail top (`EditorWorkspaceLegibilityTest.java:~913-916`) and the UI3D-11 `VISIBILITY.tsv` rows PASS; neither measures occlusion (F-09).\n- **Fix:** in Sculpt, bound the pill's width between the brush column's trailing edge and the rail's leading edge. Offsetting the brush column below the pill would break `uilr120`'s asserted top alignment, so that variant is an OWNER layout decision, not a recommended fix.")
# F-07
rep("### F-07 — P3 — Selected state and verdicts do not reach a screen reader\n\n- **Verdict:** PARTIAL (gap). **Grade:** E2.",
    "### F-07 — no severity — Selected state and verdicts do not reach a screen reader\n\n- **Verdict:** BLOCKED-OWNER (recommendation), horizon H2 (corrected in Round 2 from H1 / PARTIAL / P3). No repo rule requires TalkBack selected state or live regions; the only accessibility rules are content descriptions and SCULPT-H1's \"shape + accessible label\". Owner decision 6. **Grade:** E2 for the code facts.")
# F-09
rep("- **Impact:** this is how F-01 shipped while the visual-evidence runs that measured it passed.",
    "- **Impact:** this is how F-01 shipped while the visual-evidence runs that measured it passed.\n- **Further coverage gaps in the same class (Round 2):**\n  - the navigator tests assert state bits under a name that promises a camera change (F-02);\n  - `uilr120` and the visibility ledger pass over the occluded Sculpt status line (F-06);\n  - only `uilr106` drives System Back through the platform route, and only for Add Primitive in Construction; no other surface or phase is tested through it (the gap behind F-03 and F-04);\n  - Freeform, Surface and History have no committed screenshot set (former UI-087, moved here as an evidence gap).\n- **Synthesis:** merge with Seat I's I-08 (`performClick` and viewport-direct taps cannot see occlusion) into one finding.")
# H-F3
rep("""- **H-F3.**
  - **Claim:** System Back in an in-project sketch with nothing open backgrounds the app rather than cancelling the sketch.
  - **Basis:** `hasDismissibleSurface` is false. This is source-derived; the intended behaviour is undocumented, so it is not a defect (UI-083).""",
"""- **H-F3 (promoted to a DOC-DIVERGENCE in Round 2, UI-083, E2, P3).**
  - **Fact (E2):** System Back in an in-project sketch with nothing open backgrounds the app, because `hasDismissibleSurface` is false.
  - **Divergence:** `CLAUDE.md` APP-H1 ("Back is one step in every phase and the platform's own at Home") and `PRODUCT.md:196` ("System Back always goes one step") imply Back away from Home is not the platform's; `uilr106` asserts that a resting Construction workspace hands Back to the platform (`EditorWorkspaceLegibilityTest.java:324-328`). Owner decision 1.""")
# positive claims
rep("| Back closes the topmost anchored surface and the app stays, through the real platform key route | E1: `uilr106` sends `KEYCODE_BACK` on API 36 (fresh PASS); E3 `HomeFlowTest.e2eAppH1_11` |",
    "| Back closes Add Primitive in a Construction project and the app stays, through the real platform key route, on API 36 | E1: `uilr106` sends `KEYCODE_BACK` (`EditorWorkspaceLegibilityTest.java:347`, fresh PASS). No other surface or phase is tested through the platform route (F-09). |")
rep("| One primary surface at a time; Display closes the inspector; the visibility ledger is clean | E1: `Ui3dStateAuditTest` `VISIBILITY.tsv` 119/119 PASS |",
    "| One primary surface at a time; Display closes the inspector | E1: `uilr117_primarySurfaceExclusivityIsDeterministic` (fresh PASS). Corroboration, not a gate: `VISIBILITY.tsv` 119/119 PASS is recorded rather than asserted (`Ui3dStateAuditTest.java:40-46`). |")
rep("| World-anchored chrome tracks its projection to within 0.09–0.44 dp; an off-screen value is hidden, not guessed | E1: `SPATIAL_ATTACHMENTS.tsv` (68 PASS, 3 CLAMPED, 1 NOT_MEASURED with reason) |",
    "| World-anchored chrome tracks its projection within 0.00–0.24 dp on the 68 PASS rows; an off-screen sketch/CAD value is hidden, not guessed. Body dimension labels are CLAMPED instead (UI3D-04 `after_move` 0.44 dp; UI3D-06 65.96 dp and 5.21 dp). | E1 recorded measurement, not a gating assertion: `SPATIAL_ATTACHMENTS.tsv` (68 PASS, 3 CLAMPED, 1 NOT_MEASURED with reason); `Ui3dStateAuditTest.java:40-46` \"records rather than asserts\" |")
rep("| HUD touch targets stay ≥ 48 dp independent of the scale band | E1: JVM `CadHudPresentationTest` |",
    "| HUD touch targets stay ≥ 48 dp independent of the scale band | Policy E1: JVM `CadHudPresentationTest` (`:72-81`, `hitPx(density) = round(48·density)`); wiring E2: `CadExtrudeCanvasView.java:251`. No measured HUD view rect in the fresh run. |")
rep("| Debug key hooks and touch attribution are absent from release | E1: `RELEASE_SELFTEST_GUARD=PASS` (fresh) |",
    "| Native debug tokens are absent from the release `.so`; the HUD touch attribution is gated to debuggable builds; release key hooks reach a native no-op (UI-084, PASS-SOURCE-AND-RECORDED-RUNTIME) | E1, provenance incomplete: `RELEASE_SELFTEST_GUARD=PASS` covers only native strings (`scripts/ci-release-selftest-guard.sh:46`, includes `FORGESHAPE_SKETCH_TAP:`, not `FORGESHAPE_CAD_HUD_TOUCH:`). E2: Java `FLAG_DEBUGGABLE` gate (`CadExtrudeCanvasView.java:249-250, 537-542`); the Java `onKeyDown` hooks remain in release (`ForgeShapeActivity.java:402-430`) and `debugMeshCommand` returns `JNI_FALSE` under `NDEBUG` (`forgeshape_jni.cpp:2267-2270`). No release build was executed. |")
rep("1. What System Back means inside a project sketch with nothing open, and whether Back in the bootstrap with drawn geometry should ask before discarding (F-03, H-F3, UI-083).",
    "1. What System Back means inside a project sketch with nothing open (UI-083, a DOC-DIVERGENCE between APP-H1 / `PRODUCT.md:196` and `uilr106`); the panel-open bootstrap Back order (F-03: `PRODUCT.md:196-198` vs `ForgeShapeActivity.java:68-72`); and whether Back in the bootstrap with drawn geometry should ask before discarding.")
rep("6. Whether TalkBack support is a product requirement, with a specification (F-07).",
    "6. Whether TalkBack support is a product requirement, with a specification (F-07, UI-073, UI-074; H2 / BLOCKED-OWNER until decided).")
open(p,"w").write(t)
