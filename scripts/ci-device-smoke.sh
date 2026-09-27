#!/usr/bin/env bash
# CI-only DEVICE signal: a fresh headless API 36 x86_64 emulator on a
# GitHub-hosted Linux VM, ForgeShape's own startup evidence, and ONE focused
# instrumentation class.
#
# This is the cloud counterpart of start-forgeshape-emulator.ps1 plus a
# focused `run-instrumented-tests.ps1 -TestClass` run, held to the same device
# rules:
#   - port 5554 / serial emulator-5554 is refused before anything starts;
#   - the AVD is created fresh in the job and booted on ONE explicit port;
#   - its identity is confirmed with `emu avd name` rather than trusted from
#     the port;
#   - every adb call carries `-s "$SERIAL"` (verify-device-guards.ps1 DEV3
#     scans this file like every other scripts/*.sh);
#   - no connected-device Gradle task is used: the APKs are built by Gradle and
#     installed and instrumented through the scoped adb calls below.
#
# Nothing here weakens a renderer requirement, mocks Vulkan or substitutes a
# native-only run for the device: if the emulator cannot give ForgeShape a
# usable Vulkan path, the result says so by name.
#
# Inputs (environment): ANDROID_HOME, plus the optional FORGESHAPE_CI_* below.
# Expects app-debug.apk and app-debug-androidTest.apk already built.
# Output: $FORGESHAPE_CI_OUT (default ci-device-evidence/) with summary.json,
# the startup and test logcat, the raw instrumentation output, a JUnit XML
# report, the emulator boot log, the environment manifest and a screenshot.
set -uo pipefail

PORT="${FORGESHAPE_CI_PORT:-5580}"
AVD="${FORGESHAPE_CI_AVD:-ForgeShape_CI_API36}"
SYSTEM_IMAGE="${FORGESHAPE_CI_SYSTEM_IMAGE:-system-images;android-36;google_apis;x86_64}"
DEVICE_PROFILE="${FORGESHAPE_CI_DEVICE_PROFILE:-medium_phone}"
GPU_MODE="${FORGESHAPE_CI_GPU:-swiftshader_indirect}"
TEST_CLASS="${FORGESHAPE_CI_TEST_CLASS:-com.forgeshape.app.Ui3dStateCorrectionTest}"
OUT="${FORGESHAPE_CI_OUT:-ci-device-evidence}"
BOOT_TIMEOUT_S="${FORGESHAPE_CI_BOOT_TIMEOUT_S:-900}"
STARTUP_TIMEOUT_S="${FORGESHAPE_CI_STARTUP_TIMEOUT_S:-600}"
TEST_TIMEOUT_S="${FORGESHAPE_CI_TEST_TIMEOUT_S:-2700}"

APP_ID="com.forgeshape.app"
ACTIVITY="$APP_ID/.ForgeShapeActivity"
RUNNER="$APP_ID.test/androidx.test.runner.AndroidJUnitRunner"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
TEST_APK="$ROOT/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk"

# The twenty-two startup suites, in emission order (CLAUDE.md, README.md).
EXPECTED_TOKENS="FORGESHAPE_CAMERA_SELFTEST_OK
FORGESHAPE_PICKING_SELFTEST_OK
FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK
FORGESHAPE_CONE_CAPSULE_SELFTEST_OK
FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK
FORGESHAPE_RENDER_SHADING_SELFTEST_OK
FORGESHAPE_SCENE_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK
FORGESHAPE_GIZMO_SELFTEST_OK
FORGESHAPE_PROJECT_SELFTEST_OK
FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK
FORGESHAPE_GLTF_EXPORT_SELFTEST_OK
FORGESHAPE_GLTF_IMPORT_SELFTEST_OK
FORGESHAPE_CAD_SELFTEST_OK
FORGESHAPE_CAD_A3_SELFTEST_OK
FORGESHAPE_SKETCH_UX_SELFTEST_OK
FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK
FORGESHAPE_MIRROR_SELFTEST_OK"
# Failure vocabulary. Never a bare FAIL: passing check NAMES contain "fails".
FAIL_PATTERN='_SELFTEST_FAIL|_FAIL:'
# Every native and Java log line ForgeShape writes carries the one tag
# "ForgeShape" (FS_TAG). Tokens and failures are read from those lines only, so
# an unrelated system line that happens to contain "_FAIL:" is never mistaken
# for a ForgeShape failure. `-v threadtime` prints "<level> ForgeShape: ...".
TAG_PATTERN=' [VDIWEF] ForgeShape *:'

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

# ---------------------------------------------------------------------------
# Result bookkeeping. summary.json is written on EVERY exit path.
# ---------------------------------------------------------------------------
RESULT="RUNNING"
DETAIL=""
PHASE="preflight"
BOOT_SECONDS=""
STARTUP_SECONDS=""
TEST_SECONDS=""
TOKENS_FOUND=0
TOKENS_IN_ORDER=false
VIEWPORT_OK=false
FAIL_LINES=0
TESTS_RUN=0
TESTS_FAILED=0
AVD_NAME_CONFIRMED=""
EMU_PID=""

json_escape() { python3 -c 'import json,sys; print(json.dumps(sys.stdin.read().rstrip("\n")))'; }

write_summary() {
    {
        printf '{\n'
        printf '  "result": %s,\n' "$(printf '%s' "$RESULT" | json_escape)"
        printf '  "detail": %s,\n' "$(printf '%s' "$DETAIL" | json_escape)"
        printf '  "phase": "%s",\n' "$PHASE"
        printf '  "serial": "%s",\n' "$SERIAL"
        printf '  "avd": "%s",\n' "$AVD"
        printf '  "avd_name_confirmed": %s,\n' "$(printf '%s' "$AVD_NAME_CONFIRMED" | json_escape)"
        printf '  "system_image": "%s",\n' "$SYSTEM_IMAGE"
        printf '  "device_profile": "%s",\n' "$DEVICE_PROFILE"
        printf '  "gpu_mode": "%s",\n' "$GPU_MODE"
        printf '  "boot_seconds": "%s",\n' "$BOOT_SECONDS"
        printf '  "startup_seconds": "%s",\n' "$STARTUP_SECONDS"
        printf '  "selftest_tokens_expected": 22,\n'
        printf '  "selftest_tokens_found": %s,\n' "$TOKENS_FOUND"
        printf '  "selftest_tokens_in_order": %s,\n' "$TOKENS_IN_ORDER"
        printf '  "native_viewport_ok": %s,\n' "$VIEWPORT_OK"
        printf '  "failure_token_lines": %s,\n' "$FAIL_LINES"
        printf '  "test_class": "%s",\n' "$TEST_CLASS"
        printf '  "tests_run": %s,\n' "$TESTS_RUN"
        printf '  "tests_failed": %s,\n' "$TESTS_FAILED"
        printf '  "test_seconds": "%s"\n' "$TEST_SECONDS"
        printf '}\n'
    } > "$OUT/summary.json"
    echo "---- summary.json ----"
    cat "$OUT/summary.json"
}

finish() {
    local code=$1
    if [ -n "$EMU_PID" ] && kill -0 "$EMU_PID" 2>/dev/null; then
        adb -s "$SERIAL" logcat -d -v threadtime > "$OUT/logcat-final.txt" 2>/dev/null || true
        adb -s "$SERIAL" emu kill > /dev/null 2>&1 || true
    fi
    write_summary
    echo "FORGESHAPE_CI_DEVICE_RESULT=$RESULT"
    exit "$code"
}

fail_with() {
    RESULT="$1"
    DETAIL="$2"
    echo "::error::$RESULT: $DETAIL"
    finish 1
}

# ---------------------------------------------------------------------------
# 1. Port and serial guard, before any process is started.
# ---------------------------------------------------------------------------
SERIAL="emulator-$PORT"
if [ "$PORT" = "5554" ] || [ "$SERIAL" = "emulator-5554" ]; then
    SERIAL="refused"
    fail_with "REFUSED" "port 5554 / emulator-5554 is repo-forbidden; nothing was started"
fi
case "$PORT" in
    ''|*[!0-9]*) fail_with "REFUSED" "port '$PORT' is not a number" ;;
esac
if [ $((PORT % 2)) -ne 0 ]; then
    fail_with "REFUSED" "emulator console port must be even, got $PORT"
fi
if ss -ltn 2>/dev/null | awk '{print $4}' | grep -qE "[:.]($PORT|$((PORT + 1)))\$"; then
    fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "port $PORT or $((PORT + 1)) already in use; no alternate port is chosen"
fi
for apk in "$APP_APK" "$TEST_APK"; do
    [ -f "$apk" ] || fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "missing $apk (build it first)"
done

EMULATOR="$ANDROID_HOME/emulator/emulator"
AVDMANAGER="$ANDROID_HOME/cmdline-tools/latest/bin/avdmanager"
[ -x "$EMULATOR" ] || fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "emulator not installed at $EMULATOR"
[ -x "$AVDMANAGER" ] || fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "avdmanager not installed at $AVDMANAGER"

# ---------------------------------------------------------------------------
# 2. A fresh AVD, created in this job.
# ---------------------------------------------------------------------------
PHASE="avd_create"
# ONE AVD home for both tools. avdmanager honours XDG_CONFIG_HOME (set on
# GitHub-hosted runners) and would write under ~/.config/.android/avd, while the
# emulator looks in ~/.android/avd and exits at once with "Unknown AVD name".
export ANDROID_AVD_HOME="${ANDROID_AVD_HOME:-$HOME/.android/avd}"
mkdir -p "$ANDROID_AVD_HOME"
if ! echo no | "$AVDMANAGER" create avd --force -n "$AVD" -k "$SYSTEM_IMAGE" -d "$DEVICE_PROFILE" \
        > "$OUT/avd-create.log" 2>&1; then
    fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "avdmanager could not create $AVD from $SYSTEM_IMAGE ($DEVICE_PROFILE)"
fi
if [ ! -f "$ANDROID_AVD_HOME/$AVD.ini" ] || [ ! -f "$ANDROID_AVD_HOME/$AVD.avd/config.ini" ]; then
    fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "avdmanager reported success but $ANDROID_AVD_HOME/$AVD.ini is missing"
fi
cp "$ANDROID_AVD_HOME/$AVD.avd/config.ini" "$OUT/avd-config.ini"

# ---------------------------------------------------------------------------
# 3. Boot headless on the explicit port. No window, no audio, no snapshot.
# ---------------------------------------------------------------------------
PHASE="boot"
EMULATOR_ARGS=(-avd "$AVD" -port "$PORT" -no-window -no-audio -no-boot-anim
               -no-snapshot -wipe-data -gpu "$GPU_MODE" -accel on
               -camera-back none -camera-front none -memory 4096 -cores 4)
echo "$EMULATOR ${EMULATOR_ARGS[*]}" > "$OUT/emulator-command.txt"
echo "Emulator command: $(cat "$OUT/emulator-command.txt")"
"$EMULATOR" -accel-check > "$OUT/accel-check.txt" 2>&1 || true
"$EMULATOR" -version 2>&1 | head -n 3 > "$OUT/emulator-version.txt" || true

boot_start=$(date +%s)
nohup "$EMULATOR" "${EMULATOR_ARGS[@]}" > "$OUT/emulator-boot.log" 2>&1 &
EMU_PID=$!

# Poll rather than block, so an emulator that exits is reported at once with
# its own reason instead of being waited on for the whole budget.
booted=""
while [ $(( $(date +%s) - boot_start )) -lt "$BOOT_TIMEOUT_S" ]; do
    if ! kill -0 "$EMU_PID" 2>/dev/null; then
        fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "emulator process exited during boot: $(grep -E 'ERROR|FATAL' "$OUT/emulator-boot.log" | head -n 2 | tr '\n' ' ')"
    fi
    if [ "$(adb -s "$SERIAL" get-state 2>/dev/null | tr -d '\r')" = "device" ]; then
        booted=$(adb -s "$SERIAL" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')
        [ "$booted" = "1" ] && break
    fi
    sleep 5
done
[ "$booted" = "1" ] || fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "sys.boot_completed not reached within ${BOOT_TIMEOUT_S}s"
BOOT_SECONDS=$(( $(date +%s) - boot_start ))
echo "Booted in ${BOOT_SECONDS}s"

# Identity, never the port: the AVD behind the serial must be the one created here.
AVD_NAME_CONFIRMED=$(adb -s "$SERIAL" emu avd name 2>/dev/null | head -n 1 | tr -d '\r')
if [ "$AVD_NAME_CONFIRMED" != "$AVD" ]; then
    fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "$SERIAL reports AVD '$AVD_NAME_CONFIRMED', expected '$AVD'"
fi

# The startup suites emit several hundred lines in milliseconds; 64M is the
# size CLAUDE.md requires before startup evidence is captured.
adb -s "$SERIAL" logcat -G 64M > /dev/null 2>&1 || true
adb -s "$SERIAL" logcat -g > "$OUT/logcat-buffer-size.txt" 2>&1 || true

# ---------------------------------------------------------------------------
# 4. Environment manifest: what the emulator actually offers.
# ---------------------------------------------------------------------------
PHASE="manifest"
{
    echo "serial=$SERIAL"
    echo "avd=$AVD (confirmed by emu avd name: $AVD_NAME_CONFIRMED)"
    echo "system_image=$SYSTEM_IMAGE"
    echo "device_profile=$DEVICE_PROFILE"
    echo "gpu_mode=$GPU_MODE"
    echo "emulator_version=$(head -n 1 "$OUT/emulator-version.txt" 2>/dev/null)"
    echo "kvm=$(ls -l /dev/kvm 2>&1)"
    echo "accel_check=$(tr '\n' ' ' < "$OUT/accel-check.txt")"
    for prop in ro.build.fingerprint ro.build.version.sdk ro.build.version.release \
                ro.product.cpu.abi ro.kernel.qemu ro.boot.qemu.gltransport.name \
                ro.hardware.egl ro.hardware.vulkan ro.opengles.version \
                ro.boot.hardware.vulkan ro.boot.qemu.vsync ro.sf.lcd_density; do
        echo "$prop=$(adb -s "$SERIAL" shell getprop "$prop" 2>/dev/null | tr -d '\r')"
    done
    echo "wm_size=$(adb -s "$SERIAL" shell wm size 2>/dev/null | tr -d '\r' | tr '\n' ' ')"
    echo "wm_density=$(adb -s "$SERIAL" shell wm density 2>/dev/null | tr -d '\r' | tr '\n' ' ')"
    echo "vulkan_features:"
    adb -s "$SERIAL" shell pm list features 2>/dev/null | tr -d '\r' | grep -i vulkan || echo "  (none reported)"
    echo "emulator_boot_log_gpu_lines:"
    grep -iE 'vulkan|gpu|swiftshader|gfxstream|renderer' "$OUT/emulator-boot.log" | head -n 40 || true
} > "$OUT/environment-manifest.txt"
cat "$OUT/environment-manifest.txt"

# ---------------------------------------------------------------------------
# 5. Startup evidence: install, launch, wait for the first presented frame.
# ---------------------------------------------------------------------------
PHASE="startup"
adb -s "$SERIAL" install -r "$APP_APK" > "$OUT/install-app.log" 2>&1 \
    || fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "app install failed: $(tail -n 1 "$OUT/install-app.log")"
adb -s "$SERIAL" logcat -c > /dev/null 2>&1 || true
startup_start=$(date +%s)
adb -s "$SERIAL" shell am start -W -n "$ACTIVITY" > "$OUT/am-start.log" 2>&1 || true

settled=""
while [ $(( $(date +%s) - startup_start )) -lt "$STARTUP_TIMEOUT_S" ]; do
    adb -s "$SERIAL" logcat -d -v threadtime > "$OUT/startup-logcat.txt" 2>/dev/null || true
    grep -E "$TAG_PATTERN" "$OUT/startup-logcat.txt" > "$OUT/startup-forgeshape.txt" || true
    if grep -q 'FORGESHAPE_NATIVE_VIEWPORT_OK' "$OUT/startup-forgeshape.txt"; then settled="ok"; break; fi
    if grep -qE 'FORGESHAPE_NATIVE_VIEWPORT_FAIL:' "$OUT/startup-forgeshape.txt"; then settled="viewport_fail"; break; fi
    if grep -qE "$FAIL_PATTERN" "$OUT/startup-forgeshape.txt" \
        && ! adb -s "$SERIAL" shell pidof "$APP_ID" > /dev/null 2>&1; then settled="died"; break; fi
    sleep 5
done
STARTUP_SECONDS=$(( $(date +%s) - startup_start ))
# One more read so lines emitted after the verdict line are kept too.
sleep 3
adb -s "$SERIAL" logcat -d -v threadtime > "$OUT/startup-logcat.txt" 2>/dev/null || true
adb -s "$SERIAL" exec-out screencap -p > "$OUT/startup-screenshot.png" 2>/dev/null || true
grep -E "$TAG_PATTERN" "$OUT/startup-logcat.txt" > "$OUT/startup-forgeshape.txt" || true

# Vulkan evidence as ForgeShape itself reports it.
grep -iE 'Vulkan|Physical device selected|Queue families|swapchain' "$OUT/startup-forgeshape.txt" \
    | head -n 40 > "$OUT/vulkan-evidence.txt" || true
{
    echo "vulkan_lines_from_forgeshape:"
    sed 's/^/  /' "$OUT/vulkan-evidence.txt"
} >> "$OUT/environment-manifest.txt"

# The twenty-two tokens, in order, then the viewport.
grep -oE 'FORGESHAPE_[A-Z0-9_]+_SELFTEST_OK' "$OUT/startup-forgeshape.txt" | awk '!seen[$0]++' > "$OUT/selftest-tokens-found.txt"
TOKENS_FOUND=$(grep -cxFf <(printf '%s\n' "$EXPECTED_TOKENS") "$OUT/selftest-tokens-found.txt" || true)
if [ "$(cat "$OUT/selftest-tokens-found.txt")" = "$EXPECTED_TOKENS" ]; then TOKENS_IN_ORDER=true; fi
if grep -q 'FORGESHAPE_NATIVE_VIEWPORT_OK' "$OUT/startup-forgeshape.txt"; then VIEWPORT_OK=true; fi
FAIL_LINES=$(grep -cE "$FAIL_PATTERN" "$OUT/startup-forgeshape.txt" || true)
grep -E "$FAIL_PATTERN" "$OUT/startup-forgeshape.txt" > "$OUT/startup-failure-lines.txt" || true
echo "Startup: tokens=$TOKENS_FOUND/22 in_order=$TOKENS_IN_ORDER viewport_ok=$VIEWPORT_OK failure_lines=$FAIL_LINES (${STARTUP_SECONDS}s)"

if [ "$FAIL_LINES" -gt 0 ] && grep -qE '_SELFTEST_FAIL|_CASE_FAIL:' "$OUT/startup-forgeshape.txt"; then
    fail_with "FAIL-CI-CLOUD-DEVICE-PRODUCT" "a startup self-test failed: $(head -n 1 "$OUT/startup-failure-lines.txt")"
fi
if [ "$VIEWPORT_OK" != true ]; then
    if [ "$settled" = "viewport_fail" ] || ! grep -q 'android.hardware.vulkan' "$OUT/environment-manifest.txt"; then
        fail_with "BLOCKED-CI-CLOUD-DEVICE-CAPABILITY" "no usable Vulkan path: $(grep -m1 -oE 'FORGESHAPE_NATIVE_VIEWPORT_FAIL:[^ ]*' "$OUT/startup-forgeshape.txt" || echo 'viewport never reported OK')"
    fi
    fail_with "DEVICE_STARTUP_UNRESOLVED" "viewport never reported OK within ${STARTUP_TIMEOUT_S}s (settled=$settled); inspect startup-logcat.txt"
fi
if [ "$FAIL_LINES" -gt 0 ]; then
    fail_with "FAIL-CI-CLOUD-DEVICE-PRODUCT" "failure token after startup: $(head -n 1 "$OUT/startup-failure-lines.txt")"
fi
if [ "$TOKENS_FOUND" -ne 22 ] || [ "$TOKENS_IN_ORDER" != true ]; then
    fail_with "DEVICE_STARTUP_UNRESOLVED" "expected 22 self-test tokens in order, found $TOKENS_FOUND (in_order=$TOKENS_IN_ORDER) with zero failures — treat as a dropped capture until proven otherwise"
fi
adb -s "$SERIAL" shell am force-stop "$APP_ID" > /dev/null 2>&1 || true

# ---------------------------------------------------------------------------
# 6. ONE focused instrumentation class. Never the full suite by default.
# ---------------------------------------------------------------------------
PHASE="instrumentation"
adb -s "$SERIAL" install -r "$TEST_APK" > "$OUT/install-test.log" 2>&1 \
    || fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "test APK install failed: $(tail -n 1 "$OUT/install-test.log")"
adb -s "$SERIAL" logcat -c > /dev/null 2>&1 || true
echo "MODE=FOCUSED_SUBSET (not full-suite evidence): $TEST_CLASS on $SERIAL"
test_start=$(date +%s)
timeout "$TEST_TIMEOUT_S" adb -s "$SERIAL" shell am instrument -w -r -e class "$TEST_CLASS" "$RUNNER" \
    > "$OUT/instrumentation-raw.txt" 2>&1
instrument_exit=$?
TEST_SECONDS=$(( $(date +%s) - test_start ))
adb -s "$SERIAL" logcat -d -v threadtime > "$OUT/test-logcat.txt" 2>/dev/null || true
adb -s "$SERIAL" exec-out screencap -p > "$OUT/post-test-screenshot.png" 2>/dev/null || true

# Raw `am instrument -r` status blocks -> JUnit XML and counts.
python3 - "$OUT/instrumentation-raw.txt" "$OUT/instrumentation-junit.xml" "$OUT/instrumentation-counts.txt" <<'PY'
import sys
from xml.sax.saxutils import escape, quoteattr
raw, junit, counts = sys.argv[1:4]
# Status codes: 1 start, 0 pass, -1 error, -2 failure, -3 ignored, -4 assumption.
tests, cur, last, final_code = [], {}, None, "none"
for line in open(raw, errors="replace").read().replace("\r", "").split("\n"):
    if line.startswith("INSTRUMENTATION_STATUS: "):
        key, _, value = line[len("INSTRUMENTATION_STATUS: "):].partition("=")
        cur[key] = value
        last = key
    elif line.startswith("INSTRUMENTATION_STATUS_CODE: "):
        code = int(line.split(": ", 1)[1])
        if code != 1 and "test" in cur:
            tests.append((cur.get("class", ""), cur["test"], code, cur.get("stack", "")))
        cur, last = {}, None
    elif line.startswith("INSTRUMENTATION_CODE: "):
        final_code = line.split(": ", 1)[1]
    elif line.startswith("INSTRUMENTATION_"):
        last = None
    elif last is not None:
        cur[last] += "\n" + line  # a multi-line value (a stack trace) continues
failed = [t for t in tests if t[2] in (-1, -2)]
with open(junit, "w") as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
    f.write('<testsuite name=%s tests="%d" failures="%d">\n'
            % (quoteattr(tests[0][0] if tests else "instrumentation"), len(tests), len(failed)))
    for cls, name, code, stack in tests:
        f.write('  <testcase classname=%s name=%s>' % (quoteattr(cls), quoteattr(name)))
        if code in (-1, -2):
            f.write('<failure message="status %d">%s</failure>' % (code, escape(stack)))
        elif code in (-3, -4):
            f.write('<skipped/>')
        f.write('</testcase>\n')
    f.write('</testsuite>\n')
with open(counts, "w") as f:
    f.write("run=%d failed=%d final_code=%s\n" % (len(tests), len(failed), final_code))
PY
read -r TESTS_RUN TESTS_FAILED FINAL_CODE < <(sed -E 's/run=([0-9]+) failed=([0-9]+) final_code=(.*)/\1 \2 \3/' "$OUT/instrumentation-counts.txt")
echo "Instrumentation: run=$TESTS_RUN failed=$TESTS_FAILED final_code=$FINAL_CODE exit=$instrument_exit (${TEST_SECONDS}s)"
grep -E '^(OK \(|FAILURES!!!|Tests run:)' "$OUT/instrumentation-raw.txt" || true

if [ "$instrument_exit" -eq 124 ]; then
    fail_with "DEVICE_INFRASTRUCTURE_FAILURE" "instrumentation exceeded ${TEST_TIMEOUT_S}s"
fi
if grep -qE 'INSTRUMENTATION_FAILED|Process crashed|shortMsg=' "$OUT/instrumentation-raw.txt"; then
    fail_with "FAIL-CI-CLOUD-DEVICE-PRODUCT" "instrumentation aborted: $(grep -m1 -E 'INSTRUMENTATION_FAILED|Process crashed|shortMsg=' "$OUT/instrumentation-raw.txt")"
fi
if [ "$TESTS_FAILED" -gt 0 ]; then
    fail_with "FAIL-CI-CLOUD-DEVICE-PRODUCT" "$TESTS_FAILED of $TESTS_RUN test(s) in $TEST_CLASS failed after ForgeShape initialised correctly"
fi
if [ "$TESTS_RUN" -eq 0 ] || ! grep -qE "^OK \($TESTS_RUN tests?\)" "$OUT/instrumentation-raw.txt"; then
    fail_with "DEVICE_STARTUP_UNRESOLVED" "no 'OK ($TESTS_RUN tests)' result for $TEST_CLASS; inspect instrumentation-raw.txt"
fi

PHASE="done"
RESULT="PASS"
DETAIL="22/22 startup tokens in order, NATIVE_VIEWPORT_OK, 0 failure tokens; $TEST_CLASS OK ($TESTS_RUN tests)"
finish 0
