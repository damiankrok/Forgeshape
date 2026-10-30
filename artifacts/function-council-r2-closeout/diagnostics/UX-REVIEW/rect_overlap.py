# Computes overlaps between recorded view rects (px, from facts.txt files of
# CI DEVICE run 40797031704, emulator 1080x2400 @ 420 dpi -> 2.625 px/dp).
# Inputs are copied verbatim from:
#   artifacts/function-council-r2/evidence/device-run-40797031704/screens/cad-a3-c2-sketch-ux-r1/facts.txt
#   artifacts/function-council-r2/evidence/device-run-40797031704/screens/sel-out-r1/facts.txt
#   artifacts/function-council-r2/evidence/device-run-40797031704/screens/ui-pref-r1/facts.txt
# The tool rail's x extent is taken from tool_rail_arc/tool_rail_spline (x 896..1043).
DP = 2.625
R = {
 "navigator":          (442,336,859,691),   # capture 04 and 11
 "navigator_flip":     (603,544,698,670),
 "dimension_label":    (800,583,914,709),   # capture 07
 "dimension_editor":   (627,572,1080,720),  # capture 08
 "dimension_field":    (638,583,890,709),   # capture 08
 "tool_rail_column":   (896,1112,1043,1453),# arc..spline entries (x extent only is used)
 "selection_outline_on": (724,1025,821,1151),
 "selection_outline_off":(832,1025,935,1151),
 "settings_back":      (74,3409,1006,3535),
 "finish_sketch":      (32,74,327,199),
 "home_settings":      (74,1041,1006,1167),
}
def area(r): return max(0,r[2]-r[0])*max(0,r[3]-r[1])
def inter(a,b):
    x0,y0=max(a[0],b[0]),max(a[1],b[1]); x1,y1=min(a[2],b[2]),min(a[3],b[3])
    return (x0,y0,x1,y1) if x1>x0 and y1>y0 else None
print("== sizes (px -> dp)")
for k,r in R.items():
    w,h=r[2]-r[0],r[3]-r[1]
    print(f"{k}: {w}x{h} px = {w/DP:.1f}x{h/DP:.1f} dp  floor48={'OK' if w/DP>=48 and h/DP>=48 else 'BELOW'}")
print("== overlaps")
def ov(a,b):
    i=inter(R[a],R[b])
    if not i: print(f"{a} x {b}: none"); return
    print(f"{a} x {b}: rect {i} = {i[2]-i[0]}x{i[3]-i[1]} px, {100*area(i)/area(R[a]):.1f}% of {a}")
ov("dimension_label","navigator")
ov("dimension_field","navigator")
ov("dimension_editor","navigator")
# rail: only the x extent is known; test the x-span against the editor/label
for k in ("dimension_label","dimension_editor"):
    r=R[k]; x0=max(r[0],896); x1=min(r[2],1043)
    print(f"{k} x tool_rail x-span 896..1043: {max(0,x1-x0)} px of {r[2]-r[0]} px width")
cx=(R['dimension_field'][0]+R['dimension_field'][2])/2; cy=(R['dimension_field'][1]+R['dimension_field'][3])/2
n=R['navigator']
print(f"dimension_field centre ({cx},{cy}) inside navigator rect: {n[0]<=cx<=n[2] and n[1]<=cy<=n[3]}")
lx=(R['dimension_label'][0]+R['dimension_label'][2])/2; ly=(R['dimension_label'][1]+R['dimension_label'][3])/2
print(f"dimension_label centre ({lx},{ly}) inside navigator rect: {n[0]<=lx<=n[2] and n[1]<=ly<=n[3]}")
print(f"settings_back top y=3409 vs window height 2400: below fold by {3409-2400} px = {(3409-2400)/DP:.0f} dp")
