# F-18 — dead reserved-chip styling

## What the audit named, and what was actually there

The audit named `EditorControlStyles.setChipReserved` and
`res/drawable/bg_capsule_reserved.xml`. Reading the method showed it referenced
a DIFFERENT drawable, `bg_control_reserved` — so there were two reserved
drawables, one referenced only by the dead method and one referenced by
nothing.

## Proof of no caller / no reference (before removal)

```
grep -rn "setChipReserved|control_reserved|capsule_reserved" app/src
  EditorControlStyles.java:459   javadoc {@link #setChipReserved} in setChipEnabled's comment
  EditorControlStyles.java:473   the definition
  EditorControlStyles.java:476   R.drawable.bg_control_reserved (inside the definition)
  bg_capsule_reserved.xml:6      its own comment mentioning bg_control_reserved
```

* No Java call site, no layout XML, no style, no `res/values` reference.
* `bg_capsule_reserved.xml` was referenced by nothing at all.
* No accessibility or state path: `setChipReserved` set a content description
  on the chip it styled; with no caller, no view ever received it. State lists
  (`res/color/control_content_tint.xml`) use `?attr/fsTextDisabled`, which is
  a theme attribute and stays (it is still used there).
* `@dimen/radius_control_inset` (used by `bg_capsule_reserved`) is still used by
  `bg_capsule_control`, `bg_capsule_control_active` and `bg_capsule_primary`;
  it stays.

## Removed

* `EditorControlStyles.setChipReserved(TextView, CharSequence)`.
* The sentence in `setChipEnabled`'s javadoc that contrasted it with
  `setChipReserved` (rewritten to stand on its own).
* `app/src/main/res/drawable/bg_control_reserved.xml`.
* `app/src/main/res/drawable/bg_capsule_reserved.xml`.

Nothing else in chip or palette styling changed.

## Checks

* `grep -rn "control_reserved|capsule_reserved|ChipReserved" app/src` → none.
* Resource merge + link + Java compile: debug and release builds succeed
  (`TEST_RESULTS.md`, `BUILD_SYMBOLS_SIZE.md`).
* `PROJECT_STATUS.md` no longer lists the pair as live debt.
