# FORGESHAPE MASTER PLAN — ADDENDUM v4.1.65

**Data:** 2026-10-29. Kontrolowany dodatek do pełnego Master Planu v4.1.61 i addendów v4.1.62–64; pełny dokument v4.1.61 nie jest zamontowany, dlatego NIE rekonstruować ani nie nadpisywać kanonu. Hierarchię „CURRENT EXECUTION” zachować przy scalaniu.

## Delta do CURRENT EXECUTION

- Zamknięto technicznie `CAD-R2-C2-PICKING-SEAM-CORRECTNESS-R1` z tokenem Claude `PASS-CAD-R2-C2-PICKING-SEAM`, po niezależnym sprawdzeniu GitHub refs/compare, CI/DEVICE artifacts, JVM XML, sha256 ZIP+APK i inspekcji kodu.
- Gałąź `feature/cad-r2-c2-picking-seam-r1@70a6c14f703bfda2ab689b855126c1d12a541e41`, od C1 `08406cfbbff471f6e53c576c24b45791f903301c`: 12 ahead/0 behind. Produkt jedna poprawka C++ `0fa12c9`, drzewo `app/src/main` stałe po `f3db953`. `main@6c9f156` niezmieniony. Bez merge, FullSharded, nowych dependencies lub `.forge` version.
- Usunięto reprodukowalny fałszywy miss na wspólnej przekątnej trójkątów przy distant-ortho 250 m: dotychczasowy Möller–Trumbore `intersectRayTriangle` wykonuje obliczenia pośrednie w double przy zachowaniu publicznego float API, epsilon, facing i mapowania semantycznych TopoRef. „BEFORE” pokazuje `hit=0` mimo barycentric wewnątrz w double; 8 czerwonych kontroli PICK_C2 poprzedza fix. To nie jest retarget przez sąsiednią ścianę.
- Zewnętrznie zweryfikowane: DEVICE 59/59 run `40776968165` + 67/67 run `40782861799`, startup 25/25, 0 FAIL; JVM 196/196 z 20 XML; corpus 72/72 byte-identical; release self-test guard PASS (x86_64/arm64 zero symbols/strings); FAST `40787465884` success. Host `4392/0`, 17 PICK_C2 i 17 oracles oraz perf per-triangle +2.4–3.0% — wg dokumentów/evidence repo, bez osobnego uruchomienia przez koordynatora.
- Verified ZIP SHA256: DEVICE 12856463065 `5c0f6ba9143028ecc088f7673f409ef86e709834e8b5f366470a9753bc1e8d60`; DEVICE 12860315938 `541234f69a7057e115098a0e52e6500fae83346b80b679c1fd96c5d9fd6affab`; FAST 12860436405 `e5f659df3436d6d4fa2339cbb84f21770ec7fe61c20df6aa7c10ad6dc01d2e1f` — wszystkie zgodne z GitHub digest.
- Exact APK z oryginalnego CI ZIP: `app-debug.apk`, 17 582 551 B, SHA256 `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`; expires 2026-11-11T23:31:32Z. To DEBUG APK, nie signed release AAB.

**Obowiązujący status:** `TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED`. OWNER testuje dokładny APK według pakietu ze STATE v4.1.65, zgłasza PASS/FAIL/NOT TESTED i ewentualny dobrowolny wybór A2. Nie wykonywać teraz kolejnego feature stage ani merge; **brak active coding prompt**. Nie traktować zielonego emulatora jako fizycznego APPROVAL.

## OPEN A2 decision

Brak zgody na drugą root sketch/body-local world sketch oraz `CADB v9`. Opcja 0 hold; 1 body-local additional workplane sketch; 2 body-owned construction plane; 3 project-level sketch. Żaden wariant wybrany; decyzja później, podmiotem jest OWNER. Zachować zgodność starszych readerów i nie ruszać codec bez osobnego projektu.

## Pozostałe ryzyka

- Fizyczny ARM64, GPU, stylus/hover i odczuwalny koszt double precision dla gęstej Sculpt mesh: NOT TESTED.
- Picking dokładnie na granicy dwóch różnych ścian może wskazać jedną z tych rzeczywiście przecinanych ścian; nie jest to twarda referencja do konkretnej powierzchni, dopóki nie nastąpi jej semantyczna kwalifikacja i confirm.
- Host JCC layout variability po C1 nie jest globalnym „perf PASS”, ale też nie jest dowodem regresji R2 produktu.
- Dodatkowy FullSharded, integracja CAD-V6→main, release AAB to oddzielne, nieautoryzowane bramki.

## Dokumentacja i źródła

- `FORGESHAPE_PROJECT_STATE_v4.1.65.md` / `FORGESHAPE_PROJECT_STATE.md` (bieżący stan)
- `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.65.md` (pochodzenie)
- Repo `artifacts/cad-r2-c2-picking-seam/{BEFORE,BEFORE_REPRO,DESIGN,TESTS,PERF,SUMMARY}.md` at `70a6c14f`
- Actions run IDs `40776968165`, `40782861799`, `40787465884`.
