<#
.SYNOPSIS
    GLB-IMPORT-R0: runs the import/roundtrip suite on the isolated AVD and
    pulls the world-geometry comparison reports off the device.

.DESCRIPTION
    The diagnostic's answer is a NUMBER — the largest world-space disagreement
    between the ForgeShape scene and the same scene decoded out of the `.glb` by
    a reader that shares nothing with the writer. This script runs the suite that
    produces it and brings the reports back into artifacts\glb-import-r0\.

    The reports are produced by ordinary cases in that same suite, so what lands
    in the bundle is what the assertions were made about rather than a separate
    path written to make evidence. Nothing is produced from a failing run.

    Every device call carries an explicit -Serial. The reserved emulator-5554 is
    refused before any adb command is issued against it, and the AVD's own name
    is confirmed rather than inferred from its port.

.PARAMETER Serial
    The adb serial of the ForgeShape-owned isolated AVD. Required, always.

.PARAMETER ExpectedAvd
    The AVD name that serial must report. Defaults to the isolated one.

.PARAMETER OutputDirectory
    Where the pulled reports are written. Defaults to artifacts\glb-import-r0.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Serial,
    [string] $ExpectedAvd = 'ForgeShape_Stage006',
    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$runner = Join-Path $root 'scripts\run-instrumented-tests.ps1'
$package = 'com.forgeshape.app'
$testClass = 'com.forgeshape.app.GlbImportPreviewTest'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root 'artifacts\glb-import-r0'
}

function Stop-Run {
    param([string] $Message)
    Write-Output "GLB_IMPORT_EVIDENCE=FAIL"
    Write-Error -Message $Message -ErrorAction Continue
    exit 1
}

# The forbidden serial is refused BEFORE any adb command is issued against it.
if ($Serial -ieq 'emulator-5554') {
    Stop-Run "Refusing: 'emulator-5554' is reserved by another program (CLAUDE.md). No adb command has been issued against it."
}

$state = & adb -s $Serial get-state 2>$null
if ($LASTEXITCODE -ne 0 -or $state -ne 'device') {
    Stop-Run "'adb -s $Serial get-state' did not report a ready device (got '$state'). This script starts nothing and never falls back to another target."
}

$avdLines = @(& adb -s $Serial emu avd name 2>&1 | ForEach-Object { "$_".Trim() })
$avd = ($avdLines | Where-Object { $_ -and $_ -ne 'OK' } | Select-Object -First 1)
if ($avd -ne $ExpectedAvd) {
    Stop-Run "Refusing: '$Serial' reports AVD '$avd', not the expected ForgeShape-owned '$ExpectedAvd'."
}

Write-Output "Target: $Serial ($avd)"

$reports = @('construction_roundtrip.txt', 'sculpt_roundtrip.txt', 'committed_sentinels.txt')
# The Source and Imported captures, taken by the suite from ONE pinned camera
# pose. They illustrate; the reports above are the authority.
$shots = @('source_scene.png', 'imported_preview.png')
$deviceDirectory = "/sdcard/Android/data/$package/files"

# Remove anything from a previous run first, so a pulled file can never be a
# stale one this run did not produce.
foreach ($name in ($reports + $shots)) {
    & adb -s $Serial shell rm -f "$deviceDirectory/$name" | Out-Null
}

Write-Output "Running $testClass ..."
& $runner -Serial $Serial -TestClass $testClass
if ($LASTEXITCODE -ne 0) {
    Stop-Run "The GLB import suite did not pass on '$Serial'. No evidence is produced from a failing run."
}

if (-not (Test-Path $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

foreach ($name in $reports) {
    $destination = Join-Path $OutputDirectory $name
    if (Test-Path $destination) { Remove-Item $destination -Force }
    & adb -s $Serial pull "$deviceDirectory/$name" $destination | Out-Null
    if (-not (Test-Path $destination)) {
        Stop-Run "The suite passed but '$name' was not on the device. Evidence must come from the run, never from a previous one."
    }
    $text = Get-Content -Path $destination -Raw
    if ($text -notmatch 'verdict=ROUNDTRIP_') {
        Stop-Run "'$name' carries no verdict line."
    }
    $verdicts = ([regex]::Matches($text, 'verdict=(\S+)') | ForEach-Object { $_.Groups[1].Value })
    Write-Output ("{0}: {1}" -f $name, ($verdicts -join ', '))
}

foreach ($name in $shots) {
    $destination = Join-Path $OutputDirectory $name
    if (Test-Path $destination) { Remove-Item $destination -Force }
    & adb -s $Serial pull "$deviceDirectory/$name" $destination | Out-Null
    if (-not (Test-Path $destination)) {
        Stop-Run "The suite passed but '$name' was not on the device."
    }
    Write-Output ("{0}: {1} bytes" -f $name, (Get-Item $destination).Length)
}

Write-Output "GLB_IMPORT_EVIDENCE=PASS"
