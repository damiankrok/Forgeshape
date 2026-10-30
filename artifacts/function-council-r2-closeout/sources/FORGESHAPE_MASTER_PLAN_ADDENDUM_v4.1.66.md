# FORGESHAPE MASTER PLAN — ADDENDUM v4.1.66

**Data:** 2026-10-29 (Europe/Berlin). **Typ zmiany:** OWNER direction / audit execution planning. **Produkt:** bez zmian. **Nie zastępuje pełnego kanonu Master Plan v4.1.61.**

## Aktualny stan i nowa decyzja OWNER

OWNER wybiera **pełny, oparty na źródłach i zautomatyzowanych testach FUNCTION-COUNCIL-R2** jako następną pracę, zamiast uzależniać odkrywanie problemów od ręcznego testowania kolejnych APK. Zlecony audyt obejmuje całą dotychczas należną funkcjonalność ForgeShape według zatwierdzonego Master Planu, rzeczywistą implementację i interakcje funkcji, niespełnione wymagania, problemy UX/UI Androida, analizę jakości/potencjału rozwoju oraz **studium portowalności na PC (Windows/Linux)**. Jest to decyzja o **AUDYCIE**, nie zgoda na implementację nowych narzędzi CAD, desktop port, nową bibliotekę, CADB v9 czy integrację.

**Przewidziana praca:** `FUNCTION-COUNCIL-R2-FULL-CAPABILITY-UX-PLATFORM-AUDIT`; jeden aktywny prompt `PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md`. Wykonanie przez Claude Code na nowej gałęzi od weryfikowanej bazy `feature/cad-r2-c2-picking-seam-r1@70a6c14f703bfda2ab689b855126c1d12a541e41`; audyt jest DOCS/DIAGNOSTICS-ONLY. Nie stwierdzono jeszcze wykonania tego audytu, jego wynik pozostaje `NOT STARTED / NOT VERIFIED`.

### Status produktu pozostaje niezmieniony

`CAD-R2-C2: TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED`. Ostatnia potwierdzona techniczna baza: `70a6c14f703bfda2ab689b855126c1d12a541e41`; `main` `6c9f156c1df91fe1a55445aed6186105a075ab31`. CI FAST 40787465884, DEVICE 40776968165 + 40782861799, corpus 72/72, JVM 196/196, debug APK SHA256 `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`. Te dowody **nie** stają się automatycznie pełnym OWNER physical PASS.

### Wymagania i bramki Council

1. Mapowanie H0 (zatwierdzone MVP należne do dziś), H1 (późniejsze autoryzowane funkcje), H2 (przyszłe/planned/blocked) z oddzielnymi werdyktami. Zero automatycznego rozszerzenia MVP.
2. 10 odrębnych seatów: produkt/journeys; Construction; CAD/Sketch/History; Sculpt/Freeform/Surface; persistence/lifecycle; Android UI/UX/accessibility; przyszły desktop; renderer/picking/performance; test/CI; niezależny adversarial review.
3. Wszystkie dostępne funkcje oraz krytyczne ścieżki odmowy/Undo/.forge, testy i dowody; 30+ opisanych journey; brak PASS bez konkretnej podstawy.
4. Ponowny audyt `FUNCTION-COUNCIL-R1` na aktualnej bazie; stary R1 miał sześć seatów, z których CAD, Sculpt, Android i renderer były jedynie częściowe, więc stare wnioski wymagają świeżej weryfikacji.
5. Zautomatyzowane testy istniejących suite, kompilacji, corpus, CI/DEVICE z realistyczną informacją o faktycznie wykonanych testach. Brak zgody na FullSharded, modyfikowanie produktu/testów/CI ani merge.
6. Dla Android UI/UX: screenshoty z konfiguracją, realne prostokąty hit, orientacja, font scale, stylus/pointer input, IME, dostępność i spójność. Kwestie fizycznego GPU/termiki/rysika i subiektywnej oceny pozostają `OWNER-PHYSICAL-UNVERIFIED`.
7. Dla przyszłego Windows/Linux: matryca przenośności i warianty adapterów C++/JNI/Java/Vulkan; **bez** implementacji, wymiany kernela lub nowej licencji runtime.
8. Wyniki: wymagania→kod→test, recheck starego długu Council D1–D7, ocena UX, plan desktop, uporządkowany backlog rozdzielający DEFECT/QUALITY/FUTURE IDEA, rejestr decyzji OWNER, jeden najmniejszy rekomendowany przyszły etap — nie rozpoczęty.
9. Jeśli pełny Master Plan v4.1.61 (lub późniejszy kanoniczny) nie jest dostępny, audyt kodu może trwać, ale wynik zgodności z Master Planem musi być `PARTIAL-PLAN-SOURCE-GAP`; zabrania się fikcyjnego odtworzenia brakującej wersji.

### Otwarta decyzja A2

`BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH` pozostaje niezmienione. Council może porównać opcje 0/1/2/3 jako materiał decyzyjny, nie wdraża ich i nie wybiera sam. Fizyczny przegląd najnowszego APK nadal stanowi oddzielną, nierozstrzygniętą bramkę; Council nie może go zamknąć wynikiem emulatora.

## Provenance

W obecnym środowisku dostępne są tylko częściowy Master v4.1.58, wcześniejsze v4.1.0 / starsze archiwa i addenda v4.1.62–65; pełny kanon v4.1.61 i pełne HISTORY nie są zamontowane. Zachować wszystkie poprzednie delty, nie nadpisywać kanonu. Weryfikacja refów wykonana przez GitHub connector 2026-10-29. Źródła repo: `PROJECT_STATUS.md` v0.102.0, `artifacts/function-council-r1/`, `artifacts/function-council-c1-d1/`, `artifacts/cad-r2-c2-picking-seam/`.
