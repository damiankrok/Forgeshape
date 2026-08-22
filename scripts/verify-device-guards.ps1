<#
.SYNOPSIS
    DEV2-01..07 device-isolation guard checks (Stage 016-R2).

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

    Exit code is 0 only if every DEV2 check PASSes.
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

# --- Report ------------------------------------------------------------------
$results | ForEach-Object {
    $status = if ($_.Pass) { 'PASS' } else { 'FAIL' }
    Write-Output "$($_.Id) | $status | $($_.Evidence)"
}
$failed = $results | Where-Object { -not $_.Pass }
if ($failed.Count -gt 0) {
    Write-Error "$($failed.Count) DEV2 check(s) FAILED."
    exit 1
}
Write-Output "All DEV2 checks PASS."
exit 0
