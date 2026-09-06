<#
.SYNOPSIS
    Pulls the SEL-OUT-R1 screenshot evidence off the isolated AVD and builds
    the owner contact sheet.

.DESCRIPTION
    `SelectionOutlineVisualEvidenceTest` captures twelve composed-display
    frames of the selection-outline journey -- Construction, outline off,
    Imported Mesh, Sculpt, CAD, an occluded selection, two bodies A then B, the
    two light grounds, the left-handed View/Overlay surface and the fallback
    selection after a Delete -- and writes them, with a line of measurable facts
    per capture, into the app's external files directory. Those facts include
    the band width and colour MEASURED out of the captured bitmap, not only the
    values the renderer reports.

    This script pulls that into artifacts/sel-out-r1/frames/, composes
    OWNER_CONTACT_SHEET_SEL_OUT_R1.png with System.Drawing (no third-party
    tool), and writes VISUAL_EVIDENCE.md from facts.txt.

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
$artifacts = Join-Path $root (Join-Path "artifacts" "sel-out-r1")
$captures = Join-Path $artifacts 'frames'
$remote = '/sdcard/Android/data/com.forgeshape.app/files/evidence/sel-out-r1'
$expected = 12

if ($Serial -eq 'emulator-5554') {
    throw 'emulator-5554 is reserved by another program and must not be used.'
}
$avd = (& $adb -s $Serial emu avd name 2>$null | Select-Object -First 1)
Write-Host ("device {0} = AVD '{1}'" -f $Serial, $avd)

if (-not $SkipRun) {
    & (Join-Path $PSScriptRoot 'run-instrumented-tests.ps1') -Serial $Serial `
        -TestClass 'com.forgeshape.app.SelectionOutlineVisualEvidenceTest'
    if ($LASTEXITCODE -ne 0) {
        throw "SelectionOutlineVisualEvidenceTest did not pass (exit $LASTEXITCODE); no evidence composed."
    }
}

New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
if (Test-Path $captures) { Remove-Item -Recurse -Force $captures }
New-Item -ItemType Directory -Force -Path $captures | Out-Null
# adb writes its progress line to stderr, and Windows PowerShell wraps a native
# command's stderr in an ErrorRecord: with $ErrorActionPreference = 'Stop' that
# becomes a terminating NativeCommandError even though the pull succeeded. The
# preference is relaxed for exactly this call and the real outcome is judged by
# the frame count below, which is the only thing that actually matters.
$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $adb -s $Serial pull $remote $captures 2>&1 | Out-Null
$ErrorActionPreference = $previousErrorAction
# adb pull of a directory nests it under its own name.
$nested = Join-Path $captures 'sel-out-r1'
if (Test-Path $nested) {
    Get-ChildItem $nested | Move-Item -Destination $captures -Force
    Remove-Item -Recurse -Force $nested
}
$frames = Get-ChildItem $captures -Filter '*.png' | Sort-Object Name
if ($frames.Count -ne $expected) {
    throw "expected $expected captures, found $($frames.Count) in $captures"
}

# ---------------------------------------------------------------------------
# The contact sheet: four columns, three rows, each frame scaled to 260 px wide
# with its file name under it. Measurable layout only; nothing is retouched.
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
$cellW = 260
$pad = 12
$labelH = 28
$cellH = 0
foreach ($frame in $frames) {
    $img = [System.Drawing.Image]::FromFile($frame.FullName)
    $h = [int][math]::Ceiling($img.Height * ($cellW / $img.Width))
    if ($h -gt $cellH) { $cellH = $h }
    $img.Dispose()
}
$columns = 4
$rows = [int][math]::Ceiling($frames.Count / $columns)
$sheet = New-Object System.Drawing.Bitmap (($columns * ($cellW + $pad)) + $pad), (($rows * ($cellH + $labelH + $pad)) + $pad)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.Clear([System.Drawing.Color]::FromArgb(255, 24, 24, 26))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$font = New-Object System.Drawing.Font 'Segoe UI', 9
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
$sheetPath = Join-Path $artifacts 'OWNER_CONTACT_SHEET_SEL_OUT_R1.png'
$sheet.Save($sheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()

# ---------------------------------------------------------------------------
# VISUAL_EVIDENCE.md: the facts the suite measured, verbatim, per capture.
# ---------------------------------------------------------------------------
$facts = Get-Content (Join-Path $captures 'facts.txt')
$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Visual evidence - SEL-OUT-R1 (`E2E-SELOUTR1-VIS`)')
$md.Add('')
$md.Add(("Captured by `SelectionOutlineVisualEvidenceTest` on `{0}` (AVD `{1}`) through the composed display, " -f $Serial, $avd) +
        'one journey in twelve frames. `OWNER_CONTACT_SHEET_SEL_OUT_R1.png` is the twelve frames at 260 px wide, four per row, in order; ' +
        'the full-resolution frames are in `frames/`.')
$md.Add('')
$md.Add('Every line below is a fact the suite measured on the device at the moment of the capture. Two kinds appear. ' +
        'The `renderer_*` values are what the Vulkan renderer reports about the resources it holds - the mask image''s extent, how ' +
        'many times it has been allocated for the life of the process, the band''s half-width in screen pixels and the number of ' +
        'composite draws recorded. The `measured_*` values are scanned out of the captured bitmap itself: the widest run of ' +
        'outline-coloured pixels along a horizontal cut through the middle of the display, how many such runs the cut crossed, and ' +
        'the ground colour sampled at the frame''s edge. A frame with the outline OFF must find no runs at all, and that absence is ' +
        'the evidence for capture 02.')
$md.Add('')
$md.Add('**Nothing aesthetic is asserted, and no owner approval of how anything looks is claimed.**')
$md.Add('')
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
$md.Add('Phone viewport only: the test harness has no separate tablet viewport, so no physical tablet capture is claimed.')
[System.IO.File]::WriteAllLines((Join-Path $artifacts 'VISUAL_EVIDENCE.md'), $md)
Write-Host ("wrote {0} and VISUAL_EVIDENCE.md from {1} frames" -f $sheetPath, $frames.Count)
