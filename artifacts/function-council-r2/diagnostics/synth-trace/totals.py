#!/usr/bin/env python3
"""Count requirements_traceability.tsv by domain x horizon x verdict. Usage: python3 -I totals.py <tsv>"""
import csv,sys,collections
r=list(csv.DictReader(open(sys.argv[1],encoding='utf-8'),delimiter='\t',quoting=csv.QUOTE_NONE))
V=["PASS-REPRODUCED","PASS-SOURCE-AND-RECORDED-RUNTIME","IMPLEMENTED-UNTESTED","PARTIAL","DEFECT-CONFIRMED","REGRESSION-CONFIRMED","INACCESSIBLE","DOC-DIVERGENCE","NOT-IMPLEMENTED-BUT-DUE","FUTURE-NOT-DUE","BLOCKED-OWNER","OUT-OF-SCOPE","UNDETERMINED-SOURCE-GAP"]
ab={"PASS-REPRODUCED":"PR","PASS-SOURCE-AND-RECORDED-RUNTIME":"PSR","IMPLEMENTED-UNTESTED":"IU","PARTIAL":"PART","DEFECT-CONFIRMED":"DEF","REGRESSION-CONFIRMED":"REG","INACCESSIBLE":"INA","DOC-DIVERGENCE":"DOC","NOT-IMPLEMENTED-BUT-DUE":"NIBD","FUTURE-NOT-DUE":"FND","BLOCKED-OWNER":"BO","OUT-OF-SCOPE":"OOS","UNDETERMINED-SOURCE-GAP":"USG"}
print("rows",len(r)); ids=[x['ID'] for x in r]; assert len(ids)==len(set(ids))
for x in r: assert x['verdict'] in V,(x['ID'],x['verdict'])
c=collections.Counter((x['domain'],x['horizon'],x['verdict']) for x in r)
doms=sorted({x['domain'] for x in r}); hs=sorted({x['horizon'] for x in r})
print("| Domain | Horizon | "+" | ".join(ab[v] for v in V)+" | Total |")
print("|"+"---|"*(len(V)+3))
for d in doms:
  for h in hs:
    row=[c[(d,h,v)] for v in V]
    if sum(row): print(f"| {d} | {h} | "+" | ".join(str(n) if n else "." for n in row)+f" | {sum(row)} |")
for h in hs:
  row=[sum(c[(d,h,v)] for d in doms) for v in V]; print(f"| **All** | **{h}** | "+" | ".join(str(n) if n else "." for n in row)+f" | {sum(row)} |")
row=[sum(c[(d,h,v)] for d in doms for h in hs) for v in V]; print("| **All** | **all** | "+" | ".join(str(n) for n in row)+f" | {sum(row)} |")
print(); print("anomalies:")
for x in r:
  if x['verdict']=="FUTURE-NOT-DUE" and x['horizon']!="H2": print(" FND not H2",x['ID'],x['horizon'])
  if x['horizon']=="H2" and x['verdict'] in("DEFECT-CONFIRMED","NOT-IMPLEMENTED-BUT-DUE","PARTIAL"): print(" H2 with",x['verdict'],x['ID'])
