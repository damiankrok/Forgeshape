<#
.SYNOPSIS
    E2E-R1C: runs the GLB export suite on the isolated AVD and pulls the two
    sentinel `.glb` files off the device.

.DESCRIPTION
    The exporter's correctness is asserted on the device, by an independent
    reader, in GlbExportTest. What this script adds is the ARTEFACT: the two
    files themselves, off the device and into artifacts\e2er1c\, so the owner
    can open them in whatever they like without reproducing a test run.

    The sentinels are produced by two ordinary cases in that same suite, so the
    files in the bundle are the files the assertions were made about — not a
    separate code path written to make evidence.

    Every device call carries an explicit -Serial. The reserved emulator-5554 is
    refused before any adb command is issued against it, and the AVD's own name
    is confirmed rather than inferred from its port, because the emulator
    assigns the first free port from 5554 to whichever instance boots first.

.PARAMETER Serial
    The adb serial of the ForgeShape-owned isolated AVD. Required, always.

.PARAMETER ExpectedAvd
    The AVD name that serial must report. Defaults to the isolated one.

.PARAMETER OutputDirectory
    Where the pulled files and the report are written. Defaults to
    artifacts\e2er1c under the repo.
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
$testClass = 'com.forgeshape.app.GlbExportTest'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root 'artifacts\e2er1c'
}

function Stop-Run {
    param([string] $Message)
    Write-Output "GLB_EXPORT_EVIDENCE=FAIL"
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

$avdLines = @(& adb -s $Serial emu avd name 2>&1 | ForEach-Object { "$_".Trim() })
$avd = ($avdLines | Where-Object { $_ -and $_ -ne 'OK' } | Select-Object -First 1)
if ($avd -ne $ExpectedAvd) {
    Stop-Run "Refusing: '$Serial' reports AVD '$avd', not the expected ForgeShape-owned '$ExpectedAvd'."
}

Write-Output "Target: $Serial ($avd)"

# Remove any sentinel from a previous run first, so a pulled file can never be
# a stale one that this run did not actually produce.
$deviceDirectory = "/sdcard/Android/data/$package/files"
foreach ($name in @('construction_sentinel.glb', 'sculpt_sentinel.glb')) {
    & adb -s $Serial shell rm -f "$deviceDirectory/$name" | Out-Null
}

Write-Output "Running $testClass ..."
& $runner -Serial $Serial -TestClass $testClass
if ($LASTEXITCODE -ne 0) {
    Stop-Run "The GLB export suite did not pass on '$Serial'. No evidence is produced from a failing run."
}

if (-not (Test-Path $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$rows = @()
foreach ($name in @('construction_sentinel.glb', 'sculpt_sentinel.glb')) {
    $destination = Join-Path $OutputDirectory $name
    if (Test-Path $destination) { Remove-Item $destination -Force }
    & adb -s $Serial pull "$deviceDirectory/$name" $destination | Out-Null
    if (-not (Test-Path $destination)) {
        Stop-Run "The suite passed but '$name' was not on the device. Evidence must come from the run, never from a previous one."
    }
    $bytes = [System.IO.File]::ReadAllBytes($destination)
    # Re-check the container here too, on the host, on the file that is actually
    # in the bundle: a pull that truncated would otherwise ship silently.
    if ($bytes.Length -lt 12) { Stop-Run "'$name' is too short to be a GLB." }
    $magic = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4)
    $version = [System.BitConverter]::ToUInt32($bytes, 4)
    $declared = [System.BitConverter]::ToUInt32($bytes, 8)
    if ($magic -ne 'glTF') { Stop-Run "'$name' does not start with the glTF magic." }
    if ($version -ne 2) { Stop-Run "'$name' declares container version $version, not 2." }
    if ($declared -ne $bytes.Length) { Stop-Run "'$name' declares $declared bytes but is $($bytes.Length)." }
    $sha = (Get-FileHash -Path $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    $rows += [pscustomobject]@{ File = $name; Bytes = $bytes.Length; Sha256 = $sha }
}

$rows | Format-Table -AutoSize | Out-String | Write-Output
Write-Output "GLB_EXPORT_EVIDENCE=PASS"
