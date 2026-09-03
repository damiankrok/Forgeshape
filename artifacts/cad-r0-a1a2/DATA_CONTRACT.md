# Data contract - the decision

**A new required section, `CADB`, on `IMPT`'s terms; no version bump.**

## Why not extend `CONS`

`CONS` is the Construction feature graph: a fixed 114-byte record of the active
primitive kind and all six remembered parameter sets, plus a feature list of
`(localFeatureId, kindCode)` pairs with no payload. A CAD Body has no primitive
kind and no parameter sets, and a sketch is variable-length. Carrying it in
`CONS` would mean either fabricating a primitive (which `IMPORT-01A`'s rule
forbids: nothing invents a primitive a body was never made from) or bumping the
`CONS` version to add a variable-length payload behind a discriminator - exactly
the shape section 12 of `DATA_PACKAGE_SPEC.md` says a new representation must
NOT take.

## Why a section, and why required

Section 12: "A new representation - a third way for a body to have geometry -
takes its own section on `IMPT`'s terms and joins the exactly-one-of rule." So
`CADB` is always required, announced by header bit3 `hasCADB`, exclusive with
`CONS` and `IMPT` per body, and an older reader refuses a file that carries one
(`BadHeader` on the flag, `UnknownRequiredSection` past it) rather than opening
the project with objects missing.

## What the record holds

The authored truth and nothing derived: workplane code, `nextEntityId`,
`profileEntityId`, direction code, depth, and every entity with its id, kind
code and its own values. See `DATA_PACKAGE_SPEC.md` section 7b for the exact
layout. No polygon, no triangle, no vertex - all regenerated on load.

## Validation

The codec calls the domain's own `validateCadBodyState`, which includes
`extractClosedProfiles`: a file whose sketch does not close the profile its
extrusion names is refused (`InvalidSemanticValue`). A `SCUL` entry over a
`CADB` body is refused (`UnresolvedReference`) because CAD -> Sculpt is not this
stage. A body claimed by two of `CONS`/`IMPT`/`CADB` is refused.

## Compatibility

- Old files decode unchanged: all twelve legacy fixtures are byte-for-byte
  what they were (`FSR1A-12`, `IMP01A-19`, `IMP01B-11/12` still assert the
  same digests; the corpus builder's `OnDisk = True` for every one).
- New files with a CAD Body are refused by older builds, fail-closed.
- Autosave, recovery, Save Copy, Open File and the fingerprint all carry the
  branch: `captureProjectDocument` writes it, `loadProjectDocument` rebuilds
  it, `projectSemanticFingerprint` hashes every authored value (the sketch is
  bounded, so this is the values themselves and not a proxy).

## Independent implementation

`scripts/build-forge-corpus.ps1` gained `New-CadPayload` and four fixtures,
written from the specification, and agrees with the C++ encoder byte for byte
on all four (identical SHA-256; see `CORPUS.md`).
