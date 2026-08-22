<#
.SYNOPSIS
    The one supported way to run the ForgeShape instrumented (androidTest) suite.

.DESCRIPTION
    Bare `gradlew.bat :app:connectedDebugAndroidTest` enumerates every attached
    device with no default target, so on a machine with more than one
    emulator/device attached it silently installs and runs the full suite on
    ALL of them. That is exactly what happened during Stage 016 to a
    reserved device (`emulator-5554` / `Medium_Phone_API_36.1`), which
    CLAUDE.md forbids ForgeShape from touching in any way.

    This script never calls `connectedDebugAndroidTest`. It builds the debug
    app and test APKs on the host, then installs and instruments through
    `adb -s <serial>` explicitly, so exactly one device is ever touched --
    mechanically, not by convention or operator discipline.

    A serial is required. `emulator-5554` is refused before any device is
    contacted. Readiness is then checked with `adb -s <serial> get-state`, not
    a bare `adb devices` enumeration -- this script never lists or touches any
    device other than the one it was given, and it starts nothing and never
    guesses a target.

.PARAMETER Serial
    The exact adb serial to install and instrument. Required. Verify the AVD
    behind it by name (`adb -s <serial> emu avd name`), not by port or serial
    number alone -- an AVD can land on an unexpected port depending on
    whatever else is running (see CLAUDE.md, Android/Vulkan safety).

.PARAMETER TestClass
    Optional fully-qualified test class to run instead of the full suite,
    e.g. com.forgeshape.app.EditorWorkspaceLayoutTest.

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.EditorWorkspaceLayoutTest
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Serial,

    [Parameter(Mandatory = $false)]
    [string]$TestClass
)

$ErrorActionPreference = 'Stop'

# Repository device rules (CLAUDE.md, "Android / Vulkan safety"). Kept as an
# explicit denylist, checked before anything else touches adb, so a refusal
# never depends on a device actually being reachable.
$ForbiddenSerials = @('emulator-5554')

if ([string]::IsNullOrWhiteSpace($Serial)) {
    Write-Error "Usage: run-instrumented-tests.ps1 -Serial <adb-serial> [-TestClass <fully.qualified.ClassName>]`nNo default target is chosen -- an explicit -Serial is required every time."
    exit 1
}

foreach ($forbidden in $ForbiddenSerials) {
    if ($Serial -ieq $forbidden) {
        Write-Error "Refusing: '$Serial' is a repo-forbidden device (CLAUDE.md: emulator-5554 / Medium_Phone_API_36.1 is reserved by another program and must not be used, started, stopped, installed to, logged, screenshotted or sent input by ForgeShape work). No adb command has been issued against it."
        exit 1
    }
}

Write-Output "Checking that '$Serial' is currently attached..."
# Deliberately `adb -s $Serial get-state`, not a bare `adb devices` enumeration:
# the latter lists (and, for later commands relying on a default target, can
# touch) every attached device, which is the exact class of mistake this
# script exists to make impossible. get-state talks to exactly one serial.
$state = & adb -s $Serial get-state 2>$null
if ($LASTEXITCODE -ne 0 -or $state -ne 'device') {
    Write-Error "Refusing: 'adb -s $Serial get-state' did not report a ready device (got '$state'). This script starts nothing and never falls back to whatever else is attached."
    exit 1
}
Write-Output "OK: '$Serial' is attached."

# --- Build (host only, no device involved) ---------------------------------
Write-Output "Building debug app and androidTest APKs..."
& .\gradlew.bat ":app:assembleDebug" ":app:assembleDebugAndroidTest"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed; nothing was installed on '$Serial'."
    exit $LASTEXITCODE
}

$appApk = "app\build\outputs\apk\debug\app-debug.apk"
$testApk = "app\build\outputs\apk\androidTest\debug\app-debug-androidTest.apk"
if (-not (Test-Path $appApk)) { Write-Error "Missing $appApk after build."; exit 1 }
if (-not (Test-Path $testApk)) { Write-Error "Missing $testApk after build."; exit 1 }

# --- Install and instrument, every adb call explicitly scoped --------------
Write-Output "Installing app APK on -s $Serial ..."
& adb -s $Serial install -r $appApk
if ($LASTEXITCODE -ne 0) { Write-Error "Install of app APK failed on '$Serial'."; exit $LASTEXITCODE }

Write-Output "Installing test APK on -s $Serial ..."
& adb -s $Serial install -r $testApk
if ($LASTEXITCODE -ne 0) { Write-Error "Install of test APK failed on '$Serial'."; exit $LASTEXITCODE }

$instrumentTarget = "com.forgeshape.app.test/androidx.test.runner.AndroidJUnitRunner"
$instrumentArgs = @('-s', $Serial, 'shell', 'am', 'instrument', '-w')
if ($TestClass) {
    $instrumentArgs += @('-e', 'class', $TestClass)
}
$instrumentArgs += $instrumentTarget

Write-Output "Running instrumentation on -s $Serial only: adb $($instrumentArgs -join ' ')"
& adb @instrumentArgs
$testExit = $LASTEXITCODE

Write-Output "Uninstalling test APK from -s $Serial (matches connectedDebugAndroidTest's own cleanup behaviour)..."
& adb -s $Serial uninstall com.forgeshape.app.test | Out-Null

if ($testExit -ne 0) {
    Write-Error "Instrumentation reported failures on '$Serial'. See the am instrument output above for FAILURES: counts."
}
exit $testExit
