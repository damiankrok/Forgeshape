# F-15 — `buildSpherifiedBox` internal linkage

## Before

`app/src/main/cpp/forgeshape_mesh_fixtures.cpp` closed its anonymous namespace
at line 33 and defined `FixtureMesh buildSpherifiedBox(uint32_t, float)` at
line 49 in namespace `forgeshape` with no declaration in
`forgeshape_mesh_fixtures.h` — external linkage by accident.

## Consumers (verified before the change)

```
grep -rn "buildSpherifiedBox" app/src
  forgeshape_mesh_fixtures.cpp:49   definition
  forgeshape_mesh_fixtures.cpp:115  buildFixtureLarge()
  forgeshape_mesh_fixtures.cpp:124  buildStressMesh()
```

No header declares it and no other translation unit names it.

## Change

The definition (and only the definition) is now wrapped in its own anonymous
namespace block (`namespace { ... }  // namespace`, lines 45–117), which is the
file's existing convention for its private helpers (`baselineCopy`, `deform`).
`buildFixtureBaseline`, `buildFixtureDeformed`, `buildFixtureLarge` and
`buildStressMesh` keep external linkage as declared in the header. No behaviour
change; no duplicate implementation.

## Proof

* Both ABIs, debug and release, link (`BUILD_SYMBOLS_SIZE.md`); the linker's
  `--no-undefined` would have failed on a symbol that had been required.
* `llvm-readelf --dyn-syms` on the built libraries: `buildSpherifiedBox`
  absent from the dynamic symbol table (it was never exported through
  `.dynsym` before either, because the library uses default visibility only for
  JNI entry points and the self-test runners — the finding was about linkage
  hygiene, not an exported symbol).
