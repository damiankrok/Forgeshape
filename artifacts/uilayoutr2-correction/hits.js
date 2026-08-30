// Separates intrinsic hit geometry from a clipped visible intersection.
//
// uiautomator reports the ON-SCREEN INTERSECTION of a node, so a control its
// container is part-way through scrolling comes back short. That is not an
// intrinsic-size failure and must never be counted as one. The 48 dp floor is a
// statement about the control's own box.
//
// The box is recovered in two steps, and which step answered is recorded:
//
//   CELL   the largest rectangle the same control reported in the same cell --
//          the state where nothing clipped it. This is the honest per-cell
//          figure and is used whenever it clears the floor.
//   RUN    used only when the harness never caught the control unclipped in that
//          cell at all, which happens where the harness scrolled a container to
//          reach a later control and left it scrolled. The same control's
//          largest box anywhere in the run is then the reference. Widths that
//          legitimately differ per cell (a panel field is wider in a wide
//          window) make this a FLOOR argument, not a per-cell measurement, and
//          the row says so.
//
// The unclipped, in-process authority for the floor is the instrumented case
// UILR2C-11, which reads each control's laid-out box directly rather than its
// on-screen intersection.
const FLOOR_DP = 48;

function resolve(hits) {
  const perCell = {}, perRun = {};
  hits.forEach((r) => {
    const c = r.cell + '/' + r.id;
    perCell[c] = perCell[c] || { w: 0, h: 0 };
    perCell[c].w = Math.max(perCell[c].w, r.visibleW);
    perCell[c].h = Math.max(perCell[c].h, r.visibleH);
    perRun[r.id] = perRun[r.id] || { w: 0, h: 0 };
    perRun[r.id].w = Math.max(perRun[r.id].w, r.visibleW);
    perRun[r.id].h = Math.max(perRun[r.id].h, r.visibleH);
  });
  hits.forEach((r) => {
    const cell = perCell[r.cell + '/' + r.id], run = perRun[r.id];
    const cellClears = cell.w >= FLOOR_DP && cell.h >= FLOOR_DP;
    const box = cellClears ? cell : run;
    r.intrinsicW = box.w;
    r.intrinsicH = box.h;
    r.intrinsicFrom = cellClears ? 'CELL' : 'RUN';
    r.clipped = (r.visibleW < box.w - 0.01 || r.visibleH < box.h - 0.01) ? 'CLIPPED' : '';
    r.pass = (box.w >= FLOOR_DP && box.h >= FLOOR_DP) ? 'PASS' : 'FAIL';
  });
  return hits;
}

module.exports = { resolve, FLOOR_DP };
