"""CR-01: independent reader of the .glb bytes the product exporter wrote.

For every node it prints the node's transform keys, the translation written,
the POSITION accessor's declared min/max, and the WORLD bounding box computed
from the float32 positions actually stored in the BIN chunk plus the node
translation (the exporter writes translation only; any rotation/scale/matrix
key would be reported). Standard library only; run with `python3 -I`.
"""
import json
import struct
import sys

COMP = {5126: ("f", 4)}
NCOMP = {"VEC3": 3, "SCALAR": 1, "VEC2": 2, "VEC4": 4}


def parse(path):
    data = open(path, "rb").read()
    magic, version, length = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67 and version == 2 and length == len(data), "bad GLB header"
    off = 12
    js = binc = None
    while off < len(data):
        clen, ctype = struct.unpack_from("<II", data, off)
        chunk = data[off + 8: off + 8 + clen]
        if ctype == 0x4E4F534A:
            js = json.loads(chunk.decode("utf-8"))
        elif ctype == 0x004E4942:
            binc = chunk
        off += 8 + clen
    print("== %s bytes=%d nodes=%d meshes=%d" % (path.rsplit("/", 1)[-1], len(data),
                                                 len(js.get("nodes", [])), len(js.get("meshes", []))))
    for ni, node in enumerate(js["nodes"]):
        keys = sorted(k for k in node if k not in ("mesh", "name"))
        t = node.get("translation", [0.0, 0.0, 0.0])
        mesh = js["meshes"][node["mesh"]]
        mn = [float("inf")] * 3
        mx = [float("-inf")] * 3
        decl_min = decl_max = None
        for prim in mesh["primitives"]:
            acc = js["accessors"][prim["attributes"]["POSITION"]]
            decl_min, decl_max = acc.get("min"), acc.get("max")
            bv = js["bufferViews"][acc["bufferView"]]
            base = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
            stride = bv.get("byteStride", 12)
            for i in range(acc["count"]):
                x, y, z = struct.unpack_from("<fff", binc, base + i * stride)
                for k, c in enumerate((x + t[0], y + t[1], z + t[2])):
                    mn[k] = min(mn[k], c)
                    mx[k] = max(mx[k], c)
        print("  node %d name=%s transform_keys=%s translation=(%.4f,%.4f,%.4f)" % (
            ni, node.get("name"), keys, t[0], t[1], t[2]))
        print("    POSITION declared min=%s max=%s" % (
            [round(v, 4) for v in decl_min], [round(v, 4) for v in decl_max]))
        print("    world_aabb_from_bytes=[%.4f,%.4f,%.4f]..[%.4f,%.4f,%.4f]" % (*mn, *mx))


for p in sys.argv[1:]:
    parse(p)
