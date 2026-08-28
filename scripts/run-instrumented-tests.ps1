<#
.SYNOPSIS
    The one supported way to run ForgeShape instrumented (androidTest) tests.

.DESCRIPTION
    Every device operation is explicitly scoped with `adb -s <serial>`.
    No-filter execution remains the supported monolithic full-suite path.
    -FullSharded discovers the live AndroidJUnitRunner inventory, proves an
    exhaustive non-overlapping deterministic partition, executes every shard,
    and is the authoritative aggregate full-suite path. -TestClass is focused
    subset evidence only and can never emit a full-suite marker.

    If infrastructure fails during -FullSharded, recover the device and rerun
    the entire command from shard 1. A class or shard-only rerun is supplementary
    evidence and cannot complete or repair an aggregate.

.PARAMETER Serial
    Exact adb serial to use. emulator-5554 is forbidden before adb is contacted.

.PARAMETER TestClass
    Optional fully-qualified test class. Mutually exclusive with -FullSharded.

.PARAMETER FullSharded
    Run the authoritative exhaustive-sharded complete suite.

.PARAMETER ShardCount
    Number of deterministic class-atomic shards for -FullSharded (default 5).

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.EditorWorkspaceLayoutTest
#>
param(
    [Parameter(Mandatory = $true)][string]$Serial,
    [Parameter(Mandatory = $false)][string]$TestClass,
    [Parameter(Mandatory = $false)][switch]$FullSharded,
    [Parameter(Mandatory = $false)][ValidateRange(1, 64)][int]$ShardCount = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'instrumented-sharding.ps1')

function Stop-InstrumentedRun {
    param([string]$Message, [int]$Code = 1)
    if ($FullSharded) {
        Write-Output 'FULL_SHARDED_SUMMARY_BEGIN'
        Write-Output 'aggregate=FAIL'
        Write-Output "reason=$Message"
        Write-Output (Get-FullSuiteMarker -IsFullSharded $true -AggregatePass $false)
        Write-Output 'FULL_SHARDED_SUMMARY_END'
    }
    Write-Error -Message $Message -ErrorAction Continue
    exit $Code
}

if ($FullSharded -and -not [string]::IsNullOrWhiteSpace($TestClass)) {
    Stop-InstrumentedRun '-FullSharded and -TestClass are mutually exclusive.'
}
if (-not $FullSharded -and $PSBoundParameters.ContainsKey('ShardCount')) {
    Stop-InstrumentedRun '-ShardCount is valid only with -FullSharded.'
}

$ForbiddenSerials = @('emulator-5554')
if ([string]::IsNullOrWhiteSpace($Serial)) {
    Stop-InstrumentedRun "Usage: run-instrumented-tests.ps1 -Serial <adb-serial> [-FullSharded [-ShardCount <n>] | -TestClass <fully.qualified.ClassName>]`nNo default target is chosen -- an explicit -Serial is required every time."
}
foreach ($forbidden in $ForbiddenSerials) {
    if ($Serial -ieq $forbidden) {
        Stop-InstrumentedRun "Refusing: '$Serial' is a repo-forbidden device (CLAUDE.md: emulator-5554 / Medium_Phone_API_36.1 is reserved and must not be contacted by ForgeShape work). No adb command has been issued against it."
    }
}

Write-Output "Checking that '$Serial' is currently attached..."
$state = & adb -s $Serial get-state 2>$null
if ($LASTEXITCODE -ne 0 -or $state -ne 'device') {
    Stop-InstrumentedRun "Refusing: 'adb -s $Serial get-state' did not report a ready device (got '$state'). This script starts nothing and never falls back to another target."
}
Write-Output "OK: '$Serial' is attached."

Write-Output 'Building debug app and androidTest APKs...'
& .\gradlew.bat ':app:assembleDebug' ':app:assembleDebugAndroidTest'
if ($LASTEXITCODE -ne 0) { Stop-InstrumentedRun "Build failed; nothing was installed on '$Serial'." $LASTEXITCODE }

$appApk = 'app\build\outputs\apk\debug\app-debug.apk'
$testApk = 'app\build\outputs\apk\androidTest\debug\app-debug-androidTest.apk'
if (-not (Test-Path $appApk)) { Stop-InstrumentedRun "Missing $appApk after build." }
if (-not (Test-Path $testApk)) { Stop-InstrumentedRun "Missing $testApk after build." }

Write-Output "Installing app APK on -s $Serial ..."
& adb -s $Serial install -r $appApk
if ($LASTEXITCODE -ne 0) { Stop-InstrumentedRun "Install of app APK failed on '$Serial'." $LASTEXITCODE }
Write-Output "Installing test APK on -s $Serial ..."
& adb -s $Serial install -r $testApk
if ($LASTEXITCODE -ne 0) { Stop-InstrumentedRun "Install of test APK failed on '$Serial'." $LASTEXITCODE }

$instrumentTarget = 'com.forgeshape.app.test/androidx.test.runner.AndroidJUnitRunner'

if ($FullSharded) {
    Write-Output "Discovering the complete live AndroidJUnitRunner inventory on -s $Serial ..."
    $discoveryOutput = @(& adb -s $Serial shell am instrument -w -r -e listTestsForOrchestrator true $instrumentTarget 2>&1 | ForEach-Object { "$_" })
    $discoveryExit = $LASTEXITCODE
    try {
        $discovery = ConvertFrom-InstrumentedDiscoveryOutput -Lines $discoveryOutput -ExitCode $discoveryExit
        $shards = @(New-InstrumentedShardPlan -Discovery $discovery -ShardCount $ShardCount)
        $assignment = Get-InstrumentedShardAssignmentProof -Discovery $discovery -Shards $shards
    } catch {
        & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
        Stop-InstrumentedRun "Full-sharded discovery/partition proof failed: $($_.Exception.Message)"
    }
    if (-not $assignment.Pass) {
        & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
        Stop-InstrumentedRun "Assignment proof failed: missing=$($assignment.MissingCount), duplicates=$($assignment.DuplicateCount), unexpected=$($assignment.UnexpectedCount)."
    }

    Write-Output "DISCOVERY_PASS | classes=$($discovery.ClassCount) | tests=$($discovery.TestCount) | shards=$ShardCount | missing=0 | duplicates=0 | unexpected=0"
    $results = @()
    $executionStopped = $false
    foreach ($shard in $shards) {
        $filter = $shard.Classes -join ','
        if ($executionStopped) {
            $results += [pscustomobject]@{
                ShardIndex = $shard.Index; ExpectedTestCount = $shard.TestCount; ActualTestCount = 0
                ExitCode = 1; Status = 'NOT_RUN'; Reason = 'an earlier shard invalidated the aggregate'; Pass = $false
            }
            continue
        }

        Write-Output "SHARD_START | shard=$($shard.Index)/$ShardCount | classes=$($shard.ClassCount) | tests=$($shard.TestCount) | filter=$filter"
        $shardOutput = @(& adb -s $Serial shell am instrument -w -e class $filter $instrumentTarget 2>&1 | ForEach-Object { "$_" })
        $shardExit = $LASTEXITCODE
        $shardOutput | ForEach-Object { Write-Output $_ }
        $result = Test-InstrumentedShardResult -ShardIndex $shard.Index -ExpectedTestCount $shard.TestCount -Lines $shardOutput -ExitCode $shardExit
        $results += $result
        Write-Output "SHARD_RESULT | shard=$($shard.Index) | expected=$($result.ExpectedTestCount) | actual=$($result.ActualTestCount) | status=$($result.Status) | reason=$($result.Reason)"
        if (-not $result.Pass) { $executionStopped = $true }
    }

    $aggregate = New-FullShardedAggregate -Discovery $discovery -Shards $shards -Results $results
    Write-Output 'FULL_SHARDED_SUMMARY_BEGIN'
    Write-Output "discovered_classes=$($discovery.ClassCount)"
    Write-Output "discovered_tests=$($discovery.TestCount)"
    Write-Output "shard_count=$ShardCount"
    foreach ($shard in $shards) {
        $result = $results | Where-Object ShardIndex -eq $shard.Index | Select-Object -First 1
        Write-Output "shard_$($shard.Index)_classes=$($shard.ClassCount)"
        Write-Output "shard_$($shard.Index)_tests=$($shard.TestCount)"
        Write-Output "shard_$($shard.Index)_filter=$($shard.Classes -join ',')"
        Write-Output "shard_$($shard.Index)_status=$($result.Status)"
    }
    Write-Output "assigned_union=$($assignment.AssignedUnionCount)"
    Write-Output "executed_union=$($aggregate.ExecutedUnionCount)"
    Write-Output "missing=$($assignment.MissingCount)"
    Write-Output "duplicates=$($assignment.DuplicateCount)"
    Write-Output "unexpected=$($assignment.UnexpectedCount)"
    Write-Output "execution_missing=$($aggregate.ExecutionMissingCount)"
    Write-Output "failed_shards=$($aggregate.FailedShardCount)"
    Write-Output "aborted_shards=$($aggregate.AbortedShardCount)"
    Write-Output "aggregate=$(if ($aggregate.Pass) { 'PASS' } else { 'FAIL' })"
    Write-Output (Get-FullSuiteMarker -IsFullSharded $true -AggregatePass $aggregate.Pass)
    Write-Output 'FULL_SHARDED_SUMMARY_END'

    Write-Output "Uninstalling test APK from -s $Serial ..."
    & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
    if (-not $aggregate.Pass) { exit 1 }
    exit 0
}

$instrumentArgs = @('-s', $Serial, 'shell', 'am', 'instrument', '-w')
if (-not [string]::IsNullOrWhiteSpace($TestClass)) {
    $instrumentArgs += @('-e', 'class', $TestClass)
    Write-Output 'MODE=FOCUSED_SUBSET (not full-suite evidence)'
} else {
    Write-Output 'MODE=MONOLITHIC_FULL (supported alternate full-suite path)'
}
$instrumentArgs += $instrumentTarget
Write-Output "Running instrumentation on -s $Serial only: adb $($instrumentArgs -join ' ')"
$testOutput = @(& adb @instrumentArgs 2>&1 | ForEach-Object { "$_" })
$testExit = $LASTEXITCODE
$testOutput | ForEach-Object { Write-Output $_ }

Write-Output "Uninstalling test APK from -s $Serial ..."
& adb -s $Serial uninstall com.forgeshape.app.test | Out-Null

$testText = $testOutput -join "`n"
$hasOneOk = [regex]::Matches($testText, '(?m)^OK\s+\(\d+\s+tests?\)\s*$').Count -eq 1
$hasFailure = $testText -match 'INSTRUMENTATION_ABORTED|INSTRUMENTATION_FAILED|Process crashed|shortMsg=|FAILURES!!!|(?m)^Tests run:.*Failures:'
if ($testExit -ne 0 -or -not $hasOneOk -or $hasFailure) {
    Write-Error -Message "Instrumentation did not return one valid successful result on '$Serial'." -ErrorAction Continue
    exit $(if ($testExit -ne 0) { $testExit } else { 1 })
}
exit 0
