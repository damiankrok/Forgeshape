# IMPORT-01B — the `.forge` data-contract decision

## The decision

**Keep `major`/`minor` at 1.0. Add no section and no header flag. Generalize
which bodies a `SCUL` entry may sit over, from `CONS`-only to either source.**

That is the whole change to the format, and it is one deleted refusal in
`validateProjectDocument`.

## What was refused before, and why lifting it is safe

`IMPORT-01A` refused a `SCUL` entry whose body was claimed by `IMPT`:

```cpp
if (covered[found] == 2) {
    return ProjectCodecStatus::UnresolvedReference;
}
```

That refusal was right at the time and for the stated reason: an imported object
could not be sculpted, so a file claiming a sculpt mesh for one described a
project the build could not evaluate, and dropping either half would have lost
it. `IMPORT-01B` makes such a file describe something the build evaluates
exactly.

The exactly-one-of rule is **untouched**. It is about `CONS` versus `IMPT` — the
two things a body's geometry can come FROM — and `SCUL` is not one of them. A
`SCUL` entry is a second REPRESENTATION of a body that already has a source, not
a third source. Which source it was frozen from is deliberately not stored,
because nothing reads it back: a Frozen Sculpt Mesh is its own positions and its
own topology whatever produced it.

## The four valid combinations

| combination | valid | fixture |
| --- | --- | --- |
| `SCNE + CONS` | yes | `construction_multibody_v1.forge` |
| `SCNE + CONS + SCUL` | yes | `sculpt_mixed_v1.forge` |
| `SCNE + IMPT` | yes | `imported_only_v1.forge` |
| `SCNE + IMPT + SCUL` | **yes, new** | `imported_sculpt_v1.forge` |
| a body in both `CONS` and `IMPT` | refused | `UnresolvedReference` |
| a `SCUL` entry `SCNE` does not carry | refused | `UnresolvedReference` |
| a Sculpt project whose active body has no `SCUL` | refused | `UnresolvedReference` |

A body named by NEITHER source branch stays legal at the CODEC level and is
refused at the RUNTIME level by `runtimeCanEvaluateProject`, exactly as before:
an optional section at a version this reader cannot read is skipped by contract,
which is a statement about the file, and "this build cannot build a body it has
no geometry for" is a different statement about the runtime.

## Why no version bump

Compatibility here can only ever be encountered **fail-closed**.

A build from before `IMPORT-01B` reading an `IMPT`+`SCUL` file hits the refusal
quoted above and returns `UnresolvedReference`. It does not open the project with
the sculpt mesh missing, and it does not open it with the imported body missing:
it refuses the file outright, with a name. An older reader cannot MISUNDERSTAND
such a file — only decline it.

That is the same shape `kHeaderFlagHasImported` already gives a pre-`IMPORT-01A`
reader, and it is the property a version bump exists to guarantee. Bumping the
minor would have added a number without adding a guarantee, and bumping the major
would have made every existing file unreadable to satisfy a rule that was already
satisfied.

In the other direction nothing changed at all: **all ten pre-`IMPORT-01B`
fixtures' digests are byte-identical**, which `FSR1A-12` and `IMP01A-19` assert
and `scripts/build-forge-corpus.ps1 -VerifyOnly` confirms independently.

## What the writer does

`captureProjectDocument` already wrote a `SCUL` entry for any body with a frozen
mesh, without asking about representation — the branch that decides `CONS` versus
`IMPT` is separate from and above the one that appends `SCUL`. So the writer
needed **no change**, and neither did `loadProjectDocument`, whose
`findSculptBody` was always keyed on `ObjectId` alone.

The only production edit in the codec is the removed refusal. Everything else in
this stage's `.forge` work is fixtures and tests.

## The fingerprint

`projectSemanticFingerprint` already mixed each body's `FrozenSculpt` state —
revision, freeze count, the two counts, `renderBothSides`, `hasEdits` — for every
body regardless of representation, so an imported body's sculpt work moves it and
autosave protects that work. Asserted by
`IMP01B_11_the_fingerprint_sees_a_stroke_on_an_imported_body`.

## Blocked-data-contract check

`BLOCKED-DATA-CONTRACT` was NOT reached. The live repository's rules —
`DATA_PACKAGE_SPEC.md` §7, §7a and §10, and `CLAUDE.md`'s `.forge` rule —
authorize exactly this shape of change: a rule may be widened within a major when
an older reader refuses rather than misreads, and `IMPT` is the worked example
the spec already records for that pattern.
