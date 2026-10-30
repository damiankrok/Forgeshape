# CAD-FACE-SUPPORTED-PLACEMENT-R1 — OWNER later test pack (physical device)

Emulator CI closes no physical gate (CLAUDE.md). On a real phone or tablet,
with a finger and then a stylus:

1. **Build a producer and a dependent.**
   - New Project → CAD; draw a 2 × 2 rectangle and extrude it 2.
   - New Sketch; tap the top face twice; draw a 1 × 1 rectangle; extrude 0.5.
   - Expect a second body standing on the top face.
2. **No Transform for the dependent.**
   - With the dependent selected, the Tool Rail shows Shape only.
   - No move, rotate or scale handles are drawn.
   - Dragging a finger or the stylus over the dependent turns the view and
     never moves the body.
   - The Undo button does not become available.
3. **Transform held, then the dependent selected.**
   - Select the producer and choose Transform; handles appear.
   - Tap the dependent: the rail switches to Shape and the handles go.
4. **Producer move carries it.**
   - Select the producer; Transform; drag the X handle about 1 m.
   - The dependent follows. One Undo returns both.
5. **Export.**
   - Export GLB; open the file in Blender or Windows 3D Viewer.
   - The dependent stands on the producer's top face, also after step 4's
     move, and also on a side face if you repeat step 1 on a side.
6. **Save and reopen.** Save, close the project and reopen it; repeat steps 2
   and 5.
7. **Large font and rotation.**
   - Repeat steps 2–3 at system font size Largest and in landscape.
   - The rail never offers Transform for the dependent.

Record the device model, Android version, input (finger / stylus) and a
pass/fail per step. Report stylus hover and comfort separately; this stage
claims neither.
