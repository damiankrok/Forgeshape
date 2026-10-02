#!/usr/bin/env bash
# Decodes every committed `.forge` fixture through THIS tree's codec on the host
# and prints one `<fixture> <verdict>` line per file, sorted -- `Ok` or the
# codec's refusal name, and for an `Ok` CAD project whether every body
# regenerates. Developer evidence only, like `host-native-selftests.sh` (whose
# objects it links): a change that alters which fixtures open, or why one is
# refused, shows up as a one-line diff against a capture from another tree.
#
# Usage: scripts/host-forge-corpus-verdicts.sh [expected-file]
#   expected-file  optional; when given, the output must equal it exactly.
# Env:   CXX (default g++), HOST_SELFTEST_OUT (default build/host-selftests)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP="$ROOT/app/src/main/cpp"
OUT="${HOST_SELFTEST_OUT:-$ROOT/build/host-selftests}"
CXX="${CXX:-g++}"

# The domain objects and the kernel, built (incrementally) by the suite script.
bash "$ROOT/scripts/host-native-selftests.sh" __no_suite__ > /dev/null

MAIN="$OUT/forge_corpus_verdicts_main.cpp"
cat > "$MAIN" <<'CPPEOF'
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_cad_body.h"
#include "forgeshape_project_document.h"
int main(int argc, char** argv) {
    using namespace forgeshape;
    for (int i = 1; i < argc; ++i) {
        std::ifstream in(argv[i], std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
        ProjectDocument doc;
        const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &doc);
        const char* name = argv[i];
        for (const char* p = argv[i]; *p != '\0'; ++p) {
            if (*p == '/') name = p + 1;
        }
        if (why != ProjectCodecStatus::Ok) {
            std::printf("%s %s\n", name, projectCodecStatusName(why));
            continue;
        }
        int regenerated = 0;
        for (const auto& body : doc.cad.bodies) {
            CadBodyMesh mesh;
            if (regenerateCadBody(body.state, &mesh) == CadStatus::Ok) ++regenerated;
        }
        std::printf("%s Ok cad_bodies=%d regenerated=%d\n", name,
                    static_cast<int>(doc.cad.bodies.size()), regenerated);
    }
    return 0;
}
CPPEOF
OBJECTS=()
for obj in "$OUT"/obj/*.o; do
    case "$(basename "$obj")" in
        *_selftest.o) continue ;;
    esac
    OBJECTS+=("$obj")
done
"$CXX" -std=c++17 -O1 -I"$CPP" -I"$CPP/third_party/manifold/include" \
    -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM \
    "$MAIN" "${OBJECTS[@]}" "$OUT/kernel/libmanifold.a" -o "$OUT/forge_corpus_verdicts"

ACTUAL="$OUT/forge-corpus-verdicts.txt"
"$OUT/forge_corpus_verdicts" $(ls "$ROOT"/testdata/forge/v1/*.forge | sort) > "$ACTUAL"
cat "$ACTUAL"
echo "CORPUS_FIXTURES $(wc -l < "$ACTUAL")"
if [ "${1:-}" != "" ]; then
    if diff -u "$1" "$ACTUAL"; then
        echo "CORPUS_VERDICTS_UNCHANGED"
    else
        echo "CORPUS_VERDICTS_CHANGED"
        exit 1
    fi
fi
