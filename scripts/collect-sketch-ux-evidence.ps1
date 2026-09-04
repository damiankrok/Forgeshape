<#
.SYNOPSIS
    Pulls the SKETCH-UX-R1 screenshot evidence off the isolated AVD and
    builds the owner contact sheet.

.DESCRIPTION
    `SketchUxVisualEvidenceTest` captures twelve composed-display frames of
    the Home -> New Project -> flat sketch -> navigator -> dimension ->
    curves -> Edit Sketch journey and writes them, with a line of measurable
    facts per capture, into the app's external files directory. This script
    pulls it into artifacts/cad-a3-c2-sketch-ux-r1/frames/, composes
    OWNER_CONTACT_SHEET_C2.png with System.Drawing (no third-party tool), and
    writes VISUAL_EVIDENCE_C2.md from facts.txt.

    It runs the suite first through the one supported runner
    (scripts\run-instrumented-tests.ps1 -TestClass ...), which requires an
    explicit serial and refuses emulator-5554 before any device is contacted.
    Pass -SkipRun to only pull and compose from a run already on the device.

.PARAMETER Serial
    The isolated AVD's adb serial. Confirmed with `emu avd name` by the runner.

.PARAMETER SkipRun
    Do not run the suite; pull and compose what is already on the device.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Serial,
    [switch] $SkipRun
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
$artifacts = Join-Path $root (Join-Path "artifacts" "cad-a3-c2-sketch-ux-r1")
$captures = Join-Path $artifacts 'frames'
$remote = '/sdcard/Android/data/com.forgeshape.app/files/evidence/cad-a3-c2-sketch-ux-r1'

if ($Serial -eq 'emulator-5554') {
    throw 'emulator-5554 is reserved by another program and must not be used.'
}
$avd = (& $adb -s $Serial emu avd name 2>$null | Select-Object -First 1)
Write-Host ("device {0} = AVD '{1}'" -f $Serial, $avd)

if (-not $SkipRun) {
    & (Join-Path $PSScriptRoot 'run-instrumented-tests.ps1') -Serial $Serial `
        -TestClass 'com.forgeshape.app.SketchUxVisualEvidenceTest'
    if ($LASTEXITCODE -ne 0) {
        throw "SketchUxVisualEvidenceTest did not pass (exit $LASTEXITCODE); no evidence composed."
    }
}

if (Test-Path $captures) { Remove-Item -Recurse -Force $captures }
New-Item -ItemType Directory -Force -Path $captures | Out-Null
& $adb -s $Serial pull $remote $captures | Out-Null
# adb pull of a directory nests it under its own name.
$nested = Join-Path $captures 'cad-a3-c2-sketch-ux-r1'
if (Test-Path $nested) {
    Get-ChildItem $nested | Move-Item -Destination $captures -Force
    Remove-Item -Recurse -Force $nested
}
$frames = Get-ChildItem $captures -Filter '*.png' | Sort-Object Name
if ($frames.Count -ne 12) {
    throw "expected 12 captures, found $($frames.Count) in $captures"
}

# ---------------------------------------------------------------------------
# The contact sheet: five columns, two rows, each frame scaled to 300 px wide
# with its file name under it. Measurable layout only; nothing is retouched.
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
$cellW = 300
$pad = 12
$labelH = 28
$first = [System.Drawing.Image]::FromFile($frames[0].FullName)
$scale = $cellW / $first.Width
$cellH = [int][math]::Ceiling($first.Height * $scale)
$first.Dispose()
$columns = 4
$rows = 3
$sheet = New-Object System.Drawing.Bitmap (($columns * ($cellW + $pad)) + $pad), (($rows * ($cellH + $labelH + $pad)) + $pad)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.Clear([System.Drawing.Color]::FromArgb(255, 24, 24, 26))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$font = New-Object System.Drawing.Font 'Segoe UI', 10
$brush = [System.Drawing.Brushes]::Gainsboro
for ($i = 0; $i -lt $frames.Count; $i++) {
    $col = $i % $columns
    $row = [math]::Floor($i / $columns)
    $x = $pad + $col * ($cellW + $pad)
    $y = $pad + $row * ($cellH + $labelH + $pad)
    $img = [System.Drawing.Image]::FromFile($frames[$i].FullName)
    $g.DrawImage($img, $x, $y, $cellW, [int]($img.Height * ($cellW / $img.Width)))
    $img.Dispose()
    $g.DrawString($frames[$i].BaseName, $font, $brush, $x, $y + $cellH + 4)
}
$g.Dispose()
$sheetPath = Join-Path $artifacts 'OWNER_CONTACT_SHEET_C2.png'
$sheet.Save($sheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()

# ---------------------------------------------------------------------------
# VISUAL_EVIDENCE_C2.md: the facts the suite measured, verbatim, per capture.
# ---------------------------------------------------------------------------
$facts = Get-Content (Join-Path $captures 'facts.txt')
$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Visual evidence - CAD-A3-C2 / SKETCH-UX-R1 (`E2E-CADUXR1-VIS`)')
$md.Add('')
$md.Add(("Captured by `SketchUxVisualEvidenceTest` on `{0}` (AVD `{1}`) through the composed display, " -f $Serial, $avd) +
        'one journey in twelve frames. `OWNER_CONTACT_SHEET_C2.png` is the twelve frames at 300 px wide, four per row, in order; ' +
        'the full-resolution frames are in `frames/`.')
$md.Add('')
$md.Add('Every line below is a fact the suite measured on the device at the moment of the capture: an element''s presence and ' +
        'on-screen bounds in pixels, a selected or highlighted state, the camera projection mode (0 perspective, 1 orthographic), ' +
        'the sketch state ' +
        '(0 inactive, 1 editing, 2 ready), the sketch plane (0 XY, 1 XZ, 2 YZ), the navigator''s flip and quarter turns, an entity kind ' +
        '(0 line, 1 polyline, 2 rectangle, 3 circle, 4 arc, 5 spline), a length in metres, the body count and the history depth. ' +
        '**Nothing aesthetic ' +
        'is asserted, and no owner approval of how anything looks is claimed.**')
$md.Add('')
# The suite writes each capture's facts first and the capture line after them,
# so the facts are held until their capture's heading has been written.
$pending = New-Object System.Collections.Generic.List[string]
foreach ($line in $facts) {
    if ($line -match '^capture=(\S+)\s+width=(\d+)\s+height=(\d+)') {
        $md.Add(('## `{0}` ({1} x {2})' -f $matches[1], $matches[2], $matches[3]))
        $md.Add('')
        $md.Add(('![{0}](frames/{0})' -f $matches[1]))
        $md.Add('')
        foreach ($fact in $pending) { $md.Add($fact) }
        $md.Add('')
        $pending.Clear()
    } elseif ($line -match '^\s+(\S+)=(.*)$') {
        $pending.Add(('- `{0}` = `{1}`' -f $matches[1], $matches[2]))
    } elseif ($line -match '^captures=') {
        $md.Add(('Captures: {0}' -f ($line -replace '^captures=', '')))
        $md.Add('')
    }
}
$md.Add('')
$md.Add('Phone viewport only: the test harness has no separate tablet viewport (the layout suites simulate window ' +
        'sizes for the chrome, not a second display), so no tablet capture is claimed.')
[System.IO.File]::WriteAllLines((Join-Path $artifacts 'VISUAL_EVIDENCE_C2.md'), $md)
Write-Host ("wrote {0} and VISUAL_EVIDENCE_C2.md from {1} frames" -f $sheetPath, $frames.Count)
