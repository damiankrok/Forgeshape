# FUNCTION-COUNCIL-R2 — diagnostic sources (audit-only, never built into the product)

Copied from the seats' scratch directories `/tmp/claude-0/fc2-scratch/<seat>/`
so the E1 reproductions cited in `SEAT_*.md` stay reproducible after the
container is gone. Only sources, scripts and their recorded outputs are kept
(binaries and object files are not). Each `*.cpp` is a host program compiled
against `app/src/main/cpp` the way `scripts/host-native-selftests.sh`
assembles the host build; the exact compile command for each one is in the
seat file that cites it. `synth-trace/merge.py` and `totals.py` regenerate
`requirements_traceability.tsv` and its totals from `work/req_*.tsv`.
Nothing here is referenced by Gradle, CMake or CI.
