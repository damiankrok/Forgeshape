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

    TEST-RUNTIME-R1 adds control over HOW an aggregate is run without weakening
    what its PASS means:

      -PlanOnly    discover, partition, fingerprint and print the plan; run no
                   instrumentation at all. Combined with -Resume it is a dry run
                   of that resume: it validates the checkpoint and prints which
                   shards would be skipped and which would run.
      -ShardOnly N execute exactly one shard. Never emits an aggregate marker.
      -Resume      continue an aggregate from its checkpoint, skipping only the
                   shards that already PASSED for the IDENTICAL tested
                   fingerprint. Any change to the app APK, the test APK, the
                   discovered inventory, the partition, the shard count or the
                   device refuses the resume by name.
      -Fresh       archive an existing checkpoint and start a new attempt.

    A run writes a checkpoint into its own run directory after every shard, so
    a failure no longer costs the shards that already passed. The runner never
    starts a fresh aggregate from shard 1 by itself: on a failure it stops,
    classifies, and prints the exact command that reruns the failing shard and
    the exact command that resumes.

    THE LIMIT THAT MATTERS: a resume never combines shards across changed app
    or test APK bytes. Edit a test helper and the previously passed shards
    describe a tree that no longer exists; the runner refuses to reuse them,
    and a final authoritative aggregate needs one fresh run.

    TEST-OWNER-02 budgets apply: a warning at the target (default 90 minutes)
    and a stop at the hard budget (default 120 minutes), and at most two
    automatic aggregate attempts per tested fingerprint.

.PARAMETER Serial
    Exact adb serial to use. emulator-5554 is forbidden before adb is contacted.

.PARAMETER TestClass
    Optional fully-qualified test class, or Class#method. Mutually exclusive
    with -FullSharded.

.PARAMETER FullSharded
    Run the exhaustive-sharded suite. With -PlanOnly, -ShardOnly or -Resume it
    controls which part of that suite this invocation performs.

.PARAMETER ShardCount
    Number of deterministic class-atomic shards for -FullSharded (default 5).

.PARAMETER ShardOnly
    Execute only this shard of the plan. Subset evidence; never an aggregate.

.PARAMETER Resume
    Continue from a checkpoint of the identical tested fingerprint.

.PARAMETER Fresh
    Archive any existing checkpoint for this fingerprint and start a new attempt.

.PARAMETER PlanOnly
    Discover, partition, fingerprint and print the plan; execute no shard.

.PARAMETER CheckpointPath
    Explicit checkpoint file. Defaults to the run directory for the fingerprint.

.PARAMETER RunDirectory
    Where the checkpoint and per-shard logs are written. Defaults to
    artifacts\instrumented-runs\<fingerprint-short-id>.

.PARAMETER TargetMinutes
    Aggregate soft budget; a warning, not a failure. Default 90.

.PARAMETER HardStopMinutes
    Aggregate hard budget; stops launching shards. Default 120.

.PARAMETER OwnerOverrideAttemptLimit
    Allow a third or later automatic aggregate attempt for one fingerprint.
    Logged loudly wherever it is used.

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5 -Fresh

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5 -ShardOnly 5

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5 -Resume

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5 -PlanOnly

.EXAMPLE
    scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.EditorWorkspaceLayoutTest
#>
param(
    [Parameter(Mandatory = $true)][string]$Serial,
    [Parameter(Mandatory = $false)][string]$TestClass,
    [Parameter(Mandatory = $false)][switch]$FullSharded,
    [Parameter(Mandatory = $false)][ValidateRange(1, 64)][int]$ShardCount = 5,
    [Parameter(Mandatory = $false)][ValidateRange(1, 64)][int]$ShardOnly = 0,
    [Parameter(Mandatory = $false)][switch]$Resume,
    [Parameter(Mandatory = $false)][switch]$Fresh,
    [Parameter(Mandatory = $false)][switch]$PlanOnly,
    [Parameter(Mandatory = $false)][string]$CheckpointPath,
    [Parameter(Mandatory = $false)][string]$RunDirectory,
    [Parameter(Mandatory = $false)][double]$TargetMinutes = 90,
    [Parameter(Mandatory = $false)][double]$HardStopMinutes = 120,
    [Parameter(Mandatory = $false)][switch]$OwnerOverrideAttemptLimit
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'instrumented-sharding.ps1')
. (Join-Path $PSScriptRoot 'instrumented-runtime.ps1')

# An aggregate marker belongs only to a run that was TRYING to be an aggregate.
# A plan-only or shard-only invocation must never print one, in either
# direction, because a reader greps for these tokens.
$script:AggregateModeActive = ($FullSharded -and -not $PlanOnly -and $ShardOnly -eq 0)

function Stop-InstrumentedRun {
    param([string]$Message, [int]$Code = 1)
    if ($script:AggregateModeActive) {
        Write-Output 'FULL_SHARDED_SUMMARY_BEGIN'
        Write-Output 'aggregate=FAIL'
        Write-Output "reason=$Message"
        Write-Output (Get-FullSuiteMarker -IsFullSharded $true -AggregatePass $false)
        Write-Output 'FULL_SHARDED_SUMMARY_END'
    } elseif ($FullSharded) {
        Write-Output "RUN_ABORTED | mode=$(if ($PlanOnly) { 'PLAN_ONLY' } elseif ($ShardOnly -gt 0) { 'SHARD_ONLY' } else { 'FULL_SHARDED' }) | reason=$Message"
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
foreach ($shardedOnly in @('ShardOnly', 'Resume', 'Fresh', 'PlanOnly', 'CheckpointPath', 'OwnerOverrideAttemptLimit')) {
    if (-not $FullSharded -and $PSBoundParameters.ContainsKey($shardedOnly)) {
        Stop-InstrumentedRun "-$shardedOnly is valid only with -FullSharded."
    }
}
if ($PlanOnly -and $ShardOnly -gt 0) { Stop-InstrumentedRun '-PlanOnly and -ShardOnly are mutually exclusive.' }
if ($Resume -and $Fresh) { Stop-InstrumentedRun '-Resume and -Fresh are mutually exclusive.' }
if ($ShardOnly -gt 0 -and $ShardOnly -gt $ShardCount) {
    Stop-InstrumentedRun "-ShardOnly $ShardOnly is outside a $ShardCount-shard plan."
}
try {
    Assert-InstrumentedBudget -TargetMinutes $TargetMinutes -HardStopMinutes $HardStopMinutes | Out-Null
} catch {
    Stop-InstrumentedRun $_.Exception.Message
}

$ForbiddenSerials = @('emulator-5554')
if ([string]::IsNullOrWhiteSpace($Serial)) {
    Stop-InstrumentedRun "Usage: run-instrumented-tests.ps1 -Serial <adb-serial> [-FullSharded [-ShardCount <n>] [-PlanOnly | -ShardOnly <n> | -Resume | -Fresh] | -TestClass <fully.qualified.ClassName>]`nNo default target is chosen -- an explicit -Serial is required every time."
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
    $overheadWatch = [System.Diagnostics.Stopwatch]::StartNew()

    # The APKs that were actually installed a moment ago are what the result
    # will describe, so they are hashed here rather than inferred from a commit.
    $appApkSha = Get-InstrumentedFileSha256 -Path $appApk
    $testApkSha = Get-InstrumentedFileSha256 -Path $testApk

    # Device identity, for the fingerprint. An emulator answers `emu avd name`;
    # anything else falls back to its model. Both calls are serial-scoped, and
    # a device that answers neither still fingerprints by serial alone.
    $deviceIdentity = ''
    $avdName = & adb -s $Serial emu avd name 2>$null | Select-Object -First 1
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($avdName)) {
        $deviceIdentity = "avd:$($avdName.Trim())"
    } else {
        $model = & adb -s $Serial shell getprop ro.product.model 2>$null | Select-Object -First 1
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($model)) {
            $deviceIdentity = "model:$($model.Trim())"
        }
    }

    Write-Output "Discovering the complete live AndroidJUnitRunner inventory on -s $Serial ..."
    $discoveryStart = $overheadWatch.Elapsed
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
    $discoverySeconds = [math]::Round(($overheadWatch.Elapsed - $discoveryStart).TotalSeconds, 1)

    $fingerprintStart = $overheadWatch.Elapsed
    $fingerprint = New-InstrumentedFingerprint -AppApkSha256 $appApkSha -TestApkSha256 $testApkSha `
        -Discovery $discovery -Shards $shards -ShardCount $ShardCount -DeviceSerial $Serial `
        -DeviceIdentity $deviceIdentity -InstrumentTarget $instrumentTarget
    $fingerprintSeconds = [math]::Round(($overheadWatch.Elapsed - $fingerprintStart).TotalSeconds, 2)

    if ([string]::IsNullOrWhiteSpace($RunDirectory)) {
        $RunDirectory = Join-Path 'artifacts' (Join-Path 'instrumented-runs' $fingerprint.ShortId)
    }
    if (-not (Test-Path -LiteralPath $RunDirectory)) {
        New-Item -ItemType Directory -Force -Path $RunDirectory | Out-Null
    }
    if ([string]::IsNullOrWhiteSpace($CheckpointPath)) {
        $CheckpointPath = Join-Path $RunDirectory 'checkpoint.json'
    }

    # -PlanOnly wins over -Resume for what this invocation DOES (nothing), and
    # -Resume still decides what the plan is judged against, so an operator can
    # ask "what would a resume actually run?" without running it.
    $mode = 'FULL_AGGREGATE'
    if ($PlanOnly) { $mode = 'PLAN_ONLY' }
    elseif ($ShardOnly -gt 0) { $mode = 'SHARD_ONLY' }
    elseif ($Resume) { $mode = 'RESUME' }
    $resumeSemantics = ($Resume -and ($mode -eq 'RESUME' -or $mode -eq 'PLAN_ONLY'))

    Write-Output "DISCOVERY_PASS | classes=$($discovery.ClassCount) | tests=$($discovery.TestCount) | shards=$ShardCount | missing=0 | duplicates=0 | unexpected=0"
    Write-Output "FINGERPRINT | id=$($fingerprint.ShortId) | app=$($appApkSha.Substring(0,12)) | test=$($testApkSha.Substring(0,12)) | inventory=$($fingerprint.InventorySha256.Substring(0,12)) | partition=$($fingerprint.PartitionSha256.Substring(0,12)) | device=$Serial | identity=$deviceIdentity"
    Write-Output "MODE=$mode | checkpoint=$CheckpointPath"

    # An existing checkpoint is read for two independent reasons: to resume
    # from it, and to know how many aggregate attempts this fingerprint has
    # already had. A corrupt or future-schema checkpoint fails closed in both.
    $existingCheckpoint = $null
    if (Test-Path -LiteralPath $CheckpointPath) {
        try {
            $existingCheckpoint = Read-InstrumentedCheckpoint -Path $CheckpointPath
        } catch {
            if ($mode -eq 'RESUME') {
                & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
                Stop-InstrumentedRun "Resume refused: $($_.Exception.Message)"
            }
            Write-Output "CHECKPOINT_UNREADABLE | $($_.Exception.Message) | it is archived rather than trusted"
            $archived = Join-Path $RunDirectory ("checkpoint.unreadable.{0}.json" -f (Get-Date -Format 'yyyyMMddHHmmss'))
            Move-Item -LiteralPath $CheckpointPath -Destination $archived -Force
            $existingCheckpoint = $null
        }
    }

    if ($Fresh -and $null -ne $existingCheckpoint) {
        $archived = Join-Path $RunDirectory ("checkpoint.attempt$($existingCheckpoint.aggregateAttempt).{0}.json" -f (Get-Date -Format 'yyyyMMddHHmmss'))
        Copy-Item -LiteralPath $CheckpointPath -Destination $archived -Force
        Write-Output "CHECKPOINT_ARCHIVED | $archived"
    }

    $priorResults = @()
    $resumeState = $null
    if ($resumeSemantics) {
        if ($null -eq $existingCheckpoint) {
            & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
            Stop-InstrumentedRun "Resume refused: no checkpoint at '$CheckpointPath'."
        }
        $resumeState = Test-InstrumentedCheckpointResumable -Checkpoint $existingCheckpoint -Fingerprint $fingerprint
        if (-not $resumeState.Resumable) {
            Write-Output "RESUME_REFUSED | $($resumeState.Reason)"
            Write-Output 'RESUME_REFUSED_EXPLANATION | the shards that passed belong to a tree these bytes are not; a final aggregate needs one fresh run (-Fresh).'
            & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
            Stop-InstrumentedRun "Resume refused: $($resumeState.Reason)"
        }
        $priorResults = @(ConvertFrom-InstrumentedCheckpointShardResults -Checkpoint $existingCheckpoint)
        Write-Output "RESUME_ACCEPTED | fingerprint=$($fingerprint.ShortId) | carried_pass_shards=$($priorResults.Count)"
    }

    $attemptSourceCheckpoint = $(if ($Fresh) { $existingCheckpoint } else { $existingCheckpoint })
    $attempt = Get-InstrumentedAttemptDecision -Mode $mode -Checkpoint $attemptSourceCheckpoint `
        -Fingerprint $fingerprint -OwnerOverride ([bool]$OwnerOverrideAttemptLimit)
    Write-Output "AGGREGATE_ATTEMPT | mode=$mode | attempt=$($attempt.Attempt) | counted=$($attempt.Counted) | $($attempt.Reason)"
    if ($attempt.OverrideUsed) {
        Write-Output 'OWNER_OVERRIDE_USED | the automatic aggregate-attempt limit was bypassed by an explicit parameter'
    }
    if (-not $attempt.Allowed) {
        & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
        Stop-InstrumentedRun $attempt.Reason
    }

    $planMode = $(if ($resumeSemantics) { 'RESUME' } elseif ($mode -eq 'SHARD_ONLY') { 'SHARD_ONLY' } else { 'FULL_AGGREGATE' })
    try {
        $plan = @(Get-InstrumentedExecutionPlan -Shards $shards -Mode $planMode `
            -Checkpoint $(if ($resumeSemantics) { $existingCheckpoint } else { $null }) -ShardOnly $ShardOnly)
    } catch {
        & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
        Stop-InstrumentedRun "Execution plan failed: $($_.Exception.Message)"
    }
    foreach ($entry in $plan) {
        Write-Output "PLAN | shard=$($entry.Index)/$ShardCount | classes=$($entry.ClassCount) | tests=$($entry.TestCount) | action=$($entry.Action) | $($entry.Reason) | filter=$($entry.Classes -join ',')"
    }

    $overheadSeconds = [math]::Round($overheadWatch.Elapsed.TotalSeconds, 1)
    Write-Output "RUNNER_OVERHEAD | total=${overheadSeconds}s | discovery=${discoverySeconds}s | fingerprint=${fingerprintSeconds}s"

    if ($mode -eq 'PLAN_ONLY') {
        Write-Output "PLAN_ONLY_COMPLETE | resume_semantics=$resumeSemantics | shards_that_would_run=$(@($plan | Where-Object Action -eq 'RUN').Count) | shards_that_would_be_skipped=$(@($plan | Where-Object Action -eq 'SKIP_PASS').Count) | no instrumentation was executed"
        Write-Output "Uninstalling test APK from -s $Serial ..."
        & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
        exit 0
    }

    # The checkpoint is created before the first shard runs, so a run killed
    # part way through still leaves an accurate record of what passed.
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode $mode `
        -AggregateAttempt $attempt.Attempt -ShardResults (ConvertTo-InstrumentedCheckpointShardResults -Results $priorResults)
    Save-InstrumentedCheckpoint -Checkpoint $checkpoint -Path $CheckpointPath | Out-Null

    $script:LiveResults = @($priorResults)
    $onShardComplete = {
        param($result)
        $script:LiveResults = @($script:LiveResults | Where-Object { $_.ShardIndex -ne $result.ShardIndex }) + @($result)
        $checkpoint.shardResults = @(ConvertTo-InstrumentedCheckpointShardResults -Results $script:LiveResults)
        Save-InstrumentedCheckpoint -Checkpoint $checkpoint -Path $CheckpointPath | Out-Null
        # Written straight to stdout, not through Write-Output: this callback
        # runs inside the sequence function, so anything it emits would be
        # collected as that function's return value instead of being logged.
        [Console]::Out.WriteLine("SHARD_RESULT | shard=$($result.ShardIndex) | expected=$($result.ExpectedTestCount) | actual=$($result.ActualTestCount) | status=$($result.Status) | class=$($result.Classification) | duration=$($result.DurationSeconds)s | reason=$($result.Reason)")
    }

    $executor = {
        param($shard)
        $filter = $shard.Classes -join ','
        # Same reason as the callback above: this scriptblock is invoked from
        # inside the sequence function, so its transcript goes to stdout
        # directly and only the result object is returned.
        [Console]::Out.WriteLine("SHARD_START | shard=$($shard.Index)/$ShardCount | classes=$($shard.ClassCount) | tests=$($shard.TestCount) | filter=$filter")
        $shardOutput = @(& adb -s $Serial shell am instrument -w -e class $filter $instrumentTarget 2>&1 | ForEach-Object { "$_" })
        $shardExit = $LASTEXITCODE
        $shardOutput | ForEach-Object { [Console]::Out.WriteLine($_) }
        $shardLog = Join-Path $RunDirectory "shard-$($shard.Index).log"
        [System.IO.File]::WriteAllLines($shardLog, [string[]]$shardOutput, (New-Object System.Text.UTF8Encoding($false)))
        return [pscustomobject]@{ Lines = $shardOutput; ExitCode = $shardExit }
    }

    $sequence = Invoke-InstrumentedShardSequence -Plan $plan -Shards $shards -Executor $executor `
        -OnShardComplete $onShardComplete -TargetMinutes $TargetMinutes -HardStopMinutes $HardStopMinutes `
        -PriorResults $priorResults

    $results = @($sequence.Results)
    if ($sequence.Budget.TargetExceeded) {
        Write-Output "TIME_BUDGET_WARNING | elapsed=$($sequence.Budget.ElapsedMinutes)m reached the ${TargetMinutes}m target; the hard stop is ${HardStopMinutes}m"
    }

    # SHARD-ONLY: subset evidence. It reports its shard and stops, and no
    # aggregate marker is printed in either direction.
    if ($mode -eq 'SHARD_ONLY') {
        $one = $results | Where-Object ShardIndex -eq $ShardOnly | Select-Object -First 1
        Write-Output 'SHARD_ONLY_SUMMARY_BEGIN'
        Write-Output "fingerprint=$($fingerprint.ShortId)"
        Write-Output "shard=$ShardOnly"
        Write-Output "status=$($one.Status)"
        Write-Output "classification=$($one.Classification)"
        Write-Output "duration_seconds=$($one.DurationSeconds)"
        Write-Output "checkpoint=$CheckpointPath"
        Write-Output 'note=shard-only is subset evidence and can never complete or repair an aggregate'
        Write-Output "next=scripts\run-instrumented-tests.ps1 -Serial $Serial -FullSharded -ShardCount $ShardCount -Resume"
        Write-Output 'SHARD_ONLY_SUMMARY_END'
        Write-Output "Uninstalling test APK from -s $Serial ..."
        & adb -s $Serial uninstall com.forgeshape.app.test | Out-Null
        if (-not $one.Pass) { exit 1 }
        exit 0
    }

    $aggregate = New-FullShardedAggregate -Discovery $discovery -Shards $shards -Results $results
    Write-Output 'FULL_SHARDED_SUMMARY_BEGIN'
    Write-Output "fingerprint=$($fingerprint.ShortId)"
    Write-Output "app_apk_sha256=$appApkSha"
    Write-Output "test_apk_sha256=$testApkSha"
    Write-Output "inventory_sha256=$($fingerprint.InventorySha256)"
    Write-Output "partition_sha256=$($fingerprint.PartitionSha256)"
    Write-Output "device=$Serial"
    Write-Output "device_identity=$deviceIdentity"
    Write-Output "mode=$mode"
    Write-Output "aggregate_attempt=$($attempt.Attempt)"
    Write-Output "discovered_classes=$($discovery.ClassCount)"
    Write-Output "discovered_tests=$($discovery.TestCount)"
    Write-Output "shard_count=$ShardCount"
    foreach ($shard in $shards) {
        $result = $results | Where-Object ShardIndex -eq $shard.Index | Select-Object -First 1
        $carried = ($null -ne $result -and ($result.PSObject.Properties.Name -contains 'CarriedFromCheckpoint'))
        Write-Output "shard_$($shard.Index)_classes=$($shard.ClassCount)"
        Write-Output "shard_$($shard.Index)_tests=$($shard.TestCount)"
        Write-Output "shard_$($shard.Index)_filter=$($shard.Classes -join ',')"
        Write-Output "shard_$($shard.Index)_status=$(if ($null -eq $result) { 'NOT_RUN' } else { $result.Status })"
        Write-Output "shard_$($shard.Index)_classification=$(if ($null -eq $result) { 'NOT_RUN' } else { $result.Classification })"
        Write-Output "shard_$($shard.Index)_duration_seconds=$(if ($null -eq $result) { 0 } else { $result.DurationSeconds })"
        Write-Output "shard_$($shard.Index)_carried_from_checkpoint=$carried"
    }
    Write-Output "assigned_union=$($assignment.AssignedUnionCount)"
    Write-Output "executed_union=$($aggregate.ExecutedUnionCount)"
    Write-Output "missing=$($assignment.MissingCount)"
    Write-Output "duplicates=$($assignment.DuplicateCount)"
    Write-Output "unexpected=$($assignment.UnexpectedCount)"
    Write-Output "execution_missing=$($aggregate.ExecutionMissingCount)"
    Write-Output "failed_shards=$($aggregate.FailedShardCount)"
    Write-Output "aborted_shards=$($aggregate.AbortedShardCount)"
    Write-Output "elapsed_minutes=$($sequence.Budget.ElapsedMinutes)"
    Write-Output "target_minutes=$TargetMinutes"
    Write-Output "hard_stop_minutes=$HardStopMinutes"
    Write-Output "checkpoint=$CheckpointPath"
    Write-Output "aggregate=$(if ($aggregate.Pass) { 'PASS' } else { 'FAIL' })"

    if (-not $aggregate.Pass) {
        # PART I: no blind restart. The operator is told exactly which shard
        # broke, what kind of failure it was, and the two commands that matter.
        $firstBad = @($results | Where-Object { -not $_.Pass } | Sort-Object ShardIndex | Select-Object -First 1)
        if ($firstBad.Count -eq 1) {
            $bad = $firstBad[0]
            Write-Output "FAILED_SHARD | shard=$($bad.ShardIndex) | status=$($bad.Status) | classification=$($bad.Classification)"
            Write-Output "FAILED_SHARD_LOG | $(Join-Path $RunDirectory "shard-$($bad.ShardIndex).log")"
            Write-Output "NEXT_RERUN_SHARD | scripts\run-instrumented-tests.ps1 -Serial $Serial -FullSharded -ShardCount $ShardCount -ShardOnly $($bad.ShardIndex)"
            Write-Output "NEXT_RESUME | scripts\run-instrumented-tests.ps1 -Serial $Serial -FullSharded -ShardCount $ShardCount -Resume"
            Write-Output 'RESUME_CAVEAT | editing any product or test source rebuilds the APKs and changes the fingerprint, which refuses the resume; a final aggregate then needs one fresh run (-Fresh).'
            Write-Output 'NO_AUTOMATIC_RESTART | this runner does not start a new aggregate from shard 1 by itself'
        }
    }
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
