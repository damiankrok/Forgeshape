# Research: jak działają Shapr3D, Inventor i FreeCAD — i co z tego wziąć do ForgeShape

Dokument dla właściciela projektu (OWNER). Opisuje, jak trzy znane programy CAD
prowadzą użytkownika przez modelowanie (szkic → bryła → kolejne szkice →
zaokrąglenia/fazy), porównuje to z ForgeShape w stanie z 2026-11-03 i proponuje
kolejność dalszych etapów. Nie jest to zapis zweryfikowanego zachowania produktu
(to należy do `PRODUCT.md`), tylko analiza i propozycja.

## 1. Wspólny model pracy we wszystkich trzech programach

Wszystkie trzy są **parametrycznymi modelerami opartymi na cechach (features)**:

1. **Szkic 2D** na płaszczyźnie (XY/XZ/YZ) albo **na płaskiej ściance** już
   istniejącej bryły.
2. **Cecha bryłowa** ze szkicu: wyciągnięcie (Extrude / Pad), wycięcie
   (Cut / Pocket), obrót (Revolve / Revolution / Groove), czasem przeciągnięcie
   po ścieżce (Sweep) i przejście między przekrojami (Loft).
3. **Cechy wykończeniowe** nakładane na krawędzie i ścianki gotowej bryły:
   zaokrąglenie (**Fillet** — to, co w Blenderze nazywa się *bevel*), faza
   (**Chamfer**), skorupa (**Shell**), pochylenie ścian (**Draft**), otwór
   (**Hole**), gwint.
4. **Szyki i lustra**: Mirror, szyk liniowy, szyk kołowy.
5. **Historia (drzewo cech)**: każdą cechę można później otworzyć i zmienić,
   a cała reszta przelicza się od nowa.

ForgeShape ma już punkty 1, 2 (Extrude New Body / Add / Cut, Revolve New Body),
5 (History) i lustro całej bryły; **nie ma** jeszcze punktu 3 ani szyków.

## 2. Shapr3D — wzorzec dla tabletu

Shapr3D jest najbliższym wzorcem, bo jest projektowany pod dotyk i rysik.

- **Bez trybów „wejdź w szkic / wyjdź ze szkicu”.** Użytkownik po prostu
  rysuje linią lub prostokątem na płaszczyźnie lub na ściance bryły — płaszczyzna
  szkicu bierze się z tego, w co trafił palec. Zamknięty obrys od razu staje się
  zaznaczalnym profilem.
- **Zaznaczenie podpowiada następny krok** (adaptacyjny pasek narzędzi):
  zaznaczysz profil → aktywne jest Extrude; zaznaczysz krawędź → Fillet/Chamfer;
  zaznaczysz ściankę → Extrude (push/pull) albo Offset.
- **Push/pull bezpośrednio na geometrii**: przeciągnięcie strzałki na ściance lub
  profilu dodaje albo odejmuje materiał; wartość można też wpisać.
- **Fillet i Chamfer jednym gestem**: zaznacz krawędź, przeciągnij strzałkę od
  bryły lub do niej. Na forum użytkownicy narzekają, że kierunek bywa nieczytelny
  — dlatego pole z wartością jest konieczne obok gestu.
- **Historia parametryczna** w osobnym panelu, z możliwością edycji kroku.

Wniosek dla ForgeShape: najważniejsze jest **„dotknij ścianki → rysuj na niej”**
i **„zaznaczenie podpowiada akcję”**. Pierwsza połowa jest zrobiona w tym etapie
(przycisk „Sketch here”), druga to naturalny następny krok.

## 3. Autodesk Inventor — wzorzec „desktopowego” CAD

- **Nowy szkic**: *Start 2D Sketch* → kliknij płaszczyznę lub płaską ściankę.
  Najszybciej z **marking menu** (menu radialne pod prawym przyciskiem) — zawiera
  m.in. New Sketch, Extrude, Hole, Fillet, Work Plane.
- **Extrude** ma: odległość, *To* (do ścianki), *Through All*, *Symmetric*,
  operacje Join / Cut / Intersect / New Solid i pochylenie (taper).
- **Cechy umieszczane (placed features)**: Fillet, Chamfer, Hole, Thread, Face
  Draft, Shell — wybierasz krawędzie/ścianki bryły, nie rysujesz szkicu.
  Chamfer zdejmuje materiał z krawędzi zewnętrznej, a dodaje na wewnętrznej.
- **Shell**: wskazujesz ścianki do usunięcia, reszta staje się ściankami o
  zadanej grubości; cechy dodane po Shell nie są już „wydrążane”.
- **Hole**: punkt szkicu na ściance + typ otworu (prosty, fazowany, pogłębiony,
  gwintowany).

## 4. FreeCAD (Part Design) — wzorzec otwarty

- Wszystko dzieje się w **Body**. Nowy szkic dołącza się (*attach*) do płaszczyzny
  z Origin albo do **zaznaczonej ścianki** — zaznaczasz ściankę i klikasz
  „Create sketch”, szkic powstaje na niej.
- Cechy addytywne: **Pad** (długość, symetrycznie, „do ścianki”), **Revolution**;
  subtraktywne: **Pocket**, **Groove** (obrót odejmujący).
- **Dress-up**: **Fillet**, **Chamfer**, Draft, Thickness (= Shell).
- **Transformacje**: Mirrored, LinearPattern, PolarPattern, MultiTransform.
- Szkicownik ma **solver więzów** (równoległość, prostopadłość, wymiar) i liczy
  stopnie swobody.

## 5. Gdzie jest ForgeShape (stan na 2026-11-03)

| Obszar | ForgeShape | Shapr3D / Inventor / FreeCAD |
| --- | --- | --- |
| Szkic na płaszczyźnie świata | jest | jest |
| Szkic na płaskiej ściance | jest; **od tego etapu także jednym dotknięciem ścianki + „Sketch here”** | jest (Shapr3D: bez przycisku) |
| Extrude / Cut / Add, One Side / Symmetric / Two Sides | jest | jest |
| Extrude „do ścianki”, „przez wszystko”, pochylenie | brak | jest |
| Revolve | tylko New Body | New Body / Join / Cut |
| Fillet / Chamfer (bevel) | **brak** | jest — najczęściej używana cecha |
| Shell, Hole, Draft | brak | jest |
| Szyki (liniowy, kołowy), Mirror cechy | brak (jest Mirror całej bryły) | jest |
| Więzy szkicu (solver) | brak (są wymiary Driving/Reference) | FreeCAD/Inventor: jest |
| „Zaznaczenie podpowiada akcję” | częściowo (strzałka Extrude po Finish) | Shapr3D: wszędzie |

## 6. Co proponuję i w jakiej kolejności

Każdy punkt to osobny etap (jeden etap = jedna rzecz, zgodnie z `CLAUDE.md`).

### Etap A — Chamfer i Fillet prostych krawędzi — **ZROBIONE w `CAD-EDGE-FINISH-R1`**

Stan po wdrożeniu: dotknięcie obok prostej krawędzi bryły CAD pokazuje
**Fillet / Chamfer**; dotknięcie środka płaskiej ścianki pokazuje obok „Sketch
here” również **Round edges / Bevel edges** (wszystkie krawędzie ścianki naraz,
jedno Cofnij). Wartość jest podpowiadana (1/2/5 × 10^k, zawsze mieści się),
a wykończenie widać i edytuje się w Historii (z przyciskiem Usuń). Zapis w
`.forge` to `CADB` v9. Ograniczenia R1 są nazwane w komunikatach: tylko
krawędzie proste, wypukłe, między płaskimi ściankami wyciągnięcia New Body/Add.

Pierwotny plan (zachowany dla kontekstu):


To jest „bevel”, o który pytasz, i najczęściej używana cecha w każdym CAD.
Jak to zrobić **bez łamania zasad projektu**:

- Jedyna biblioteka geometryczna to Manifold, a Manifold **nie ma** operacji
  fillet. Ale dla **prostej krawędzi między dwiema płaskimi ściankami** (a to są
  prawie wszystkie krawędzie brył z Extrude) wynik można policzyć dokładnie
  własną geometrią i jednym boolean `Difference`, który już jest w łańcuchu cech:
  - **Chamfer**: od bryły odejmujemy pryzmat o przekroju trójkąta (dwie odległości
    na obu ściankach) wyciągnięty wzdłuż krawędzi.
  - **Fillet**: odejmujemy pryzmat o przekroju „narożnik minus ćwiartka koła”
    (promień r, 32 segmenty jak okrąg w szkicu) wyciągnięty wzdłuż krawędzi.
- Krawędź musi być nazwana **semantycznie** (dwie sąsiednie ścianki jako
  `CadFaceToken` + cecha), nigdy indeksem trójkąta — tak jak dziś ścianki.
- Nowa cecha w łańcuchu (`CadFeatureKind::EdgeTreatment`), nowa wersja `CADB`
  pisana tylko wtedy, gdy bryła ją ma (stare projekty bez zmian bajtowych).
- UX jak w Shapr3D: dotknij krawędzi → pojawia się strzałka i pole wartości;
  wybór Fillet/Chamfer w docku akcji.
- Ograniczenie pierwszej wersji (nazwane w UI, nie ukryte): tylko proste
  krawędzie między płaskimi ściankami, kąt dowolny; krawędzie zakrzywione
  i łańcuchy styczne — później.

### Etap B — „Zaznaczenie podpowiada akcję”

Po dotknięciu ścianki pokazuje się nie tylko „Sketch here”, ale mały dok:
Sketch here · Extrude ścianki (push/pull) · (po etapie A) Fillet/Chamfer krawędzi.
Push/pull ścianki to po prostu nowa cecha Add/Cut z profilem = obrys ścianki.

### Etap C — Extrude „Through All” i „To face”

Dwa nowe tryby zasięgu, które w praktyce zastępują ręczne wpisywanie głębokości
przy wycięciach (otwory na wylot).

### Etap D — Hole

Punkt szkicu na ściance + średnica + głębokość/na wylot (+ fazowanie). Zbudowany
na Cut + Revolve, bez nowej geometrii.

### Etap E — Szyk liniowy i kołowy cech

Powtórzenie istniejącej cechy Add/Cut N razy; każda kopia to transformacja
narzędzia boolean, nie kopia siatki.

### Etap F — Shell

Wymaga offsetu ścian (Manifold nie ma), więc dopiero po A–E i z osobną decyzją.

## 7. Co zrobiono w tym etapie (`STARTUP-FACE-ICONS-R1`)

- **Szkic na ściance jednym dotknięciem**: dotknięcie płaskiej ścianki bryły CAD
  pokazuje obok palca przycisk **„Sketch here”**; jego dotknięcie otwiera szkic
  na tej ściance (wcześniej: logo → szuflada → New Sketch → dwa dotknięcia
  ścianki).
- **Szybki start**: 25 zestawów autotestów (ok. 4500 sprawdzeń) wykonywało się
  przy każdym uruchomieniu wersji debug na wątku UI — ok. 20 s czarnego ekranu.
  Teraz działają tylko na żądanie (`setprop debug.forgeshape.selftests 1`, co
  robi CI).
- **Nowe logo i ikona aplikacji** (wcześniej aplikacja nie miała własnej ikony)
  oraz przerysowane ikony, które były niejednoznaczne albo identyczne dla różnych
  akcji.

## Źródła

- [Shapr3D — Sketching in Shapr3D](https://support.shapr3d.com/hc/en-us/articles/18816009328284-Sketching-in-Shapr3D)
- [Shapr3D — Bracket Mount: Modeling Basics](https://support.shapr3d.com/hc/en-us/articles/12470669638300-Bracket-mount)
- [Shapr3D — 3D modeling (push/pull, Fillet/Chamfer, adaptive toolbar)](https://www.shapr3d.com/product/3d-modeling)
- [Shapr3D forum — Future features + fixes?](https://discourse.shapr3d.com/t/future-features-fixes/9225)
- [Autodesk Inventor — About Marking Menus](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-4B081B74-45EA-451E-84BE-9FFE0E2ACAE3.htm)
- [Autodesk Inventor LT — Placed features](https://help.autodesk.com/cloudhelp/2021/ENU/InventorLT-Help/files/GUID-B6A701E6-AFEB-4272-9EFC-EDC156FB8A59.htm)
- [Fillet and Chamfer features (Inventor, OpenTextBC)](https://opentextbc.ca/autodesk/chapter/fillet-and-chamfer-features)
- [FreeCAD — PartDesign Workbench](https://wiki.freecad.org/PartDesign_Workbench/id)
- [FreeCAD — PartDesign tutorial](https://wiki.freecad.org/PartDesign_tutorial)
- [FreeCAD — PartDesign Pad](https://wiki.freecad.org/PartDesign_Pad/en)
- [FreeCAD — Creating a simple part with PartDesign](https://wiki.freecad.org/Creating_a_simple_part_with_PartDesign)
