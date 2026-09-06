<#
.SYNOPSIS
    Pulls the UI-PREF-R1 screenshot evidence off the isolated AVD and builds
    the owner contact sheet.

.DESCRIPTION
    `UiPrefVisualEvidenceTest` captures twenty composed-display frames of the
    Settings -> five palettes -> handedness -> gizmo journey and writes them,
    with a line of measurable facts per capture, into the app's external files
    directory. This script pulls it into artifacts/ui-pref-r1/frames/,
    composes OWNER_CONTACT_SHEET_UI_PREF_R1.png with System.Drawing (no
    third-party tool), and writes VISUAL_EVIDENCE.md from facts.txt.

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
$artifacts = Join-Path $root (Join-Path "artifacts" "ui-pref-r1")
$captures = Join-Path $artifacts 'frames'
$remote = '/sdcard/Android/data/com.forgeshape.app/files/evidence/ui-pref-r1'

if ($Serial -eq 'emulator-5554') {
    throw 'emulator-5554 is reserved by another program and must not be used.'
}
$avd = (& $adb -s $Serial emu avd name 2>$null | Select-Object -First 1)
Write-Host ("device {0} = AVD '{1}'" -f $Serial, $avd)

if (-not $SkipRun) {
    & (Join-Path $PSScriptRoot 'run-instrumented-tests.ps1') -Serial $Serial `
        -TestClass 'com.forgeshape.app.UiPrefVisualEvidenceTest'
    if ($LASTEXITCODE -ne 0) {
        throw "UiPrefVisualEvidenceTest did not pass (exit $LASTEXITCODE); no evidence composed."
    }
}

if (Test-Path $captures) { Remove-Item -Recurse -Force $captures }
New-Item -ItemType Directory -Force -Path $captures | Out-Null
& $adb -s $Serial pull $remote $captures | Out-Null
# adb pull of a directory nests it under its own name.
$nested = Join-Path $captures 'ui-pref-r1'
if (Test-Path $nested) {
    Get-ChildItem $nested | Move-Item -Destination $captures -Force
    Remove-Item -Recurse -Force $nested
}
$frames = Get-ChildItem $captures -Filter '*.png' | Sort-Object Name
if ($frames.Count -ne 20) {
    throw "expected 20 captures, found $($frames.Count) in $captures"
}

# ---------------------------------------------------------------------------
# The contact sheet: five columns, four rows, each frame scaled to 240 px wide
# with its file name under it. Measurable layout only; nothing is retouched.
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
$cellW = 240
$pad = 12
$labelH = 28
# Portrait and landscape frames share a cell: the cell is as tall as the
# tallest scaled frame, and a landscape frame sits at the top of its cell.
$cellH = 0
foreach ($frame in $frames) {
    $img = [System.Drawing.Image]::FromFile($frame.FullName)
    $h = [int][math]::Ceiling($img.Height * ($cellW / $img.Width))
    if ($h -gt $cellH) { $cellH = $h }
    $img.Dispose()
}
$columns = 5
$rows = 4
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
$sheetPath = Join-Path $artifacts 'OWNER_CONTACT_SHEET_UI_PREF_R1.png'
$sheet.Save($sheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()

# ---------------------------------------------------------------------------
# VISUAL_EVIDENCE.md: the facts the suite measured, verbatim, per capture.
# ---------------------------------------------------------------------------
$facts = Get-Content (Join-Path $captures 'facts.txt')
$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Visual evidence - UI-PREF-R1 (`E2E-UIPREFR1-VIS`)')
$md.Add('')
$md.Add(("Captured by `UiPrefVisualEvidenceTest` on `{0}` (AVD `{1}`) through the composed display, " -f $Serial, $avd) +
        'one journey in twenty frames. `OWNER_CONTACT_SHEET_UI_PREF_R1.png` is the twenty frames at 240 px wide, five per row, in order; ' +
        'the full-resolution frames are in `frames/`.')
$md.Add('')
$md.Add('Every line below is a fact the suite measured on the device at the moment of the capture: the preference in force ' +
        '(the palette by name, the handedness, the gizmo visual scale and stroke weight), the native values the preference reached ' +
        '(the viewport background index 0..4, the native visual scale and stroke weight 0 Thin / 1 Regular / 2 Bold), whether the ' +
        'system bars draw dark icons, the window ground''s WCAG luminance, an element''s presence and on-screen bounds in pixels, ' +
        'the rail''s inset, width and top in dp, the gizmo''s pivot and handle pixels with the handle each pixel hits, the body count and ' +
        'the history depth. **Nothing aesthetic is asserted, and no owner approval of how anything looks is claimed.**')
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
$md.Add('Phone viewport only: the test harness has no separate tablet viewport (the layout suites simulate window ' +
        'sizes for the chrome, not a second display), so no physical tablet capture is claimed.')
[System.IO.File]::WriteAllLines((Join-Path $artifacts 'VISUAL_EVIDENCE.md'), $md)
Write-Host ("wrote {0} and VISUAL_EVIDENCE.md from {1} frames" -f $sheetPath, $frames.Count)
