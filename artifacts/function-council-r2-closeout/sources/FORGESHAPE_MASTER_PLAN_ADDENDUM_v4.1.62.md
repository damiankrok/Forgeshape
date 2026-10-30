# FORGESHAPE MASTER PLAN — ADDENDUM v4.1.62 / CAD-R2

**Do dopięcia do pełnego, kanonicznego `FORGESHAPE_MASTER_PLAN_v4.1.61.md`, a NIE do użycia zamiast niego.** 2026-10-28.

## A. Nowa dyspozycja OWNER (supersedes only work-start hold)

OWNER prowizorycznie przyjmuje ostatni debug APK jako wystarczający do rozpoczęcia dalszej pracy nad CAD i autoryzuje duży, wielofazowy prompt Claude Code inspirowany **techniką** SolidCraft. Nie zatwierdza przez to finalnego pełnego OWNER physical checklist, merge, pierwszej premiery sklepowej, nowego kernela CAD ani nie ogranicza przyjętego MVP.

Dotychczasową zasadę „nie rozpoczynaj S3 przed OWNER physical review” zastępuje w ograniczonym zakresie: **można teraz rozpocząć CAD-R2 na osobnej gałęzi z korekty**. Nie wolno integrować wyników przed końcowym OWNER gate.

## B. Nowa aktywna implementacja

- ID: `CAD-R2-RETAINED-SKETCH-PARAMETRIC-UX-AND-GEOMETRY-HARDENING`.
- Agent: Claude Code ULTRACODE.
- Task branch z `feature/modeling-r1-owner-shell-sketch-fix-r1@636797e`, planowany `feature/cad-r2-retained-sketch-parametric-ux-r1`.
- Prompt: `PROMPT_CAD_R2_ULTRACODE_RETAINED_SKETCH_PARAMETRIC_QUALITY.md`.
- Motyw przewodni: dopracowanie już dostarczonego CAD, szczególnie retained sketch workflow i parametric edit, poprawność regionów i dokładny support oraz testy oraklowe.
- 5 faz: Phase 0 preflight/design; Phase 1 retained sketches; Phase 2 feature edits/history; Phase 3 geometry selection/support; Phase 4 oracle+reliability/quality. Każda faza ma własne testy, a final kończy się DEVICE i FAST z exact APK.
- Nie powoduje zmiany `.forge` layout ani kodeków, nie rozszerza automatycznie listy operacji poza istniejący model. Gdy nie da się poprawnie zrealizować podfunkcji bez niezatwierdzonego schematu/warstwy — jawny design block.

## C. Kryteria releasability i testów

Gate przed final PASS: existing 25 native suites + nowe tests, JVM full, ≥72 legacy corpus byte-parity, release guard, bounded DEVICE journeys prawdziwymi dotykami, CI FAST na tym samym final candidate, signed APK SHA. Debug `app-debug.apk` służy testom OWNER, nie jest produkcyjnym AAB. FullSharded i merge wyłącznie osobna decyzja integracyjna.

## D. SolidCraft research policy

Reference only: SolidCraft `tangency-labs/solidcraft@504d7c948860444ab4968dab62033f34f4f9e6c2`. Inspiracje: derivative geometry vs authored CAD truth, semantic anchors, stable commands, deterministic/oracle tests, atomic write/recovery. Zakaz kopiowania modułów, ikon, tekstów, UI lub przebudowy C++/Manifold na Rust/truck/SolidCore. Priorytet: mierzalny efekt w ForgeShape.

## E. Out of scope i otwarte kwestie

Brak samoczynnego uruchomienia Fillet/Chamfer/Shell/Hole/STEP/constraint solver/AI/MCP/Revolve Add-Cut/feature reorder/NURBS. Nie rozstrzyga `OQ-B` (pełny MVP vs tiering), release readiness ani fizycznego OWNER aesthetic acceptance. Historia S3 zachowana: to dopracowanie retained-sketch, a nie zamiana modelu projektu.

## F. Pochodzenie

Zweryfikowano aktualne GitHub HEAD refs 2026-10-28; fizyczny APK ma SHA-256 `b557827d56c6979cf46c3451894b38717d6c882faa7610cfbc74f088113411cb`. Pełny plik kanonicznego Master Planu v4.1.61 nie był dostępny lokalnie w trakcie tej aktualizacji — addendum nie jest próbą jego odtworzenia. Zachować w nim wszystkie oryginalne OWNER decisions, changelog, roadmap i source metadata.
