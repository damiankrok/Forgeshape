# FORGESHAPE MASTER PLAN — ADDENDUM v4.1.64

**Data:** 2026-10-28. Jest to kontrolowany **dodatek** do kanonicznego Master Planu v4.1.61 oraz addendów 4.1.62–4.1.63, a nie zamiennik pełnego dokumentu. W obecnym runtime nie ma pełnego pliku v4.1.61; nie wolno rekonstruować nieznanych sekcji i OWNER decisions z pamięci. Po dostarczeniu kanonu: scalić, zachowując istniejącą numerację i nie skracając historii.

## Delta do §1 CURRENT EXECUTION — CAD-R2-C1 TECH PASS

Aktualny etap zakończony: `CAD-R2-C1-POST-IMPLEMENTATION-QUALITY-GATE`, token `PASS-CAD-R2-C1-WITH-RECORDED-BLOCKERS`. Branch `feature/cad-r2-c1-quality-gate-r1@08406cfbbff471f6e53c576c24b45791f903301c`; base `ea24d20805d9487d26221d9dd45e47dc16fb6ecb`, GitHub 10 ahead/0 behind. Ostatni produkt `cf340f04646da0b89f0ff53d6cd996316ac3dba9`; po nim tylko docs do HEAD. `main 6c9f156` nienaruszony, brak merge i FullSharded.

**Produkt C1:** `SupportChooser` nie pozwala promieniowi przeniknąć zakrzywionej lub innej niekwalifikowalnej powierzchni i wybrać płaszczyzny świata ZA nią; zachowuje płaszczyznę w PRZODZIE i poprawny `TopoRef` kwalifikowalnej ściany. W History chip `Edit Sketch` z viewportu jest wycofany, więc przy font scale 1.5 nie przechwytuje akcji History. Bez zmiany formatu, korpusu, runtime biblioteki i renderera.

**Dowód:** host 4375/0 z repo (nie wykonany niezależnie); 17 oracles wg repo; JVM 196/196 (20 XML z oryginalnego ZIP), DEVICE 67/67 + 55/55 (dwa osobne ZIP-y, start 25/25 clean; w drugim clean na trzecim przechwyceniu), corpus 72/72 byte-identical, release guard 0/0 na obu release ABI, CI FAST 40766013297 success. APK 17 552 535 B, SHA-256 `fd7a11a9a7b1bbf7748b46e2d71c71a2a830b54f59e8bd7f9011113d968b1dd5` samodzielnie zweryfikowany z CI artifact 12849379257. **Status: TECH PASS / OWNER PHYSICAL REVIEW OPEN / NOT MERGED.**

## Disposition: performance A

Host gap `eval_all` +32.5% / `drafting_trim100` +38.2% **reprodukowalny przy danym layoucie**. Protokół ABBA i izolowany harness pokazują silną zależność JCC/32-byte branch boundary dla host x86; product-only link A/B +1.3% / +1.0%; mitigated A/B −1.1% / −1.6% (nieistotne). Dlatego **brak potwierdzonej regresji spowodowanej CAD-R2**, a nie ogólne „performance PASS everywhere”. `ARM64 real-device timing = OPEN`, host code-layout noise dokumentować i kontrolować w dalszych benchmarkach. Nie dodawać domyślnie flag kompilatora x86 do APK ARM64.

## OPEN OWNER DECISION — A2

`OQ-CAD-R2-WORLD-PLANE-RETAINED-SKETCH`: brak decyzji. Opcja 0 = utrzymanie aktualnej odmowy (stan domyślny); opcja 1 = dodatkowe body-local root sketches, nowy `CADB v9`; opcja 2 = authored construction planes, nowy model+format; opcja 3 = project-level sketches, najszersze konsekwencje. Wybór nie wynika z wcześniejszej prowizorycznej akceptacji APK ani z technicznego PASS. **Nie implementować A2 bez jawnego wariantu OWNER i osobnego zakresu.**

## Następny pojedynczy etap — CAD-R2-C2

`CAD-R2-C2-PICKING-SEAM-CORRECTNESS-R1`, proponowany scoped bugfix preflight i wykonanie. **Cel:** ustalić i, jeśli w pełni odtworzone, usunąć odrzucenie CPU ray–triangle przy wspólnej krawędzi dwóch trójkątów widocznej płaszczyzny (dokument `HIT_OWNERSHIP.md`, far ortho eye ~250 m). Sprawdzić przez testy host i realne touch instrumented, że nie dopuści to do retargetu na światową płaszczyznę z tyłu ani do fałszywych trafień na zewnątrz obiektu. Minimalna zmiana `forgeshape_picking.*` / consumer tylko jeśli diagnostyka wymusi; zachować semantic face identity, hit-nearest and back-face policy, renderer geometry source, no hidden state.

**Gates:** przed kodem BEFORE + failing focused host reproducer na exakt base; native full 25 suites, existing 17 oracles, JVM, Android build, release selftest guard, corpus 72/72, DEVICE affected + union under time cap, FAST, identyczny product-tree po testach, artifact/APK hash. Jeśli nie da się wykazać przyczyny lub poprawka jest ryzykowna — `BLOCKED-NO-SAFE-PICKING-FIX` z dowodami, bez „mock success”. Fizyczny OWNER review obowiązkowy dla każdego nowego APK; brak merge/FullSharded.

**Not authorised:** drugi root, CADB v9, solver, feature reorder, Revolve Add/Cut, STEP/IGES, Fillet/Chamfer/Shell, rewrite GPU, new B-rep dependency, AI/MCP, copying SolidCraft, integration merges. Wszelkie niezidentyfikowane wymagania historii pozostają niezatwierdzone.

## Źródła

- https://github.com/damiankrok/Forgeshape/tree/08406cfbbff471f6e53c576c24b45791f903301c/artifacts/cad-r2-c1-quality-gate
- CI DEVICE: 40752389411 / 40760227555; FAST: 40766013297
- `FORGESHAPE_PROJECT_STATE_v4.1.64.md` — bieżący szybki odczyt, szczegóły refów i evidence.
