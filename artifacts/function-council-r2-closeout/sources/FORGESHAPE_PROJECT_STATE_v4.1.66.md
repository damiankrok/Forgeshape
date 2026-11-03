# FORGESHAPE_PROJECT_STATE — v4.1.66

**Data:** 2026-10-29 (Europe/Berlin). **Typ:** decyzja OWNER o kolejnym zadaniu AUDYTOWYM, nie nowy wynik kodu. **Stan bazowy:** C2 TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED. **Aktywne zlecenie do przekazania Claude Code:** `FUNCTION-COUNCIL-R2-FULL-CAPABILITY-UX-PLATFORM-AUDIT`, status `PROMPT PREPARED / NOT STARTED`; żaden wynik R2 Council nie jest jeszcze potwierdzony.

## 1. Zmiana kierunku pracy

OWNER zamiast ręcznego APK-first testowania żąda pełnego audytu Council funkcji należnych według Master Planu, zgodności kodu i rzeczywistych przepływów, możliwości rozwoju, Android UI/UX i planowanej adaptowalności na Windows/Linux. Audyt ma użyć źródeł, obecnych testów, emulatora i automatycznej weryfikacji, ale nie może udawać fizycznego OWNER PASS ani zastępować testów realnego GPU/stylusa/termiki.

**Jeden prompt:** `/mnt/data/PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md`. **Nowa gałąź AUDIT-ONLY** z aktualnej sprawdzonej C2 `feature/cad-r2-c2-picking-seam-r1@70a6c14f703bfda2ab689b855126c1d12a541e41` (GitHub zweryfikowany 2026-10-29). `main@6c9f156c1df91fe1a55445aed6186105a075ab31` bez zmian. Nie uruchamiać poprzedniego docs-only promptu `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1`, gdyż brak evidence OWNER.

## 2. Cel i kryteria Council

Dziesięć niezależnych seatów: (A) Master/product journeys, (B) Construction/Objects, (C) CAD/Sketch/History, (D) Sculpt/Freeform/Surface, (E) persistence/GLB/lifecycle, (F) Android UI/UX/accessibility, (G) desktop Windows/Linux architectural portability, (H) renderer/picking/performance, (I) tests/CI, (J) adversarial cross-integration. Po ich analizie: requirements traceability H0/H1/H2, pełny feature inventory, >=30 journeys, Android ekran×stan×orientacja/font, PC port matrix z wariantami, stare Council D1–D7 rechecked, root-cause risk list, backlog DEFECT/QUALITY/FUTURE IDEA, oddzielne OWNER decisions i jeden najbliższy etap do późniejszej zgody.

**Podstawa audytu:** repo `CLAUDE.md`, `ARCHITECTURE.md`, `PRODUCT.md`, `PROJECT_STATUS.md` v0.102.0, `DATA_PACKAGE_SPEC.md`, istniejące dossiers CAD-R2/C1/C2 i poprzedni `artifacts/function-council-r1/`. Historyczny Council R1 dotyczy `a1b50f2`, 6 seatów, z czego 4 częściowe; nie wolno uogólniać jego starych wniosków na dzisiejszy produkt. D1 zgłoszono wcześniej jako naprawiony w C1-D1, ale Council R2 musi zbadać go ponownie, a D2–D7 zweryfikować z aktualnym kodem.

**Brak kanonu:** pełny `FORGESHAPE_MASTER_PLAN_v4.1.61.md` i pełne HISTORY v4.1.61 nie są dostępne w bieżącym containerze; dostępne tylko starsze Master i addenda 4.1.62–65. Jeżeli Claude nie znajdzie obecnego pełnego Master/OWNER decisions, audyt ma status `PARTIAL-PLAN-SOURCE-GAP`; nie wolno stwierdzić pełnej zgodności z planem przez zgadywanie.

## 3. Wstrzymane decyzje i zakazy

- **A2 world-plane retained sketches:** żadnej zgody na dodatkowy root, CADB v9 ani project-level szkice. Council może opisać warianty; nie implementować.
- **OWNER physical review C2:** brak pełnego PASS; zostaje oddzielną bramką. Emulator nie zamyka touch-feel/stylus/ARM64 GPU, a brak fizycznego testu nie blokuje dokumentacyjnego Council.
- **Integration:** nie mergować do `main` ani innych gałęzi; nie rebase/cherry-pick/force push, nie uruchamiać FullSharded bez nowej autoryzacji.
- **MVP:** brak rozszerzenia. Brak nowych funkcji, zmian Android/desktop produktu, nowej biblioteki, kernela, schematu `.forge`, testów lub CI przez tę fazę. Powstają wyłącznie dossier i opcjonalnie izolowane dowody diagnostyczne.
- **Nie ma TECH PASS Council:** dopiero raport Claude, kontrola źródeł i testów, potem koordynatorski verdict i nowy prompt.

## 4. Stan techniczny, zachowany

C2 branch HEAD `70a6c14f703bfda2ab689b855126c1d12a541e41`, produkt `0fa12c9`/tree frozen `f3db953`; CI FAST `40787465884` success; DEVICE `40776968165` 59/59 + `40782861799` 67/67 (łącznie 126); JVM 196/196; `.forge` corpus 72/72; release guard PASS, host 4392/4392 **wg repo**. Debug APK SHA256 `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`. Nie ma niezależnego fizycznego wyniku OWNER.

## 5. Dokumenty

Nowy: `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.66.md`, `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.66.md`, prompt Council R2. Starsze addenda i cała poprzednia wersja stanu pozostają poniżej bez zmian. W repo doc `PROJECT_STATUS.md` zostanie dopisany przez Claude po audycie, nie teraz.

---

## ARCHIVED — v4.1.65 preserved verbatim

# FORGESHAPE_PROJECT_STATE — v4.1.65

**Data audytu:** 2026-10-29 (Europe/Berlin). **Repo:** `damiankrok/Forgeshape`. **Źródła:** GitHub refs, compare, Actions run metadata, pobrane i odczytane trzy ZIP-y CI, bezpośrednia inspekcja kodu, repozytoryjny dossier, poprzedni PROJECT_STATE v4.1.64.

**AKTUALNY STAN:** `CAD-R2-C2: TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED`. Etap CAD-R2-C2 jest zamknięty technicznie; nie oznacza to pełnej akceptacji funkcji na telefonie, zgody na integrację ani rozpoczęcia nowego CAD modułu. **Następny aktywny krok: OWNER test fizycznego APK i odrębna decyzja A2. Żaden nowy coding prompt nie jest aktywny.**

## 1. Zweryfikowane fakty GitHub / CI

| Identyfikator | Zweryfikowana wartość |
|---|---|
| `main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` |
| R2 | `ea24d20805d9487d26221d9dd45e47dc16fb6ecb` |
| R2-C1 (baza C2) | `08406cfbbff471f6e53c576c24b45791f903301c` |
| R2-C2 branch | `feature/cad-r2-c2-picking-seam-r1` |
| R2-C2 HEAD | `70a6c14f703bfda2ab689b855126c1d12a541e41` |
| HEAD vs C1 | 12 ahead / 0 behind, 22 zmienione ścieżki (w tym dokumentacja i testy) |
| Zmiana produktu | `0fa12c9` (`app/src/main/cpp/forgeshape_picking.cpp`) |
| Product-tree frozen | `f3db953`; od tego SHA do HEAD brak zmian `app/src/main/` |
| DEVICE B | Actions `40776968165` na `b227a0b`, **59/59**, 0 fail/error/skip; startup 25/25 w kolejności |
| DEVICE A | Actions `40782861799` na `30b561b`, **67/67**, 0 fail/error/skip; startup 25/25 w kolejności |
| DEVICE razem | **126/126** z dwóch rozłącznych pul, ten sam produkt `app/src/main` |
| FAST | Actions `40787465884` na `14a3dbc`, conclusion success |
| JVM | **196/196**, 20 plików XML, 0 failures/errors/skips |
| Corpus | `committed=72 generated=72 mismatches=0`, `FORGE_CORPUS_PARITY=PASS` |
| Release self-test guard | PASS; oba ABI release x86_64 i arm64-v8a: 0 symboli / 0 napisów self-testów; debug kontrola 35/398 |
| Host self-tests | **4392/4392, 25 suite**, wg `artifacts/cad-r2-c2-picking-seam/TESTS.md`, NIE uruchamiane niezależnie przez koordynatora |
| Geometry oracles | 17 PASS, wg repozytoryjnego zapisu; NIE uruchamiane niezależnie |
| Perf | pojedynczy ray/triangle +2.4–3.0%, CAD scene pick ~bez zmian; wynik z repo `PERF.md`, NIEzmierzony niezależnie ani na ARM64 |
| Integracja | brak merge, brak FullSharded |

**Artifact provenance (oryginalne ZIP-y pobrane, SHA niezależnie przeliczone i porównane z GitHub digest):**

- DEVICE B ID `12856463065`, SHA256 `5c0f6ba9143028ecc088f7673f409ef86e709834e8b5f366470a9753bc1e8d60`.
- DEVICE A ID `12860315938`, SHA256 `541234f69a7057e115098a0e52e6500fae83346b80b679c1fd96c5d9fd6affab`.
- FAST ID `12860436405`, SHA256 `e5f659df3436d6d4fa2339cbb84f21770ec7fe61c20df6aa7c10ad6dc01d2e1f`.
- **Dokładny debug APK:** `app/build/outputs/apk/debug/app-debug.apk` w FAST ZIP; rozmiar **17 582 551 B**, SHA256 **`262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`**. Sprawdzony bez rebuildu; kopia `/mnt/data/ForgeShape_CAD_R2_C2_OWNER_14a3dbc.apk`. ZIP expires `2026-11-11T23:31:32Z` (nie jest podpisanym produkcyjnym AAB).

CI URLs:
https://github.com/damiankrok/Forgeshape/actions/runs/40776968165
https://github.com/damiankrok/Forgeshape/actions/runs/40782861799
https://github.com/damiankrok/Forgeshape/actions/runs/40787465884

## 2. Weryfikacja techniczna: co faktycznie naprawiono

`BEFORE_REPRO.md` odtwarza na bazie C1 promień ortograficzny z oka 250 m, przecinający wspólną przekątną dwóch trójkątów dna kieszeni: `pickSceneSnapshot hit=0`; chooser wybiera płaszczyznę świata ZA bryłą. Barycentryczne obliczenie float32 dla jednego trójkąta `u+v=1.00000429` poza, double na tych samych danych `u+v=0.999998093` wewnątrz. Udokumentowany czerwony zestaw ośmiu z 17 kontroli `PICK_C2` przed poprawką.

`forgeshape_picking.cpp` ma nadal Möller–Trumbore i dotychczasowe stałe progowe/culling oraz float API, lecz działa na pośrednich `double` (`D3`, dot/cross). Ścieżka mappingu triangle→`CadFaceRange`→`TopoRef` nie została zmieniona. D19/D20 w DEVICE pokrywają szew, dziurę, krawędź sylwetki, D21 orbitę / confirm, D22 hover **symulowany**. Nie dowodzą fizycznego stylusa ani niezawodności na każdym ABI. Błąd semantycznego przekierowania `nearest face` nie został wprowadzony.

**Ograniczenia pozytywnego werdyktu:** w tym audycie NIE wykonano kompilacji repo ani testów host ponownie. Wyniki HOST/PERF są dowodami dostarczonymi w repo; niezależnie weryfikowano ZIP-y CI, metadane workflow, 20 JVM XML, DEVICE XML i kod. Wyniki perf i host nie są traktowane jako fizyczne benchmarki ARM64. Wybór jednej z dwóch rzeczywiście trafionych ścian na wspólnej krawędzi może zależeć od porządku/tie i ABI; to nie jest nowy trwały token.

## 3. ACTIVE OWNER PHYSICAL TEST GATE — bez automatycznego PASS

Testowany musi być dokładnie APK o SHA256 `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`. Raport OWNER dla każdego punktu powinien zawierać `PASS / FAIL / NOT TESTED` i urządzenie / Android / sposób wejścia (palec/rysik):

1. Orthographic: dwa tapnięcia w środek dna kieszeni i górnej ściany — szkic na zamierzonej ścianie, nie na world plane za bryłą.
2. Orbit, obrót ekranu, zachowana semantyka celu; Cancel nie tworzy Undo ani zmian projektu.
3. Otwór przelotowy: trafienie wewnątrz otworu wskazuje faktycznie widoczną płaszczyznę za nim, poza sylwetką nie ma fałszywego body hit.
4. Zakrzywiona Cut wall: odmawia podparcia; światowa płaszczyzna z tyłu nie przechwytuje; światowa płaszczyzna przed bryłą może być świadomie wybrana.
5. History → Sketches: Keep, reuse Add/Cut, Delete unused, Edit Feature/Cancel, Undo/Redo i roundtrip `.forge` na fizycznym urządzeniu.
6. Największa systemowa czcionka, pion/poziom: kontrolki ≥48 dp, bez nałożenia chipu na History.
7. Sculpt stroke, Freeform cage selection, zwykłe wybieranie obiektów — brak regresji hit testów; palec + realny rysik (hover jeśli sprzęt obsługuje).
8. Na ARM64 odczuwalna wydajność pod dużą siatką i w Sculpt, bez deklaracji absolutnej liczby FPS bez instrumentacji.

**Reguła verdict:** jakikolwiek FAIL → `OWNER-PHYSICAL-FAIL`, nie merge, wąski prompt korekcyjny. Bez FAIL, ale jakikolwiek NOT TESTED → `OWNER-PHYSICAL-PARTIAL/PENDING`; tylko wszystkie wymagane punkty PASS lub odrębnie zaakceptowane N/A → `OWNER-PHYSICAL-FULL-PASS`. Nawet pełny PASS nie jest automatyczną zgodą na integrację `main`.

## 4. OPEN DESIGN HOLD — A2 (nadal bez decyzji)

`BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH`: Keep Sketch na XY/XZ/YZ wewnątrz już istniejącego CAD Body jest w tym modelu niedozwolone. Warianty z repo:
0. bez zmiany (zachować blokadę);
1. dodatkowe szkice body-local na płaszczyznach świata, nowa semantyka / `CADB v9`, starszy reader ma jawnie odmawiać;
2. body-owned konstrukcyjne płaszczyzny parametryczne, nowa encja + wersja formatu;
3. project-level sketches, zmiana semantyki dokumentu / Object/History/codec.

**Żadna opcja nie została zatwierdzona przez OWNER.** Tymczasowa wcześniejsza akceptacja starszego APK pozwoliła kontynuować prace, ale nie rozstrzyga A2 ani pełnej akceptacji obecnego APK. Nie uruchamiać implementacji formatu w drodze domyślnej preferencji koordynatora.

## 5. Następna praca Claude / proces

**Nie ma aktywnego następnego zadania kodowego.** Po raporcie fizycznym OWNER i jawnej decyzji A2 należy przeanalizować faktyczne FAIL/PASS/NOT TESTED oraz osobno zgodę/odmowę na zmianę formatu. Następny pojedynczy prompt przygotowany przez koordynatora to `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1` — DOCS-ONLY, gotowy, ale **NIE URUCHAMIAĆ bez OWNER evidence**. Niech Claude nie samorozpoczyna CADB v9, FullSharded, merge ani nowego etapu funkcji. Jeśli OWNER zgłosi FAIL, bezpośrednio przygotować prompt korekcyjny zamiast przedłużać docs-only.

## 6. Dokumentacja i provenance

Ostatni pełny kanoniczny `FORGESHAPE_MASTER_PLAN_v4.1.61.md` i pełne HISTORY v4.1.61 nadal nie są dostępne w bieżącym runtime. Zachowano dawne pliki master/recovery i addenda v4.1.62–64; wydano addenda v4.1.65, NIE zastępujące oryginału. W repo `PROJECT_STATUS.md` v0.102.0 potwierdza stan TECH PASS/OWNER REVIEW/NO MERGE. Należy scalić dodatki z kanonem dopiero gdy źródło jest dostępne.

---

## ARCHIVED: full previous coordinator state v4.1.64 (preserved verbatim)

# FORGESHAPE_PROJECT_STATE — v4.1.64

**Data:** 2026-10-28 | **Koordynator:** ChatGPT | **Repo:** `damiankrok/Forgeshape`  
**Status obowiązujący:** `CAD-R2-C1 TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 WORLD-PLANE DESIGN HOLD / NO MERGE`.  
**Hierarchia źródeł:** GitHub refs i pobrane CI artifacts > repozytoryjne `PROJECT_STATUS.md` i dossier etapu > relacja Claude > wcześniejsze wersje planu. Nie traktować `TECH PASS` jak pełnego OWNER PASS.

## 1. Bieżący etap i decyzja koordynacyjna

`CAD-R2-C1-POST-IMPLEMENTATION-QUALITY-GATE` zakończony technicznie tokenem Claude `PASS-CAD-R2-C1-WITH-RECORDED-BLOCKERS`. Koordynator niezależnie potwierdził refy, diff, GitHub run conclusions, ZIP evidence, XML JVM, DEVICE counts, corpus parity, release guard i APK SHA. **Host 4375/0 oraz 17 geometrycznych oracles są zapisane w repo, lecz nie zostały uruchomione niezależnie przez koordynatora.**

**Zweryfikowany rezultat produktu:** błędne przejście przez niekwalifikowalną zakrzywioną powierzchnię do płaszczyzny świata ZA bryłą usunięte. Zachowany priorytet płaszczyzny w PRZODZIE. Chip Edit Sketch znika przy otwartym History, co usuwa konflikt kliknięć przy systemowym font scale 1.5. Nowe scenariusze obejmują font scale 1.0/1.3/1.5, obrót i powrót do aplikacji. No code copied from SolidCraft.

**Otwarte decyzje / ryzyka:**
- `BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH`: Keep Sketch na world plane w istniejącym body nie jest obsługiwany; do wyboru OWNER wariant 0/1/2/3, ale **żaden nie został zaakceptowany**. Domyślnie zachować zachowanie i odmowę z `R2_SK_06`, `D18`. Opcja 1 wymagałaby osobno zaakceptowanej `CADB v9`, NIE jest domyślnym zakresem.
- `PICK-RAY-TRIANGLE-SEAM`: zdiagnozowany stary problem `pickSceneSnapshot` na wspólnej krawędzi trójkątów przy odległym ortho eye (~250 m); może powodować brak trafienia i wtórny wybór world plane. Wymaga reproduktora i dopiero uzasadnionej minimalnej poprawki.
- `HOST-X86-LAYOUT-PERF`: rzeczywista zmienność `eval_all` +32.5% i `trim100` +38.2% w self-test binary jest zależna od link layout / JCC errata. Produktowy A/B link +1.3% / +1.0%, mitgacja granic 32B ≈ bez istotnej luki; `NO-REPRO-STABLE` **dotyczy przyczynowości zmian CAD-R2**, nie dowodzi płaskiej wydajności urządzenia.
- `ARM64-PERF`, fizyczny rysik, hover, GPU, 16KB page, ergonomia: **niepotwierdzone fizycznie**. CI DEVICE to emulator x86_64, nie telefon.
- `tap None clears aim` to obecne zachowanie ze względów bezpieczeństwa confirm; zachować bez samowolnej zmiany UX.

**Kolejny proponowany pojedynczy etap:** `CAD-R2-C2-PICKING-SEAM-CORRECTNESS-R1`, punktowa reprodukcja i ewentualna naprawa wspólnej krawędzi trójkątów. Można zlecić jako niezależną naprawę stabilności bez niepotwierdzonego OWNER PASS, lecz nie wolno merge'ować ani rozszerzać MVP. Po tym OWNER fizycznie zatwierdza najnowszy APK. Wartość etapu to poprawność już istniejących wejść, nie nowa funkcjonalność.

## 2. GitHub HEAD, dowód pochodzenia (2026-10-28)

| Ref | SHA |
|---|---|
| `main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` |
| `feature/modeling-r1-owner-shell-sketch-fix-r1` | `636797e518898c31c4cd691d453db2833ef00896` |
| `feature/cad-r2-retained-sketch-parametric-ux-r1` | `ea24d20805d9487d26221d9dd45e47dc16fb6ecb` |
| **`feature/cad-r2-c1-quality-gate-r1`** | **`08406cfbbff471f6e53c576c24b45791f903301c`** |
| ostatni produkt C1 | `cf340f04646da0b89f0ff53d6cd996316ac3dba9` |
| DEVICE A head | `cf340f04646da0b89f0ff53d6cd996316ac3dba9` |
| DEVICE B head | `e81639b1ead69dcd5402388f90dfbfbe0aca71bf` |
| CI FAST head | `911f5c3b09c97b9f1fa9ebc8339741a4e6681a8c` |

Github compare R2→C1: **10 commits ahead / 0 behind, 22 paths changed**, 3 product-file locations (`forgeshape_support_chooser.cpp`, `CadExtrudeCanvasView.java`, `EditorWorkspaceView.java`) plus chooser header, host and DEVICE tests, and dossier. Compare `cf340f0...08406cf`: **3 commits docs-only**, no `app/` diff. `main` i R2 branch bez zmian. **Nie merge'owano**.

## 3. Zweryfikowane bramki jakości

| Bramka | Wynik | Dowód |
|---|---|---|
| Native host | `HOST_SELFTESTS_OK (4375 checks, 0 failed)` 25 suites | repo TESTS.md, nie uruchomione osobno |
| Geometry oracles | `CAD_R2_ORACLE_PASS=17` | repo TESTS.md; nie uruchomione osobno |
| JVM | **196/196**, 0 failures/errors/skips, 20 XML | **niezależnie z ZIP FAST** |
| DEVICE A | **67/67**, 0 FAIL, startup 25/25 clean | **niezależnie z ZIP** ID 12846579227, run 40752389411 |
| DEVICE B | **55/55**, 0 FAIL, 3rd capture startup 25/25 clean | **niezależnie z ZIP** ID 12849042735, run 40760227555; first two captures dropped lines |
| DEVICE combined | **122/122** 2 oddzielne runy, zgodny product tree | compare i ZIP artifacts |
| CI FAST | success `40766013297` | GitHub + ZIP |
| Corpus | `committed=72 generated=72 mismatches=0`, PASS | ZIP `forge-corpus-parity.txt` |
| Release guard | PASS, x86_64/arm64 release 0 SelfTests symbols / 0 strings; debug 35 / 398 | ZIP `release-selftest-guard.md` |
| APK | **17 552 535 B**; SHA-256 **`fd7a11a9a7b1bbf7748b46e2d71c71a2a830b54f59e8bd7f9011113d968b1dd5`** | niezależny SHA z CI ZIP, bez rebuild |
| Archive ZIP FAST | SHA-256 `ddca52c843cbcd0516c760b153913a1a8bc124d8942e563b367dee8e28b8361b` | niezależny SHA równy GitHub digest |

FAST artifact: `ci-fast-evidence` **12849379257**, expiry 2026-11-11T20:52:13Z. Ścieżka wewnątrz `app/build/outputs/apk/debug/app-debug.apk`. Lokalna kopia oryginału: `/mnt/data/ForgeShape_CAD_R2_C1_OWNER_911f5c3.apk`. Debug APK, nie produkcyjny podpisany AAB.

CI URLs: https://github.com/damiankrok/Forgeshape/actions/runs/40752389411 ; https://github.com/damiankrok/Forgeshape/actions/runs/40760227555 ; https://github.com/damiankrok/Forgeshape/actions/runs/40766013297 .

## 4. Granice następnego zadania

`CAD-R2-C2` może dotyczyć wyłącznie potwierdzonego CPU ray/triangle seam i regresji wyboru ścian/myszy/rysika. Start z exact `08406cfbbff471f6e53c576c24b45791f903301c`, NOWA gałąź. Najpierw BEFORE z próbą odtworzenia oryginalnego błędu, test czerwony, potem minimalna poprawka i pełne testy. Jeśli reprodukcja nie potwierdzi lub poprawka narusza invariant/range/perf, zatrzymać się z `BLOCKED`, bez wymuszonej zmiany kodu.

**Zakazy:** zmiana .forge, nowych ABI/JNI slot bez potrzeby, kernela, renderer rewrite, nowego solvera, Fillet/Chamfer, reorder/suppression, CADB v9, dodatkowych root sketch, kopiowania SolidCraft, zmiany zachowania world-plane z C1, merge/rebase/force push, FullSharded, uznania emulatora za OWNER physical PASS.

## 5. Provenance dokumentacji

GitHub `PROJECT_STATUS.md` na C1 HEAD zgłasza status v0.101.0; źródła `artifacts/cad-r2-c1-quality-gate/{BEFORE,PERF_PROTOCOL,PERF_ANALYSIS,PERF_RAW*,HIT_OWNERSHIP,TESTS,SUMMARY}.md`. Dokumenty koordynacyjne v4.1.62/63 zachowane. Pełny kanoniczny `FORGESHAPE_MASTER_PLAN_v4.1.61.md` oraz pełne HISTORY v4.1.61 nie są aktualnie dostępne w runtime — **nie zastępować ich skrótem**; wydano addenda v4.1.64 do integracji z oryginałami. Jeśli dostępne, należy włączyć delty do sekcji 1 Master i odpowiedniego rejestru historii bez usunięć.
