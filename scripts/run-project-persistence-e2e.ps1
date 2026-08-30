<#
.SYNOPSIS
    E2ER1A-02/03 and E2ER1B-01/02: proves ForgeShape work survives real process
    death — both work the user saved, and work they never did.

.DESCRIPTION
    Instrumentation runs inside the app process, so a test cannot kill that
    process and keep asserting. This script is what makes the process death
    real: it runs the first half of a case, kills the app process, confirms the
    PID is actually gone, and then runs the verifying half in a process that has
    never seen the scene the first half built.

    Stages 1-4 are E2E-R1A: the user pressed Save, and Open brings it back.
    Stages 5-8 are E2E-R1B and are the harder claim: the user pressed nothing.
    Autosave alone protected the work, and the recovery question on a genuinely
    cold launch is what offers it back.

    Both halves are ordinary instrumentation methods and are also covered by the
    exhaustive suite; what this script adds, and the only thing it adds, is the
    process death between them.

    Every device call goes through scripts\run-instrumented-tests.ps1 with an
    explicit -Serial, or is an explicit `adb -s <serial>` here. There is no bare
    adb call, no device enumeration, and no default target: the reserved
    emulator-5554 is refused before any device is contacted, and the AVD's own
    name is confirmed rather than inferred from its port.

.PARAMETER Serial
    The adb serial of the ForgeShape-owned isolated AVD. Required, always.

.PARAMETER ExpectedAvd
    The AVD name that serial must report. Defaults to the isolated one.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Serial,
    [string] $ExpectedAvd = 'ForgeShape_Stage006'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$runner = Join-Path $root 'scripts\run-instrumented-tests.ps1'
$package = 'com.forgeshape.app'
$testClass = 'com.forgeshape.app.ProjectProcessDeathTest'

function Stop-Run {
    param([string] $Message)
    Write-Output "PROJECT_PERSISTENCE_E2E=FAIL"
    Write-Error -Message $Message -ErrorAction Continue
    exit 1
}

# The forbidden serial is refused BEFORE any adb command is issued against it,
# including a harmless-looking probe.
if ($Serial -ieq 'emulator-5554') {
    Stop-Run "Refusing: 'emulator-5554' is reserved by another program (CLAUDE.md). No adb command has been issued against it."
}

$state = & adb -s $Serial get-state 2>$null
if ($LASTEXITCODE -ne 0 -or $state -ne 'device') {
    Stop-Run "'adb -s $Serial get-state' did not report a ready device (got '$state'). This script starts nothing and never falls back to another target."
}

# A port never says which AVD is behind it: the emulator assigns the first free
# port from 5554 to whichever instance boots first, so identity is confirmed.
$avdLines = @(& adb -s $Serial emu avd name 2>&1 | ForEach-Object { "$_".Trim() })
$avd = ($avdLines | Where-Object { $_ -and $_ -ne 'OK' } | Select-Object -First 1)
if ($avd -ne $ExpectedAvd) {
    Stop-Run "Refusing: '$Serial' reports AVD '$avd', not the expected ForgeShape-owned '$ExpectedAvd'."
}
Write-Output "Target confirmed: $Serial is AVD '$avd'."

function Get-AppPid {
    $out = @(& adb -s $Serial shell pidof $package 2>$null | ForEach-Object { "$_".Trim() })
    $value = ($out | Where-Object { $_ } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($value)) { return $null }
    return $value
}

function Invoke-Stage {
    param([string] $Method)
    Write-Output ''
    Write-Output "=== STAGE $Method ==="
    & powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Serial $Serial -TestClass "$testClass#$Method"
    if ($LASTEXITCODE -ne 0) {
        Stop-Run "Stage '$Method' did not pass."
    }
}

# The claim being established, stated exactly: at the moment the VERIFYING stage
# begins, no ForgeShape process exists. Everything that stage then reads about
# the project therefore came off the disk and out of the codec, because there is
# nothing else left for it to have come from.
#
# The PID is observed rather than assumed. In practice the app has usually
# already exited by the time the saving stage's runner has uninstalled the test
# APK, which is a stronger form of the same fact; the force-stop below makes the
# outcome the same either way, and the absence afterwards is what is asserted.
$script:deathEvidence = @()

function Assert-NoLiveProcess {
    param([string] $Label)
    $before = Get-AppPid
    & adb -s $Serial shell am force-stop $package | Out-Null
    Start-Sleep -Milliseconds 1500
    $after = Get-AppPid
    if ($after) {
        Stop-Run "$Label : a ForgeShape process ($after) survived am force-stop; the process-death claim would be false."
    }
    $beforeText = if ($before) { "pid $before" } else { 'already exited' }
    Write-Output "$Label : before force-stop = $beforeText; after = no process. The verifying stage starts fresh."
    $script:deathEvidence += "$Label : $beforeText -> no process before the verifying stage"
}

Push-Location $root
try {
    # E2ER1A-02 -- Construction across process death.
    Invoke-Stage 'stage1_buildAndSaveAConstructionProject'
    Assert-NoLiveProcess 'E2ER1A-02'
    Invoke-Stage 'stage2_openTheConstructionProjectAfterProcessDeath'

    # E2ER1A-03 -- Sculpt across process death.
    Invoke-Stage 'stage3_sculptAndSaveASculptProject'
    Assert-NoLiveProcess 'E2ER1A-03'
    Invoke-Stage 'stage4_openTheSculptProjectAfterProcessDeath'

    # E2ER1B-01 -- Construction that was NEVER saved, recovered after death.
    Invoke-Stage 'stage5_editWithoutSavingAndLetAutosaveProtectIt'
    Assert-NoLiveProcess 'E2ER1B-01'
    Invoke-Stage 'stage6_recoverTheUnsavedConstructionWorkAfterProcessDeath'

    # E2ER1B-02 -- a real sculpt stroke that was never saved.
    Invoke-Stage 'stage7_sculptWithoutSavingAndLetAutosaveProtectIt'
    Assert-NoLiveProcess 'E2ER1B-02'
    Invoke-Stage 'stage8_recoverTheUnsavedSculptWorkAfterProcessDeath'
} finally {
    Pop-Location
}

Write-Output ''
foreach ($line in $script:deathEvidence) { Write-Output $line }
Write-Output 'PROJECT_PERSISTENCE_E2E=PASS'
exit 0
