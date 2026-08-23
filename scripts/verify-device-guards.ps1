<#
.SYNOPSIS
    DEV2-01..07 + DEV3-01..06 device-isolation guard checks.

.DESCRIPTION
    Verifies, without ever contacting emulator-5554 and without requiring any
    device to be attached, that the two repo device scripts fail closed the
    way CLAUDE.md requires. Two kinds of check are used:
      - Live, safe invocations: calling the real script with an input that is
        rejected before any OS/adb/process call is made (forbidden port,
        forbidden serial, missing serial). These run the actual reject code
        path, not a mock of it.
      - Static content inspection: grepping the script source for control-flow
        properties that cannot be safely exercised live without a real device
        or a real port conflict (explicit -port literal, absence of bare adb,
        every adb call scoped with -s).

    Exit code is 0 only if every DEV2 and DEV3 check PASSes.
#>

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $root 'scripts\start-forgeshape-emulator.ps1'
$instrumenter = Join-Path $root 'scripts\run-instrumented-tests.ps1'

$results = New-Object System.Collections.Generic.List[object]
function Record($id, $pass, $evidence) {
    $results.Add([pscustomobject]@{ Id = $id; Pass = $pass; Evidence = $evidence })
}

# Child scripts set $ErrorActionPreference = 'Stop', so their own Write-Error
# calls are terminating errors. Invoked via `&` in-process, that exception
# would unwind past this script's own scope entirely (verified: it aborted
# this checker outright on a first attempt), and even wrapped in try/catch,
# $LASTEXITCODE is never set by a same-session script that throws before
# reaching its own `exit N` -- so `$LASTEXITCODE -ne 0` would silently read a
# stale value from some earlier command and pass vacuously. Each live
# invocation below therefore runs the child as a genuine child PROCESS
# (`powershell.exe -File`), whose real exit code $LASTEXITCODE reliably
# reflects across that process boundary regardless of how the child failed.
function Invoke-ChildScript {
    param([string]$ScriptPath, [string[]]$ScriptArgs)
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @ScriptArgs *> $null
    return $LASTEXITCODE
}

# --- DEV2-01: forbidden port 5554 launcher rejects before process start ----
$exit01 = Invoke-ChildScript -ScriptPath $launcher -ScriptArgs @('-Port', '5554')
Record 'DEV2-01' ($exit01 -ne 0) "start-forgeshape-emulator.ps1 -Port 5554 exited $exit01 (nonzero = rejected)"

# --- DEV2-02: an occupied requested port -> BLOCKED, no alternate-port -----
# Bind a dummy listener on a harmless high port (not 5554, not 5580) so the
# launcher's own occupancy check has something real to detect, without ever
# touching the ports this repo actually cares about.
$dummyPort = 58123
$listener = $null
$dev202 = $false
$dev202evidence = ''
try {
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $dummyPort)
    $listener.Start()
    $exit02 = Invoke-ChildScript -ScriptPath $launcher -ScriptArgs @('-Port', "$dummyPort", '-Avd', 'Irrelevant_For_This_Check')
    $dev202 = ($exit02 -ne 0)
    $dev202evidence = "start-forgeshape-emulator.ps1 -Port $dummyPort (pre-occupied) exited $exit02 (nonzero = BLOCKED)"
} finally {
    if ($listener) { $listener.Stop() }
}
Record 'DEV2-02' $dev202 $dev202evidence

# --- DEV2-03: launcher command contains explicit -port 5580 (default) ------
$launcherSrc = Get-Content -Raw $launcher
$hasExplicitPortLiteral = ($launcherSrc -match '\[int\]\$Port\s*=\s*5580') -and
                          ($launcherSrc -match '-avd \$Avd -port \$Port')
Record 'DEV2-03' $hasExplicitPortLiteral 'default $Port = 5580 and launch command line interpolates "-avd $Avd -port $Port"'

# --- DEV2-04: missing instrumentation serial -> fail before adb ------------
# A literal empty string is dropped by PowerShell's own -File argument
# parsing ("Missing an argument for parameter 'Serial'") before the script
# body runs at all -- already a fail-before-adb proof, just via the engine's
# own Mandatory-parameter binding. A whitespace-only value passes that engine
# binding and instead exercises the script's OWN IsNullOrWhiteSpace guard, so
# check both.
$exit04a = Invoke-ChildScript -ScriptPath $instrumenter -ScriptArgs @('-Serial', '')
$exit04b = Invoke-ChildScript -ScriptPath $instrumenter -ScriptArgs @('-Serial', ' ')
Record 'DEV2-04' (($exit04a -ne 0) -and ($exit04b -ne 0)) "-Serial '' exited $exit04a (engine Mandatory binding); -Serial ' ' exited $exit04b (script's own IsNullOrWhiteSpace guard); both nonzero = rejected pre-adb"

# --- DEV2-05: emulator-5554 instrumentation serial -> fail before adb ------
$exit05 = Invoke-ChildScript -ScriptPath $instrumenter -ScriptArgs @('-Serial', 'emulator-5554')
Record 'DEV2-05' ($exit05 -ne 0) "run-instrumented-tests.ps1 -Serial emulator-5554 exited $exit05 (nonzero = rejected pre-adb)"

# --- DEV2-06: repo scripts contain no bare adb invocation -------------------
# Scans every OTHER repo script (not this checker, which only talks ABOUT adb
# in comments/regex literals and invokes nothing) for actual `& adb ...`
# statements -- the call-operator form both device scripts consistently use
# for every real invocation -- and flags any that isn't scoped `-s <serial>`
# (`adb @instrumentArgs` counts as scoped: that array's first two elements are
# always '-s', $Serial).
$scriptFiles = Get-ChildItem (Join-Path $root 'scripts') -Filter '*.ps1' |
    Where-Object { $_.FullName -ne (Resolve-Path $PSCommandPath).Path }
$bareHits = @()
foreach ($f in $scriptFiles) {
    $lineNum = 0
    foreach ($line in Get-Content $f.FullName) {
        $lineNum++
        $trimmed = $line.TrimStart()
        if ($trimmed.StartsWith('#')) { continue }
        if ($trimmed -match '&\s*adb\b' -and
            $trimmed -notmatch '-s\s+\$?\w' -and
            $trimmed -notmatch '@instrumentArgs') {
            $bareHits += "$($f.Name):${lineNum}: $trimmed"
        }
    }
}
Record 'DEV2-06' ($bareHits.Count -eq 0) $(if ($bareHits.Count -eq 0) { "no bare '& adb' invocation across $($scriptFiles.Count) other script(s)" } else { $bareHits -join ' | ' })

# --- DEV2-07: instrumentation on emulator-5580 uses only adb -s emulator-5580
# Static property of run-instrumented-tests.ps1: every adb call site is
# parameterized on $Serial, never a literal serial, so running it with
# -Serial emulator-5580 can only ever touch emulator-5580.
$instrumenterSrc = Get-Content -Raw $instrumenter
$adbCallLines = (Select-String -Path $instrumenter -Pattern '&\s*adb\b')
$allScopedToSerialVar = $true
$adbCallEvidence = @()
foreach ($m in $adbCallLines) {
    $t = $m.Line.Trim()
    $adbCallEvidence += "line $($m.LineNumber): $t"
    if ($t -notmatch '-s\s+\$Serial' -and $t -notmatch '@instrumentArgs') {
        $allScopedToSerialVar = $false
    }
}
Record 'DEV2-07' $allScopedToSerialVar ($adbCallEvidence -join ' | ')

# =============================================================================
# DEV3-01..06 -- hardened static protection of the executable workflow surface
# =============================================================================
#
# DEV2-06/07 above are kept exactly as accepted, but they only ever recognised
# the `& adb` call-operator form and only ever looked at scripts\*.ps1. A plain
# `adb shell ...` -- the form anyone would type first -- was invisible to them,
# as was any .cmd/.bat/.sh or Gradle surface, and an executable
# connected*AndroidTest fan-out was not checked at all. DEV2-06 also accepted
# `@instrumentArgs` by NAME, trusting a variable rather than proving the array
# it refers to actually carries '-s'.
#
# The detector below is shared by the fixture checks and the real repo scan, so
# DEV3-01..05 are testing the same code that guards the repo, not a copy of it.

# Removes quoted string bodies and trailing comments, so PROSE about a
# forbidden command (this repo really does log
# "Running instrumentation on -s <serial> only: adb ..." and mentions
# connectedDebugAndroidTest in a message) is never mistaken for running it.
function Get-ExecutablePart {
    param([string]$Line, [string]$CommentPrefix = '#')
    $t = $Line.Trim()
    if ($t -eq '') { return '' }
    if ($CommentPrefix -eq '#'   -and $t.StartsWith('#'))  { return '' }
    if ($t.StartsWith('::') -or $t -match '^(?i)rem\s')    { return '' }
    # Strip string literals first, then anything after a comment marker.
    $t = $t -replace "'[^']*'", "''"
    $t = $t -replace '"[^"]*"', '""'
    $t = $t -replace '#.*$', ''
    return $t
}

# Proves an `adb ...` invocation is scoped. Either it carries a literal
# `-s <something>`, or it splats an array -- in which case the array's own
# definition must be found in the file and must itself contain '-s'. The
# variable's NAME proves nothing and is never accepted on its own.
function Test-AdbInvocationIsScoped {
    param([string]$ExecutablePart, [string[]]$AllLines)
    $after = $ExecutablePart -replace '^.*?\badb(?:\.exe)?\b', ''
    if ($after -match '(^|\s)-s(\s|$)') { return $true }
    if ($after -match '@(\w+)') {
        $arrayName = $Matches[1]
        foreach ($l in $AllLines) {
            if ($l -match ('\$' + [regex]::Escape($arrayName) + '\s*=\s*@\(')) {
                # The ORIGINAL line is inspected here on purpose: the array
                # literal's '-s' lives inside quotes, which Get-ExecutablePart
                # would have stripped.
                if ($l -match "@\(\s*['`"]-s['`"]") { return $true }
            }
        }
        return $false
    }
    return $false
}

# Returns violation strings for one file's content.
#
# Tracks PowerShell block comments (`<# ... #>`) as state across lines. Without
# that, every line of a .SYNOPSIS/.DESCRIPTION help block reads as code -- and
# both repo device scripts document the forbidden commands in exactly such a
# block, so the scanner reported them as violations of the rule they explain.
function Find-DeviceGuardViolations {
    param([string[]]$Lines, [string]$Label)
    $violations = @()
    $n = 0
    $inBlockComment = $false
    foreach ($line in $Lines) {
        $n++
        if ($inBlockComment) {
            if ($line -match '#>') { $inBlockComment = $false }
            continue
        }
        if ($line -match '<#') {
            # A block that also closes on this line leaves no code behind here.
            if ($line -notmatch '#>') { $inBlockComment = $true }
            continue
        }
        $exec = Get-ExecutablePart -Line $line
        if ($exec -eq '') { continue }
        if ($exec -match '(?:^|[\s;&|(])&?\s*adb(?:\.exe)?(?:\s|$)') {
            if (-not (Test-AdbInvocationIsScoped -ExecutablePart $exec -AllLines $Lines)) {
                $violations += "${Label}:${n}: unscoped adb: $($line.Trim())"
            }
        }
        if ($exec -match 'connected\w*AndroidTest') {
            $violations += "${Label}:${n}: connected-device fan-out: $($line.Trim())"
        }
    }
    return ,$violations
}

# --- DEV3-01: a bare `adb shell` is detected -------------------------------
$fx01 = @('adb shell am instrument -w com.example/AndroidJUnitRunner')
$v01 = Find-DeviceGuardViolations -Lines $fx01 -Label 'fixture01'
Record 'DEV3-01' ($v01.Count -eq 1) "bare 'adb shell' -> $($v01.Count) violation(s) (1 = detected)"

# --- DEV3-02: a bare `& adb shell` is detected ------------------------------
$fx02 = @('& adb shell am instrument -w com.example/AndroidJUnitRunner')
$v02 = Find-DeviceGuardViolations -Lines $fx02 -Label 'fixture02'
Record 'DEV3-02' ($v02.Count -eq 1) "bare '& adb shell' -> $($v02.Count) violation(s) (1 = detected)"

# --- DEV3-03: correctly scoped calls pass, and prose does not fail ----------
$fx03 = @(
    '& adb -s $Serial install -r $appApk',
    'adb -s emulator-5580 get-state',
    '# adb shell am instrument -w  (a comment about the forbidden form)',
    'Write-Output "Running instrumentation on -s $Serial only: adb $($args -join '' '')"',
    'Write-Output "matches connectedDebugAndroidTest''s own cleanup behaviour"'
)
$v03 = Find-DeviceGuardViolations -Lines $fx03 -Label 'fixture03'
Record 'DEV3-03' ($v03.Count -eq 0) "scoped calls + prose/comments -> $($v03.Count) violation(s) (0 = correct)"

# --- DEV3-04: an argument-array wrapper must PROVE -s, not be trusted -------
$fx04good = @(
    '$instrumentArgs = @(''-s'', $Serial, ''shell'', ''am'', ''instrument'', ''-w'')',
    '& adb @instrumentArgs'
)
$fx04bad = @(
    '$instrumentArgs = @(''shell'', ''am'', ''instrument'', ''-w'')',
    '& adb @instrumentArgs'
)
$v04good = Find-DeviceGuardViolations -Lines $fx04good -Label 'fixture04good'
$v04bad = Find-DeviceGuardViolations -Lines $fx04bad -Label 'fixture04bad'
Record 'DEV3-04' (($v04good.Count -eq 0) -and ($v04bad.Count -eq 1)) `
    "array proving -s -> $($v04good.Count) violation(s) (0 expected); identically-named array WITHOUT -s -> $($v04bad.Count) (1 expected, so the name alone is not trusted)"

# --- DEV3-05: an executable connected-device fan-out is detected ------------
$fx05 = @('.\gradlew.bat :app:connectedDebugAndroidTest')
$v05 = Find-DeviceGuardViolations -Lines $fx05 -Label 'fixture05'
Record 'DEV3-05' ($v05.Count -eq 1) "executable connectedDebugAndroidTest -> $($v05.Count) violation(s) (1 = detected)"

# --- DEV3-06: the real scan covers the critical surfaces and is clean -------
# Guards against the vacuous pass: a scanner that enumerated nothing, or that
# missed the two device scripts or the Gradle files, would report "no
# violations" while checking nothing at all.
$surfaces = @()
$surfaces += Get-ChildItem (Join-Path $root 'scripts') -File |
    Where-Object { $_.Extension -in @('.ps1', '.cmd', '.bat', '.sh') }
$surfaces += Get-ChildItem $root -File |
    Where-Object { $_.Extension -in @('.cmd', '.bat', '.sh') -or $_.Name -like '*.gradle' }
$surfaces += Get-ChildItem (Join-Path $root 'app') -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like '*.gradle' }
$surfaces = $surfaces | Sort-Object FullName -Unique |
    Where-Object { $_.FullName -ne (Resolve-Path $PSCommandPath).Path }

$scanned = @()
$realViolations = @()
foreach ($f in $surfaces) {
    $scanned += $f.Name
    $realViolations += Find-DeviceGuardViolations -Lines (Get-Content $f.FullName) -Label $f.Name
}
$requiredSurfaces = @('start-forgeshape-emulator.ps1', 'run-instrumented-tests.ps1',
                      'build.gradle', 'gradlew.bat')
$missing = $requiredSurfaces | Where-Object { $scanned -notcontains $_ }
$dev306 = ($missing.Count -eq 0) -and ($realViolations.Count -eq 0) -and ($scanned.Count -ge 4)
Record 'DEV3-06' $dev306 $(if ($dev306) {
        "scanned $($scanned.Count) executable surface(s) including all $($requiredSurfaces.Count) required; 0 violations"
    } elseif ($missing.Count -gt 0) {
        "CRITICAL SURFACE NOT SCANNED: $($missing -join ', ')"
    } else {
        $realViolations -join ' | '
    })

# --- Report ------------------------------------------------------------------
$results | ForEach-Object {
    $status = if ($_.Pass) { 'PASS' } else { 'FAIL' }
    Write-Output "$($_.Id) | $status | $($_.Evidence)"
}
# The @() is load-bearing, not style. In PowerShell 5.1 a Where-Object that
# matches exactly ONE object returns that object, not a one-element array, and
# a PSCustomObject has no .Count -- so `$failed.Count` was $null, `$null -gt 0`
# was false, and this script printed "All checks PASS" and exited 0 whenever
# exactly one check had FAILED. Found by DEV3-06 failing on its own first run.
$failed = @($results | Where-Object { -not $_.Pass })
if ($failed.Count -gt 0) {
    Write-Error "$($failed.Count) device-guard check(s) FAILED."
    exit 1
}
Write-Output "All device-guard checks PASS."
exit 0
