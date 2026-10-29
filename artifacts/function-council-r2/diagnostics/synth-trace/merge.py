#!/usr/bin/env python3
"""FC2 synthesis: merge work/req_*.tsv into requirements_traceability.tsv.
Usage: python3 -I merge.py <council_dir>"""
import csv, glob, os, sys, collections
D = sys.argv[1]
WORK = os.path.join(D, 'work')
OUT = os.path.join(D, 'requirements_traceability.tsv')

VOCAB = ["PASS-REPRODUCED","PASS-SOURCE-AND-RECORDED-RUNTIME","IMPLEMENTED-UNTESTED","PARTIAL",
 "DEFECT-CONFIRMED","REGRESSION-CONFIRMED","INACCESSIBLE","DOC-DIVERGENCE","NOT-IMPLEMENTED-BUT-DUE",
 "FUTURE-NOT-DUE","BLOCKED-OWNER","OUT-OF-SCOPE","UNDETERMINED-SOURCE-GAP"]
# weakness order for split verdicts (stronger -> weaker)
PASSRANK = {"PASS-REPRODUCED":0,"PASS-SOURCE-AND-RECORDED-RUNTIME":1,"IMPLEMENTED-UNTESTED":2,"PARTIAL":3,"DEFECT-CONFIRMED":4}

# keep_id <- [dropped duplicate ids]; chosen by hand after reading every row (see REQUIREMENTS_TRACEABILITY.md)
MERGE = {
 "UI-001":["APP-001"], "UI-002":["APP-002"], "UI-003":["APP-008"], "UI-004":["APP-011","CAD-001"],
 "CAD-002":["APP-012","DOC-060"], "APP-021":["UI-005"], "APP-024":["UI-007"],
 "APP-025":["MOD-048","DOC-071"], "APP-026":["MOD-049","DOC-072","ADV-046","CAD-099"],
 "APP-027":["DOC-053"], "APP-030":["DOC-054"], "ADV-001":["APP-031","DOC-011"],
 "APP-033":["DOC-047"], "APP-035":["ADV-028"], "APP-037":["ADV-027"], "APP-039":["DOC-049"],
 "APP-041":["DOC-051","ADV-029"], "CON-088":["DOC-050"], "APP-042":["DOC-057"],
 "APP-045":["DOC-055","ADV-030"], "APP-044":["DOC-056"], "APP-046":["DOC-066","ADV-031"],
 "APP-048":["MOD-059"], "APP-049":["DOC-070"], "APP-052":["DOC-083"],
 "APP-053":["DOC-040","ADV-016","ADV-019","RND-053","PORT-050"], "DOC-037":["ADV-015"],
 "DOC-038":["PORT-048","ADV-012","ADV-020"], "ADV-013":["DOC-039"], "APP-054":["DOC-042"],
 "APP-055":["DOC-043","ADV-023","ADV-024"], "APP-056":["DOC-045","ADV-025"], "APP-057":["ADV-026"],
 "DOC-044":["APP-081"], "APP-058":["DOC-046"], "APP-061":["UI-008"], "APP-064":["UI-054"],
 "APP-065":["UI-060","ADV-070"], "APP-066":["UI-056"], "APP-067":["UI-058"],
 "APP-068":["UI-059","CON-019","CON-094"], "APP-070":["PORT-052"],
 "APP-071":["DOC-078","ADV-068","PORT-063"], "RND-006":["APP-072","DOC-077b","ADV-069"],
 "APP-075":["CON-044","UI-035"], "PORT-026":["APP-076"], "APP-077":["PORT-027","UI-080"],
 "APP-087":["DOC-052","MOD-083"], "APP-084":["DOC-080"], "APP-073":["UI-041"],
 "CON-024":["TST-076"], "CON-025":["DOC-067"], "CON-026":["ADV-047","RND-040"], "CON-029":["ADV-052"],
 "CON-031":["MOD-041","ADV-048"], "CON-033":["ADV-043"], "CON-034":["DOC-021"], "CON-037":["ADV-042"],
 "CON-038":["ADV-045"], "CON-040":["DOC-019","DOC-020"], "CON-041":["DOC-036"],
 "CAD-009":["CON-045","ADV-041"], "CON-046":["MOD-039"], "MOD-058":["CON-050","ADV-044"],
 "CON-058":["ADV-049"], "CON-063":["ADV-032"], "CON-065":["MOD-055","ADV-036"], "CON-066":["ADV-033"],
 "CON-067":["CON-089","DOC-061","DOC-059"], "MOD-027":["UI-069","ADV-035"],
 "CON-070":["DOC-012","DOC-013","ADV-008","ADV-009"], "CON-071":["ADV-034","PORT-068"],
 "CON-073":["APP-005"], "CON-077":["MOD-040"], "CON-081":["RND-026"], "CON-082":["RND-022"],
 "CON-083":["UI-061"], "CON-084":["RND-020"], "CON-086":["ADV-065"], "CON-010":["ADV-073"],
 "CON-014":["ADV-059"], "CON-016":["RND-048"], "CON-059":["RND-042"], "CAD-003":["UI-032"],
 "CAD-005":["RND-044","PORT-071"], "CAD-008":["CON-080"], "CAD-013":["UI-018"], "CAD-018":["UI-065"],
 "UI-051":["ADV-067"], "CAD-036":["UI-022"], "CAD-037":["UI-023"], "CAD-054":["UI-025"],
 "CAD-057":["UI-026"], "CAD-066":["RND-034"], "CAD-067":["RND-032"], "CAD-074":["RND-030"],
 "CAD-075":["UI-027"], "CAD-076":["UI-084","TST-052"], "CAD-093":["UI-030"], "CAD-096":["UI-016"],
 "CAD-098":["ADV-037"], "CAD-100":["ADV-038"], "CAD-070":["ADV-040"], "CAD-069":["ADV-039"],
 "MOD-061":["CAD-101"], "CAD-102":["DOC-022"], "TST-020":["DOC-010","PORT-008","CAD-103"],
 "DOC-008":["ADV-010"], "TST-041":["UI-081"], "UI-010":["CAD-115"], "DOC-079":["ADV-004"],
 "DOC-028":["ADV-003"], "DOC-033":["ADV-005"], "DOC-086":["RND-052","ADV-022"], "DOC-063":["MOD-042"],
 "DOC-064":["UI-046"], "DOC-073":["RND-041"], "DOC-075":["MOD-060","MOD-026"], "DOC-077a":["RND-005"],
 "TST-014":["DOC-082","PORT-035","ADV-061"], "DOC-031":["MOD-085"], "DOC-032":["MOD-108"],
 "CON-052":["UI-037"], "UI-044":["TST-043"], "UI-045":["PORT-073","RND-009"], "MOD-043":["UI-062"],
 "MOD-031":["UI-063"], "MOD-001":["UI-064"], "MOD-090":["UI-066"], "MOD-100":["UI-067"],
 "UI-079":["PORT-024"], "RND-001":["PORT-074"], "RND-003":["PORT-019"], "RND-008":["PORT-022"],
 "TST-003":["PORT-033"], "RND-004":["PORT-047"], "RND-064":["TST-061"], "TST-028":["PORT-067"],
 "TST-029":["PORT-016"], "UI-033":["PORT-025"], "RND-058":["TST-012"], "MOD-019":["RND-061"],
 "MOD-070":["RND-062"], "TST-045":["ADV-072","MOD-044"], "MOD-081":["ADV-050"], "MOD-069":["APP-016"],
 "MOD-102":["APP-018"],
}
# synthesis verdict decisions where merged rows disagreed (reason recorded in the row)
OVERRIDE = {
 "CON-052":("IMPLEMENTED-UNTESTED","UI-037 cites MirrorSmokeTest, which presses no Back (CON-052 E2); weaker half taken"),
 "RND-006":("DEFECT-CONFIRMED","APP-072/ADV-069 graded PASS; seat H R2 (delib_A_to_H O-1, delib_H_to_E #10) shows checkRendererLifecycle runs only from onResume - defect stands at E2"),
 "DOC-044":("DEFECT-CONFIRMED","APP-081's test fsr1b09 covers only a process that HAD a candidate; E-02 is the no-candidate process (E2)"),
 "ADV-013":("DEFECT-CONFIRMED","seat E graded DOC-039 PARTIAL; seat J J-04 (R2 C-17 reworded) proves from source the .pending copy is never read back (E2)"),
 "DOC-063":("DEFECT-CONFIRMED","seat D graded MOD-042 PARTIAL (no H1 rule binds the load path); kept as recorded-debt defect, P3, E1 reproduced by seat E; crafted file only"),
 "ADV-001":("PASS-REPRODUCED","domain half (loadProjectDocument) reproduced; DOC-011's PARTIAL was the JNI-wrapper half, counted once as DOC-087 DEFECT-CONFIRMED"),
 "APP-065":("PARTIAL","UI-060/ADV-070 start from a never-saved project; 'dirties no project' fails for a SAVED project across the palette recreation (APP-039)"),
 "DOC-038":("PARTIAL","ADV-012/ADV-020 hold for a single writer; seat E shows (E2) the concurrent-writer loss path"),
 "DOC-086":("DEFECT-CONFIRMED","seat J graded ADV-022 PARTIAL; seats E and H (RND-052) prove from source the worker reads the fingerprint and encodes at different instants (E2)"),
 "RND-008":("DOC-DIVERGENCE","PORT-022 (synchronous teardown) holds for an acked detach; seat H shows wait_for(5 s) returns unacked, against the 'must block' rule"),
 "UI-004":("PASS-REPRODUCED","CAD-001's PARTIAL was the bootstrap Back path F-03, counted once as UI-009 DEFECT-CONFIRMED"),
}
HORIZON_FIX = {"H0/H1":"H1","-":"H0","H2-pending-decision":"H2"}

def norm_h(h):
    h=h.strip()
    if h.startswith("H1 (owner question"): return "H1"
    if h.startswith("- (evidence gap"): return "NA"
    return HORIZON_FIX.get(h,h)

def norm_v(v):
    v=v.strip()
    if v in VOCAB: return v
    found=[x for x in VOCAB if x in v]
    if not found: raise SystemExit("unknown verdict: "+v)
    if all(x in PASSRANK for x in found):
        return max(found,key=lambda x:PASSRANK[x])
    raise SystemExit("unresolvable split verdict: "+v)

def domain(i):
    p,n=i.split('-')[0],i.split('-')[1]
    num=int(''.join(c for c in n if c.isdigit()))
    if p=="MOD":
        if 68<=num<=91: return "Freeform"
        if 92<=num<=113: return "Surface"
        return "Sculpt+cross-representation"
    return {"APP":"App shell & project lifecycle","CON":"Construction & objects","CAD":"CAD & sketch",
            "DOC":"Document, persistence & interchange","UI":"Android UI/UX","PORT":"Portability",
            "RND":"Renderer, picking & performance","TST":"Test & CI evidence","ADV":"Adversarial integrity"}[p]

rows={}; header=None; order=[]
for f in sorted(glob.glob(os.path.join(WORK,'req_*.tsv'))):
    seat=os.path.basename(f)[4]
    r=list(csv.reader(open(f,encoding='utf-8'),delimiter='\t',quoting=csv.QUOTE_NONE))
    if header is None: header=r[0]
    assert r[0]==header, f
    for x in r[1:]:
        assert len(x)==len(header),(f,x[0])
        assert x[0] not in rows, x[0]
        rows[x[0]]=(seat,x); order.append(x[0])
H={h:i for i,h in enumerate(header)}
dropped={d:k for k,ds in MERGE.items() for d in ds}
assert not set(dropped)&set(MERGE), set(dropped)&set(MERGE)
for k,ds in MERGE.items():
    assert k in rows,k
    for d in ds: assert d in rows,d
out_h=["domain","seat"]+header+["merged_duplicates","original_verdicts","synthesis_note"]
out=[]; conflicts=[]
for i in order:
    if i in dropped: continue
    seat,x=rows[i]; x=list(x)
    orig=[f"{i}={x[H['verdict']]}"]
    h=norm_h(x[H['horizon']]); v=norm_v(x[H['verdict']]); note=[]
    if h!=x[H['horizon']]: note.append(f"horizon normalised from '{x[H['horizon']]}'")
    if v!=x[H['verdict']]: note.append("split verdict normalised to its weaker half")
    merged=[]
    for d in MERGE.get(i,[]):
        ds,dx=rows[d]
        dv=norm_v(dx[H['verdict']])
        merged.append(f"{d} (seat {ds}, {dx[H['horizon']]}, {dx[H['verdict']]}, {dx[H['evidence_grade']]})")
        orig.append(f"{d}={dx[H['verdict']]}")
        if dv!=v and i not in OVERRIDE:
            if dv in PASSRANK and v in PASSRANK and max(PASSRANK[dv],PASSRANK[v])<=1:
                if PASSRANK[dv]<PASSRANK[v]:
                    note.append(f"verdict raised {v}->{dv}: {d} reproduced the same requirement in this audit ({dx[H['evidence_grade']]})")
                    v=dv; x[H['evidence_grade']]=x[H['evidence_grade']]+" + "+d+": "+dx[H['evidence_grade']]
            else: conflicts.append((i,v,d,dv))
        if norm_h(dx[H['horizon']])!=h: note.append(f"{d} horizon {dx[H['horizon']]}")
    if i in OVERRIDE:
        nv,why=OVERRIDE[i]
        if nv!=v: note.append(f"verdict set {v}->{nv}")
        v=nv; note.append(why)
    x[H['horizon']]=h; x[H['verdict']]=v
    if merged:
        dep=x[H['dependencies']]
        x[H['dependencies']]=(dep+"; " if dep not in ("","-") else "")+"duplicate of "+", ".join(m.split(' ')[0] for m in merged)
    out.append([domain(i),seat]+x+["; ".join(merged) or "-"," | ".join(orig),"; ".join(note) or "-"])
with open(OUT,'w',encoding='utf-8',newline='') as fh:
    w=csv.writer(fh,delimiter='\t',quoting=csv.QUOTE_NONE,escapechar='\\',lineterminator='\n')
    w.writerow(out_h); w.writerows(out)
print("input rows",len(rows),"dropped",len(dropped),"output",len(out))
print("unresolved verdict conflicts (canonical kept):")
for c in conflicts: print("  ",c)
