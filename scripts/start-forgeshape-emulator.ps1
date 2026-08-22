<#
.SYNOPSIS
    The one supported way to boot a ForgeShape-owned emulator for runtime
    evidence, always on an explicit, non-default port.

.DESCRIPTION
    The Android emulator assigns the first free port starting at 5554 to
    whichever instance boots first, so an AVD launched without an explicit
    `-port` can land on the reserved `emulator-5554` (`Medium_Phone_API_36.1`,
    owned by another program) purely by allocation order -- this happened
    during Stage 016-R. This script never lets that happen: it always passes
    an explicit `-avd` and `-port`, refuses the forbidden port outright, checks
    ONLY the one port it is about to use (never 5554 or any other), and BLOCKS
    rather than silently choosing a different port when that one is occupied.

    Guard order:
      1. Reject the forbidden port (5554) before touching the OS or adb at all.
      2. Check ONLY the requested port with Get-NetTCPConnection. No probe of
         5554 or of any port other than the one requested.
      3. If occupied: BLOCKED. No fallback port is ever chosen automatically.
      4. Launch the emulator DETACHED (WMI/CIM Win32_Process.Create), because
         it dies if spawned as a child of this or any other tool shell.
      5. Wait for the device to come up, then confirm AVD identity with
         `adb -s <serial> emu avd name` -- never trust the port/serial alone.
         A mismatch is reported as BLOCKED/STOP; this script does not
         auto-kill whatever it finds, since that AVD may not be ours to stop.

    All post-launch device commands are scoped `adb -s <serial> ...`. This
    script never runs a bare, device-wide adb command.

.PARAMETER Avd
    The AVD to launch. Defaults to the repo-approved ForgeShape_Stage006.

.PARAMETER Port
    The explicit emulator port. Defaults to the repo-approved 5580. 5554 is
    always rejected, regardless of what is passed here.

.PARAMETER SdkRoot
    Android SDK root containing emulator\emulator.exe. Defaults to
    local.properties' sdk.dir, then $env:ANDROID_SDK_ROOT / $env:ANDROID_HOME.

.EXAMPLE
    scripts\start-forgeshape-emulator.ps1
    scripts\start-forgeshape-emulator.ps1 -Avd ForgeShape_Stage006 -Port 5580
#>
param(
    [string]$Avd = 'ForgeShape_Stage006',
    [int]$Port = 5580,
    [string]$SdkRoot
)

$ErrorActionPreference = 'Stop'

# --- 1. Reject the forbidden port before touching anything else ------------
# CLAUDE.md: emulator-5554 / Medium_Phone_API_36.1 is reserved by another
# program and must not be used, started, stopped, modified, installed to,
# logged, screenshotted or sent input by ForgeShape work.
$ForbiddenPort = 5554
if ($Port -eq $ForbiddenPort) {
    Write-Error "Refusing: port $ForbiddenPort is repo-forbidden (CLAUDE.md: emulator-5554 / Medium_Phone_API_36.1 is reserved by another program). No OS query and no process has been started."
    exit 1
}
$Serial = "emulator-$Port"
if ($Serial -ieq 'emulator-5554') {
    Write-Error "Refusing: resulting serial '$Serial' is repo-forbidden. No OS query and no process has been started."
    exit 1
}

# --- 2. Check ONLY the requested port, never 5554 or any other -------------
Write-Output "Checking that port $Port (only) is free..."
$portOwner = $null
try {
    $portOwner = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
} catch {
    $portOwner = $null
}
if ($portOwner) {
    Write-Error "BLOCKED: port $Port is already in use (PID $($portOwner.OwningProcess -join ',')). No alternate port is chosen automatically -- pick a different explicit -Port, or free $Port, and re-run."
    exit 1
}
Write-Output "OK: port $Port is free."

# --- Locate emulator.exe -----------------------------------------------------
if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
    $localProps = Join-Path $PSScriptRoot '..\local.properties'
    if (Test-Path $localProps) {
        $sdkLine = Select-String -Path $localProps -Pattern '^sdk\.dir=' | Select-Object -First 1
        if ($sdkLine) {
            $SdkRoot = ($sdkLine.Line -replace '^sdk\.dir=', '') -replace '\\\\', '\' -replace '\\:', ':'
        }
    }
}
if ([string]::IsNullOrWhiteSpace($SdkRoot)) { $SdkRoot = $env:ANDROID_SDK_ROOT }
if ([string]::IsNullOrWhiteSpace($SdkRoot)) { $SdkRoot = $env:ANDROID_HOME }
if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
    Write-Error "Could not determine Android SDK root (checked local.properties, ANDROID_SDK_ROOT, ANDROID_HOME). Pass -SdkRoot explicitly."
    exit 1
}
$emulatorExe = Join-Path $SdkRoot 'emulator\emulator.exe'
if (-not (Test-Path $emulatorExe)) {
    Write-Error "emulator.exe not found at '$emulatorExe'."
    exit 1
}

# --- 3. Launch DETACHED, with explicit -avd and -port -----------------------
# Must be a WMI/CIM child of explorer/the service host, not of this shell --
# the emulator process dies if its parent shell exits or is torn down.
$commandLine = "`"$emulatorExe`" -avd $Avd -port $Port"
Write-Output "Launching detached: $commandLine"
$result = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $commandLine }
if ($result.ReturnValue -ne 0) {
    Write-Error "Win32_Process.Create failed with return code $($result.ReturnValue)."
    exit 1
}
Write-Output "Emulator process launched (PID $($result.ProcessId)). Waiting for '$Serial' to come up..."

# --- 4. Wait for the device, then confirm identity via adb -s <serial> only -
# adb reports "error: device 'x' not found" on stderr, with a nonzero exit,
# for as long as the serial hasn't appeared yet -- expected during boot, not
# a failure. Under $ErrorActionPreference = 'Stop', redirecting that stderr
# turns it into a terminating NativeCommandError, so this poll must swallow
# it via try/catch rather than 2>$null.
$deadline = (Get-Date).AddSeconds(180)
$ready = $false
while ((Get-Date) -lt $deadline) {
    $state = $null
    try { $state = & adb -s $Serial get-state 2>$null } catch { $state = $null }
    if ($LASTEXITCODE -eq 0 -and $state -eq 'device') { $ready = $true; break }
    Start-Sleep -Seconds 3
}
if (-not $ready) {
    Write-Error "BLOCKED: '$Serial' did not report ready state within 180s. Not confirmed usable; not treated as safe."
    exit 1
}

$identity = (& adb -s $Serial emu avd name 2>$null | Select-Object -First 1).Trim()
if ($identity -ne $Avd) {
    Write-Error "BLOCKED: 'adb -s $Serial emu avd name' reported '$identity', expected '$Avd'. A port/serial is never a stable identifier for an AVD -- refusing to treat '$Serial' as safe. This script has not stopped, wiped or reconfigured it."
    exit 1
}

Write-Output "OK: '$Serial' confirmed as '$Avd' by AVD name (not by port). Ready for use."
exit 0
