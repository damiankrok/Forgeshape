<#
.SYNOPSIS
    GLB-IMPORT-R1: runs the external-GLB preview suite on the isolated AVD and
    pulls the Nomad-like fixture fingerprint and screenshot off the device.

.DESCRIPTION
    R1 widened the diagnostic reader to the class of static GLB another
    sculpting tool writes. The owner's own low-poly character is external to
    this repository, so what the suite exercises is a deterministic synthetic
    file carrying the same structural features — a node matrix, seven TRIANGLES
    primitives over one shared POSITION accessor, no NORMAL, ignored colour and
    UV attributes, a double-sided material.

    The fingerprint this pulls back is a hash of THAT file and never of the
    owner's. It is meaningful because every byte of the fixture is an integer
    over a power of two, so the same build produces the same bytes anywhere.

    The reports are produced by ordinary cases in the same suite, so what lands
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
    Where the pulled files are written. Defaults to artifacts\glb-import-r1.
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
$testClass = 'com.forgeshape.app.GlbImportExternalR1Test'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root 'artifacts\glb-import-r1'
}

function Stop-Run {
    param([string] $Message)
    Write-Output "GLB_IMPORT_R1_EVIDENCE=FAIL"
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

$reports = @('nomad_like_fixture.txt')
$shots = @('nomad_like_preview.png')
$deviceDirectory = "/sdcard/Android/data/$package/files"

# Remove anything from a previous run first, so a pulled file can never be a
# stale one this run did not produce.
foreach ($name in ($reports + $shots)) {
    & adb -s $Serial shell rm -f "$deviceDirectory/$name" | Out-Null
}

Write-Output "Running $testClass ..."
& $runner -Serial $Serial -TestClass $testClass
if ($LASTEXITCODE -ne 0) {
    Stop-Run "The external-GLB suite did not pass on '$Serial'. No evidence is produced from a failing run."
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
    if ($text -notmatch 'sha256=[0-9a-f]{64}') {
        Stop-Run "'$name' carries no fixture digest."
    }
    $digest = ([regex]::Match($text, 'sha256=([0-9a-f]{64})')).Groups[1].Value
    Write-Output ("{0}: sha256={1}" -f $name, $digest)
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

Write-Output "GLB_IMPORT_R1_EVIDENCE=PASS"
