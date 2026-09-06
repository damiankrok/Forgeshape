<#
.SYNOPSIS
    TESTRUNTIME-01..24: the runner's fingerprint, checkpoint, resume, budget,
    attempt and classification logic, proved without a device.

.DESCRIPTION
    Every case here runs against synthetic fixtures with the shard executor and
    the clock injected, so the whole matrix takes seconds. That is the point:
    the behaviour this stage adds is exactly the behaviour that used to need a
    two-and-a-half-hour aggregate to observe.

    Two cases deliberately invoke the real runner script against the reserved
    serial, which it refuses before contacting adb. They prove what no
    synthetic fixture can: that the shipped script itself cannot print an
    aggregate marker from a shard-only or plan-only invocation.
#>
param()

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'instrumented-sharding.ps1')
. (Join-Path $PSScriptRoot 'instrumented-runtime.ps1')

$results = @()

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Invoke-RuntimeCheck {
    param([string]$Id, [string]$Name, [scriptblock]$Body)
    try {
        & $Body
        $script:results += [pscustomobject]@{ Id = $Id; Name = $Name; Pass = $true; Evidence = 'assertions passed' }
    } catch {
        $script:results += [pscustomobject]@{ Id = $Id; Name = $Name; Pass = $false; Evidence = $_.Exception.Message }
    }
}

function New-DiscoveryFixture {
    param([hashtable]$Extra = $null)
    $definitions = [ordered]@{
        'com.forgeshape.AlphaTest' = @('a1', 'a2', 'a3')
        'com.forgeshape.BetaTest' = @('b1', 'b2')
        'com.forgeshape.GammaTest' = @('g1')
        'com.forgeshape.DeltaTest' = @('d1', 'd2')
    }
    if ($null -ne $Extra) {
        foreach ($key in $Extra.Keys) { $definitions[$key] = $Extra[$key] }
    }
    $total = 0
    foreach ($key in $definitions.Keys) { $total += $definitions[$key].Count }
    $current = 0
    $lines = @()
    foreach ($className in $definitions.Keys) {
        foreach ($testName in $definitions[$className]) {
            $current++
            $lines += "INSTRUMENTATION_STATUS: class=$className"
            $lines += "INSTRUMENTATION_STATUS: current=$current"
            $lines += 'INSTRUMENTATION_STATUS: id=AndroidJUnitRunner'
            $lines += "INSTRUMENTATION_STATUS: numtests=$total"
            $lines += "INSTRUMENTATION_STATUS: test=$testName"
            $lines += 'INSTRUMENTATION_STATUS_CODE: 1'
        }
    }
    $lines += "OK ($total tests)"
    $lines += 'INSTRUMENTATION_CODE: -1'
    return $lines
}

function New-Fingerprint {
    param(
        [string]$AppSha = 'a' * 64,
        [string]$TestSha = 'b' * 64,
        $Discovery = $null,
        $Shards = $null,
        [int]$ShardCount = 2,
        [string]$Serial = 'emulator-5580',
        [string]$Identity = 'avd:ForgeShape_Stage006'
    )
    if ($null -eq $Discovery) { $Discovery = $script:discovery }
    if ($null -eq $Shards) { $Shards = $script:plan2 }
    return New-InstrumentedFingerprint -AppApkSha256 $AppSha -TestApkSha256 $TestSha `
        -Discovery $Discovery -Shards $Shards -ShardCount $ShardCount -DeviceSerial $Serial `
        -DeviceIdentity $Identity -InstrumentTarget 'com.forgeshape.app.test/androidx.test.runner.AndroidJUnitRunner'
}

function New-TempRunDirectory {
    $path = Join-Path ([System.IO.Path]::GetTempPath()) ("forgeshape-testruntime-" + [System.Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $path | Out-Null
    return $path
}

# A clock that jumps a fixed number of minutes on every reading, so a budget
# case can simulate hours without waiting for them.
function New-SteppingClock {
    param([double]$StepMinutes)
    $state = [pscustomobject]@{ Base = (Get-Date '2026-01-01T00:00:00Z'); Ticks = 0; Step = $StepMinutes }
    return @{
        Clock = { $state.Ticks++; return $state.Base.AddMinutes($state.Step * ($state.Ticks - 1)) }.GetNewClosure()
        State = $state
    }
}

function New-PassLines { param([int]$Count) return @("OK ($Count tests)") }

$script:discovery = ConvertFrom-InstrumentedDiscoveryOutput -Lines (New-DiscoveryFixture) -ExitCode 0
$script:plan2 = @(New-InstrumentedShardPlan -Discovery $script:discovery -ShardCount 2)
$script:runnerScript = Join-Path $PSScriptRoot 'run-instrumented-tests.ps1'
$script:engine = (Get-Process -Id $PID).Path

# --------------------------------------------------------------------------
# Fingerprint identity
# --------------------------------------------------------------------------

Invoke-RuntimeCheck 'TESTRUNTIME-01' 'fingerprint is stable for the same APKs and inventory' {
    $a = New-Fingerprint
    $b = New-Fingerprint
    Assert-True ($a.FingerprintSha256 -ceq $b.FingerprintSha256) 'the same inputs produced two fingerprints'
    Assert-True ($a.ShortId.Length -eq 12) 'the short id is not 12 characters'
    # Re-planning the same discovery must not move it either.
    $replanned = @(New-InstrumentedShardPlan -Discovery $script:discovery -ShardCount 2)
    $c = New-Fingerprint -Shards $replanned
    Assert-True ($a.FingerprintSha256 -ceq $c.FingerprintSha256) 'an identical re-plan changed the fingerprint'
}

Invoke-RuntimeCheck 'TESTRUNTIME-02' 'a changed app APK invalidates the checkpoint' {
    $original = New-Fingerprint
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $original -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    $changed = New-Fingerprint -AppSha ('c' * 64)
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $changed
    Assert-True (-not $state.Resumable) 'a changed app APK was accepted for resume'
    Assert-True ($state.Reasons -contains 'RESUME_INVALID_APP_APK_CHANGED') "reason was '$($state.Reason)'"
}

Invoke-RuntimeCheck 'TESTRUNTIME-03' 'a changed test APK invalidates the checkpoint' {
    $original = New-Fingerprint
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $original -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    $changed = New-Fingerprint -TestSha ('d' * 64)
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $changed
    Assert-True (-not $state.Resumable) 'a changed test APK was accepted for resume'
    Assert-True ($state.Reasons -contains 'RESUME_INVALID_TEST_APK_CHANGED') "reason was '$($state.Reason)'"
}

Invoke-RuntimeCheck 'TESTRUNTIME-04' 'a changed test inventory invalidates the checkpoint' {
    $original = New-Fingerprint
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $original -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    $widerLines = New-DiscoveryFixture -Extra @{ 'com.forgeshape.EpsilonTest' = @('e1') }
    $wider = ConvertFrom-InstrumentedDiscoveryOutput -Lines $widerLines -ExitCode 0
    $widerPlan = @(New-InstrumentedShardPlan -Discovery $wider -ShardCount 2)
    $changed = New-Fingerprint -Discovery $wider -Shards $widerPlan
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $changed
    Assert-True (-not $state.Resumable) 'a changed inventory was accepted for resume'
    Assert-True ($state.Reasons -contains 'RESUME_INVALID_TEST_INVENTORY_CHANGED') "reason was '$($state.Reason)'"
}

Invoke-RuntimeCheck 'TESTRUNTIME-05' 'a changed partition invalidates the checkpoint' {
    $original = New-Fingerprint
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $original -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    # The same tests, deliberately assigned the other way round.
    $swapped = @(
        [pscustomobject]@{ Index = 1; Classes = @($script:plan2[1].Classes); ClassCount = $script:plan2[1].ClassCount; TestIds = @($script:plan2[1].TestIds); TestCount = $script:plan2[1].TestCount }
        [pscustomobject]@{ Index = 2; Classes = @($script:plan2[0].Classes); ClassCount = $script:plan2[0].ClassCount; TestIds = @($script:plan2[0].TestIds); TestCount = $script:plan2[0].TestCount }
    )
    $changed = New-Fingerprint -Shards $swapped
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $changed
    Assert-True (-not $state.Resumable) 'a changed partition was accepted for resume'
    Assert-True ($state.Reasons -contains 'RESUME_INVALID_PARTITION_CHANGED') "reason was '$($state.Reason)'"
}

Invoke-RuntimeCheck 'TESTRUNTIME-06' 'a changed shard count invalidates the checkpoint' {
    $original = New-Fingerprint
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $original -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    $plan3 = @(New-InstrumentedShardPlan -Discovery $script:discovery -ShardCount 3)
    $changed = New-Fingerprint -Shards $plan3 -ShardCount 3
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $changed
    Assert-True (-not $state.Resumable) 'a changed shard count was accepted for resume'
    Assert-True ($state.Reasons -contains 'RESUME_INVALID_SHARD_COUNT_CHANGED') "reason was '$($state.Reason)'"
}

Invoke-RuntimeCheck 'TESTRUNTIME-07' 'a different device invalidates the checkpoint' {
    $original = New-Fingerprint
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $original -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    $otherSerial = New-Fingerprint -Serial 'emulator-5600'
    $bySerial = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $otherSerial
    Assert-True (-not $bySerial.Resumable -and $bySerial.Reasons -contains 'RESUME_INVALID_DEVICE_CHANGED') `
        "a different serial was accepted ($($bySerial.Reason))"
    $otherAvd = New-Fingerprint -Identity 'avd:SomeOtherAvd'
    $byIdentity = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $otherAvd
    Assert-True (-not $byIdentity.Resumable -and $byIdentity.Reasons -contains 'RESUME_INVALID_DEVICE_CHANGED') `
        "the same serial on a different AVD was accepted ($($byIdentity.Reason))"
}

# --------------------------------------------------------------------------
# Checkpoint file behaviour
# --------------------------------------------------------------------------

Invoke-RuntimeCheck 'TESTRUNTIME-08' 'a corrupt checkpoint is refused' {
    $dir = New-TempRunDirectory
    try {
        $path = Join-Path $dir 'checkpoint.json'
        Set-Content -LiteralPath $path -Value '{ this is not json' -Encoding UTF8
        $refused = $false
        $message = ''
        try { Read-InstrumentedCheckpoint -Path $path | Out-Null } catch { $refused = $true; $message = $_.Exception.Message }
        Assert-True ($refused -and $message -match 'RESUME_INVALID_CHECKPOINT_CORRUPT') "corrupt checkpoint gave '$message'"

        # Valid JSON that is missing a required field is refused too.
        Set-Content -LiteralPath $path -Value '{ "schemaVersion": 1 }' -Encoding UTF8
        $refused2 = $false
        try { Read-InstrumentedCheckpoint -Path $path | Out-Null } catch { $refused2 = $true }
        Assert-True $refused2 'a checkpoint missing required fields was accepted'
    } finally { Remove-Item -Recurse -Force $dir }
}

Invoke-RuntimeCheck 'TESTRUNTIME-09' 'a future checkpoint schema is refused' {
    $dir = New-TempRunDirectory
    try {
        $path = Join-Path $dir 'checkpoint.json'
        $fingerprint = New-Fingerprint
        $checkpoint = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
        $checkpoint.schemaVersion = 99
        Save-InstrumentedCheckpoint -Checkpoint $checkpoint -Path $path | Out-Null
        $refused = $false
        $message = ''
        try { Read-InstrumentedCheckpoint -Path $path | Out-Null } catch { $refused = $true; $message = $_.Exception.Message }
        Assert-True ($refused -and $message -match 'RESUME_INVALID_CHECKPOINT_SCHEMA') "future schema gave '$message'"
    } finally { Remove-Item -Recurse -Force $dir }
}

Invoke-RuntimeCheck 'TESTRUNTIME-10' 'a checkpoint write replaces atomically' {
    $dir = New-TempRunDirectory
    try {
        $path = Join-Path $dir 'checkpoint.json'
        $fingerprint = New-Fingerprint
        $first = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
        Save-InstrumentedCheckpoint -Checkpoint $first -Path $path | Out-Null
        $second = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 2
        Save-InstrumentedCheckpoint -Checkpoint $second -Path $path | Out-Null
        $read = Read-InstrumentedCheckpoint -Path $path
        Assert-True ([int]$read.aggregateAttempt -eq 2) 'the replacement did not land'
        Assert-True (-not (Test-Path -LiteralPath "$path.writing")) 'the temporary write file survived the replace'
        Assert-True (@(Get-ChildItem -LiteralPath $dir -File).Count -eq 1) 'the replace left more than one file behind'
    } finally { Remove-Item -Recurse -Force $dir }
}

# --------------------------------------------------------------------------
# Plans, shard-only and resume
# --------------------------------------------------------------------------

Invoke-RuntimeCheck 'TESTRUNTIME-11' 'shard-only plans exactly one shard' {
    $plan = @(Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'SHARD_ONLY' -ShardOnly 2)
    Assert-True (@($plan | Where-Object Action -eq 'RUN').Count -eq 1) 'more than one shard was planned to run'
    Assert-True (($plan | Where-Object Action -eq 'RUN').Index -eq 2) 'the wrong shard was selected'
    Assert-True (($plan | Where-Object Index -eq 1).Action -eq 'SKIP_NOT_SELECTED') 'the other shard was not skipped'

    $executed = @()
    $executor = { param($shard) $executed += $shard.Index; return [pscustomobject]@{ Lines = @("OK ($($shard.TestCount) tests)"); ExitCode = 0 } }.GetNewClosure()
    $run = Invoke-InstrumentedShardSequence -Plan $plan -Shards $script:plan2 -Executor $executor
    Assert-True (@($run.Results | Where-Object { $_.Status -eq 'PASS' }).Count -eq 1) 'shard-only produced more than one result'

    $outsideRejected = $false
    try { Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'SHARD_ONLY' -ShardOnly 9 | Out-Null } catch { $outsideRejected = $true }
    Assert-True $outsideRejected 'a shard outside the plan was accepted'
}

Invoke-RuntimeCheck 'TESTRUNTIME-12' 'shard-only cannot emit an aggregate marker' {
    # The real script, refused at the reserved serial before adb is contacted.
    # Its own Write-Error reaches this parent as an error record, and the parent
    # runs under Stop, so the preference is relaxed for exactly these two calls.
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = @(& $script:engine -NoProfile -File $script:runnerScript -Serial emulator-5554 `
        -FullSharded -ShardCount 2 -ShardOnly 1 2>&1 | ForEach-Object { "$_" })
    $exit = $LASTEXITCODE
    $text = $output -join "`n"
    Assert-True ($exit -ne 0) 'the reserved-serial shard-only fixture did not stop before adb'
    Assert-True ($text -notmatch 'FULL_SHARDED_SUITE_PASS') 'a shard-only invocation printed an aggregate PASS'
    Assert-True ($text -notmatch 'FULL_SHARDED_SUITE_FAIL') 'a shard-only invocation printed an aggregate marker at all'
    # And plan-only likewise.
    $planOutput = @(& $script:engine -NoProfile -File $script:runnerScript -Serial emulator-5554 `
        -FullSharded -ShardCount 2 -PlanOnly 2>&1 | ForEach-Object { "$_" })
    $ErrorActionPreference = $previousPreference
    Assert-True ((($planOutput -join "`n") -notmatch 'FULL_SHARDED_SUITE_') ) 'a plan-only invocation printed an aggregate marker'
}

Invoke-RuntimeCheck 'TESTRUNTIME-13' 'a same-tree resume skips the shards that passed' {
    # SEVERAL carried shards, not one: a single carried shard hid a nested-array
    # return that made every count come out as 1.
    $plan3 = @(New-InstrumentedShardPlan -Discovery $script:discovery -ShardCount 3)
    $fingerprint3 = New-Fingerprint -Shards $plan3 -ShardCount 3
    $twoPassed = @(
        [pscustomobject]@{ ShardIndex = 1; ExpectedTestCount = $plan3[0].TestCount; ActualTestCount = $plan3[0].TestCount; ExitCode = 0; Status = 'PASS'; Reason = 'ok'; Pass = $true; Classification = 'PASS'; DurationSeconds = 11 }
        [pscustomobject]@{ ShardIndex = 2; ExpectedTestCount = $plan3[1].TestCount; ActualTestCount = $plan3[1].TestCount; ExitCode = 0; Status = 'PASS'; Reason = 'ok'; Pass = $true; Classification = 'PASS'; DurationSeconds = 12 }
    )
    $checkpoint3 = New-InstrumentedCheckpoint -Fingerprint $fingerprint3 -Mode 'FULL_AGGREGATE' -AggregateAttempt 1 `
        -ShardResults (ConvertTo-InstrumentedCheckpointShardResults -Results $twoPassed)
    Assert-True (@($checkpoint3.shardResults).Count -eq 2) "the checkpoint stored $(@($checkpoint3.shardResults).Count) shard record(s) instead of 2"
    $carried3 = @(ConvertFrom-InstrumentedCheckpointShardResults -Checkpoint $checkpoint3)
    Assert-True ($carried3.Count -eq 2) "two carried PASS shards were restored as $($carried3.Count)"
    $plan3Resume = @(Get-InstrumentedExecutionPlan -Shards $plan3 -Mode 'RESUME' -Checkpoint $checkpoint3)
    Assert-True (@($plan3Resume | Where-Object Action -eq 'SKIP_PASS').Count -eq 2) 'both carried shards were not skipped'
    Assert-True (@($plan3Resume | Where-Object Action -eq 'RUN').Count -eq 1) 'the outstanding shard was not the only one planned'

    $fingerprint = New-Fingerprint
    $passed = @([pscustomobject]@{
        ShardIndex = 1; ExpectedTestCount = $script:plan2[0].TestCount; ActualTestCount = $script:plan2[0].TestCount
        ExitCode = 0; Status = 'PASS'; Reason = 'ok'; Pass = $true; Classification = 'PASS'
        DurationSeconds = 42; StartedAt = $null; FinishedAt = $null
    })
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1 `
        -ShardResults (ConvertTo-InstrumentedCheckpointShardResults -Results $passed)
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $fingerprint
    Assert-True $state.Resumable "an identical fingerprint was refused: $($state.Reason)"

    $plan = @(Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'RESUME' -Checkpoint $checkpoint)
    Assert-True (($plan | Where-Object Index -eq 1).Action -eq 'SKIP_PASS') 'the passed shard was not skipped'
    Assert-True (($plan | Where-Object Index -eq 2).Action -eq 'RUN') 'the unfinished shard was not planned'

    # Deliberately CHATTY: the real executor writes a transcript as it runs, and
    # an earlier version of the sequence took its whole pipeline as the result.
    $ran = New-Object System.Collections.ArrayList
    $executor = {
        param($shard)
        [void]$ran.Add($shard.Index)
        Write-Output "SHARD_START | shard=$($shard.Index)"
        Write-Output 'INSTRUMENTATION_STATUS_CODE: 0'
        return [pscustomobject]@{ Lines = @("OK ($($shard.TestCount) tests)"); ExitCode = 0 }
    }.GetNewClosure()
    $prior = @(ConvertFrom-InstrumentedCheckpointShardResults -Checkpoint $checkpoint)
    $run = Invoke-InstrumentedShardSequence -Plan $plan -Shards $script:plan2 -Executor $executor -PriorResults $prior
    Assert-True ($ran.Count -eq 1 -and $ran[0] -eq 2) "the resume executed shards $($ran -join ',')"
    Assert-True (($run.Results | Where-Object ShardIndex -eq 1).DurationSeconds -eq 42) 'the carried duration was lost'

    # And the carried shard still counts toward the aggregate.
    $aggregate = New-FullShardedAggregate -Discovery $script:discovery -Shards $script:plan2 -Results $run.Results
    Assert-True $aggregate.Pass 'a complete resume did not produce an aggregate PASS'
}

Invoke-RuntimeCheck 'TESTRUNTIME-14' 'a resume reruns the shard that failed' {
    $fingerprint = New-Fingerprint
    $mixed = @(
        [pscustomobject]@{ ShardIndex = 1; ExpectedTestCount = 4; ActualTestCount = 4; ExitCode = 0; Status = 'PASS'; Reason = 'ok'; Pass = $true; Classification = 'PASS'; DurationSeconds = 10 }
        [pscustomobject]@{ ShardIndex = 2; ExpectedTestCount = 4; ActualTestCount = 0; ExitCode = 0; Status = 'ASSERTION_FAILURE'; Reason = 'failed'; Pass = $false; Classification = 'PRODUCT_TEST_FAILURE'; DurationSeconds = 20 }
    )
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1 `
        -ShardResults (ConvertTo-InstrumentedCheckpointShardResults -Results $mixed)
    $plan = @(Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'RESUME' -Checkpoint $checkpoint)
    Assert-True (($plan | Where-Object Index -eq 2).Action -eq 'RUN') 'the failed shard was not rerun'
    Assert-True (($plan | Where-Object Index -eq 1).Action -eq 'SKIP_PASS') 'the passed shard was rerun anyway'
    $carried = @(ConvertFrom-InstrumentedCheckpointShardResults -Checkpoint $checkpoint)
    Assert-True ($carried.Count -eq 1) 'a non-PASS shard was carried forward as if it had passed'
}

Invoke-RuntimeCheck 'TESTRUNTIME-15' 'a resume reruns a shard that never ran' {
    $fingerprint = New-Fingerprint
    $partial = @(
        [pscustomobject]@{ ShardIndex = 1; ExpectedTestCount = 4; ActualTestCount = 4; ExitCode = 0; Status = 'PASS'; Reason = 'ok'; Pass = $true; Classification = 'PASS'; DurationSeconds = 10 }
        [pscustomobject]@{ ShardIndex = 2; ExpectedTestCount = 4; ActualTestCount = 0; ExitCode = 0; Status = 'NOT_RUN'; Reason = 'stopped'; Pass = $false; Classification = 'NOT_RUN'; DurationSeconds = 0 }
    )
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1 `
        -ShardResults (ConvertTo-InstrumentedCheckpointShardResults -Results $partial)
    $plan = @(Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'RESUME' -Checkpoint $checkpoint)
    Assert-True (($plan | Where-Object Index -eq 2).Action -eq 'RUN') 'a NOT_RUN shard was not scheduled'
}

Invoke-RuntimeCheck 'TESTRUNTIME-16' 'a changed tree cannot reuse a passed shard' {
    $fingerprint = New-Fingerprint
    $passed = @([pscustomobject]@{
        ShardIndex = 1; ExpectedTestCount = 4; ActualTestCount = 4; ExitCode = 0; Status = 'PASS'
        Reason = 'ok'; Pass = $true; Classification = 'PASS'; DurationSeconds = 10
    })
    $checkpoint = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1 `
        -ShardResults (ConvertTo-InstrumentedCheckpointShardResults -Results $passed)
    # The exact case the brief calls critical: a test helper was edited, so the
    # test APK's bytes moved. The passed shard describes a tree that is gone.
    $afterEdit = New-Fingerprint -TestSha ('f' * 64)
    $state = Test-InstrumentedCheckpointResumable -Checkpoint $checkpoint -Fingerprint $afterEdit
    Assert-True (-not $state.Resumable) 'a rebuilt test APK still allowed the old PASS to be reused'
    Assert-True ($state.Reasons -contains 'RESUME_INVALID_TEST_APK_CHANGED') "reason was '$($state.Reason)'"
    Assert-True ($afterEdit.FingerprintSha256 -cne $fingerprint.FingerprintSha256) 'the fingerprint did not move with the APK'
}

# --------------------------------------------------------------------------
# Budgets
# --------------------------------------------------------------------------

Invoke-RuntimeCheck 'TESTRUNTIME-17' 'the target budget warns and does not stop' {
    $state = Get-InstrumentedBudgetState -ElapsedSeconds (95 * 60) -TargetMinutes 90 -HardStopMinutes 120
    Assert-True ($state.TargetExceeded -and -not $state.HardStopReached) 'the 90-minute target did not behave as a warning'
    Assert-True ($state.RemainingMinutes -eq 25) "remaining was $($state.RemainingMinutes)"

    # A run that crosses the target still executes every shard.
    $clock = New-SteppingClock -StepMinutes 20
    $plan = @(Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'FULL_AGGREGATE')
    $ran = New-Object System.Collections.ArrayList
    $executor = { param($shard) [void]$ran.Add($shard.Index); return [pscustomobject]@{ Lines = @("OK ($($shard.TestCount) tests)"); ExitCode = 0 } }.GetNewClosure()
    $run = Invoke-InstrumentedShardSequence -Plan $plan -Shards $script:plan2 -Executor $executor `
        -Clock $clock.Clock -TargetMinutes 30 -HardStopMinutes 600
    Assert-True ($ran.Count -eq 2) "crossing the target stopped the run after $($ran.Count) shard(s)"
    Assert-True ($run.Budget.TargetExceeded) 'the target was not reported as exceeded'
}

Invoke-RuntimeCheck 'TESTRUNTIME-18' 'the hard stop stops launching shards' {
    $clock = New-SteppingClock -StepMinutes 40
    $plan = @(Get-InstrumentedExecutionPlan -Shards $script:plan2 -Mode 'FULL_AGGREGATE')
    $ran = New-Object System.Collections.ArrayList
    $executor = { param($shard) [void]$ran.Add($shard.Index); return [pscustomobject]@{ Lines = @("OK ($($shard.TestCount) tests)"); ExitCode = 0 } }.GetNewClosure()
    $run = Invoke-InstrumentedShardSequence -Plan $plan -Shards $script:plan2 -Executor $executor `
        -Clock $clock.Clock -TargetMinutes 30 -HardStopMinutes 60
    Assert-True ($ran.Count -eq 1) "the hard stop let $($ran.Count) shard(s) start"
    $stopped = $run.Results | Where-Object Status -eq 'TIME_BUDGET_EXCEEDED'
    Assert-True ($null -ne $stopped) 'no shard was marked TIME_BUDGET_EXCEEDED'
    Assert-True ((Get-InstrumentedFailureClassification -Status 'TIME_BUDGET_EXCEEDED') -eq 'TIME_BUDGET_EXCEEDED') `
        'the time-budget status did not classify as TIME_BUDGET_EXCEEDED'
    $aggregate = New-FullShardedAggregate -Discovery $script:discovery -Shards $script:plan2 -Results $run.Results
    Assert-True (-not $aggregate.Pass) 'a time-stopped run still produced an aggregate PASS'
}

Invoke-RuntimeCheck 'TESTRUNTIME-19' 'invalid budget parameters are refused' {
    foreach ($case in @(
        @{ Target = 0; Hard = 120 }
        @{ Target = -5; Hard = 120 }
        @{ Target = 90; Hard = 30 }
        @{ Target = 'abc'; Hard = 120 }
        @{ Target = 90; Hard = 5000 }
        @{ Target = [double]::NaN; Hard = 120 }
    )) {
        $refused = $false
        try { Assert-InstrumentedBudget -TargetMinutes $case.Target -HardStopMinutes $case.Hard | Out-Null } catch { $refused = $true }
        Assert-True $refused "budget target=$($case.Target) hard=$($case.Hard) was accepted"
    }
    Assert-True (Assert-InstrumentedBudget -TargetMinutes 90 -HardStopMinutes 120) 'the documented defaults were refused'
    Assert-True (Assert-InstrumentedBudget -TargetMinutes 45 -HardStopMinutes 45) 'an equal target and hard stop was refused'
}

# --------------------------------------------------------------------------
# Aggregate attempts
# --------------------------------------------------------------------------

Invoke-RuntimeCheck 'TESTRUNTIME-20' 'the first two aggregate attempts are allowed' {
    $fingerprint = New-Fingerprint
    $first = Get-InstrumentedAttemptDecision -Mode 'FULL_AGGREGATE' -Checkpoint $null -Fingerprint $fingerprint
    Assert-True ($first.Allowed -and $first.Attempt -eq 1 -and $first.Counted) 'the first attempt was not allowed as attempt 1'
    $afterFirst = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 1
    $second = Get-InstrumentedAttemptDecision -Mode 'FULL_AGGREGATE' -Checkpoint $afterFirst -Fingerprint $fingerprint
    Assert-True ($second.Allowed -and $second.Attempt -eq 2) 'the second attempt was not allowed'

    # Diagnostics cost no attempt.
    foreach ($mode in @('SHARD_ONLY', 'PLAN_ONLY', 'RESUME')) {
        $diagnostic = Get-InstrumentedAttemptDecision -Mode $mode -Checkpoint $afterFirst -Fingerprint $fingerprint
        Assert-True ($diagnostic.Allowed -and -not $diagnostic.Counted) "$mode consumed an aggregate attempt"
    }
    # A different fingerprint starts its own count.
    $other = New-Fingerprint -AppSha ('9' * 64)
    $fresh = Get-InstrumentedAttemptDecision -Mode 'FULL_AGGREGATE' -Checkpoint $afterFirst -Fingerprint $other
    Assert-True ($fresh.Attempt -eq 1) 'a different fingerprint inherited another subject''s attempt count'
}

Invoke-RuntimeCheck 'TESTRUNTIME-21' 'a third aggregate attempt needs an explicit override' {
    $fingerprint = New-Fingerprint
    $afterSecond = New-InstrumentedCheckpoint -Fingerprint $fingerprint -Mode 'FULL_AGGREGATE' -AggregateAttempt 2
    $refused = Get-InstrumentedAttemptDecision -Mode 'FULL_AGGREGATE' -Checkpoint $afterSecond -Fingerprint $fingerprint
    Assert-True (-not $refused.Allowed) 'a third automatic attempt was allowed'
    Assert-True ($refused.Reason -match 'ATTEMPT_LIMIT_REACHED') "reason was '$($refused.Reason)'"
    $overridden = Get-InstrumentedAttemptDecision -Mode 'FULL_AGGREGATE' -Checkpoint $afterSecond `
        -Fingerprint $fingerprint -OwnerOverride $true
    Assert-True ($overridden.Allowed -and $overridden.OverrideUsed) 'the explicit override did not allow a third attempt'
    Assert-True ($overridden.Reason -match 'ATTEMPT_LIMIT_OVERRIDDEN') 'the override was not named in the reason'
}

# --------------------------------------------------------------------------
# Failure classification
# --------------------------------------------------------------------------

Invoke-RuntimeCheck 'TESTRUNTIME-22' 'infrastructure signatures classify as infrastructure' {
    $abort = Test-InstrumentedShardResult -ShardIndex 1 -ExpectedTestCount 4 `
        -Lines @('com.forgeshape.app.SculptUndoTest:.........INSTRUMENTATION_ABORTED: System has crashed.') -ExitCode 0
    Assert-True ((Get-InstrumentedFailureClassification -Status $abort.Status -Lines @('INSTRUMENTATION_ABORTED: System has crashed.')) -eq 'INFRASTRUCTURE_FAILURE') `
        'an instrumentation abort was not classified as infrastructure'
    foreach ($line in @('error: device offline', 'adb: device emulator-5580 not found', 'INSTALL_FAILED_INSUFFICIENT_STORAGE', 'error: closed')) {
        $classified = Get-InstrumentedFailureClassification -Status 'RUNNER_FAILURE' -Lines @($line) -ExitCode 1
        Assert-True ($classified -eq 'INFRASTRUCTURE_FAILURE') "'$line' classified as $classified"
    }
}

Invoke-RuntimeCheck 'TESTRUNTIME-23' 'an assertion failure classifies as a product failure' {
    $lines = @('FAILURES!!!', 'Tests run: 108,  Failures: 1')
    $result = Test-InstrumentedShardResult -ShardIndex 1 -ExpectedTestCount 108 -Lines $lines -ExitCode 0
    Assert-True ($result.Status -eq 'ASSERTION_FAILURE') "status was $($result.Status)"
    Assert-True ((Get-InstrumentedFailureClassification -Status $result.Status -Lines $lines) -eq 'PRODUCT_TEST_FAILURE') `
        'an assertion failure was not classified as a product failure'
    # And a PASS stays a PASS.
    $ok = Test-InstrumentedShardResult -ShardIndex 1 -ExpectedTestCount 4 -Lines (New-PassLines -Count 4) -ExitCode 0
    Assert-True ((Get-InstrumentedFailureClassification -Status $ok.Status -Lines (New-PassLines -Count 4)) -eq 'PASS') 'a passing shard was classified as a failure'
}

Invoke-RuntimeCheck 'TESTRUNTIME-24' 'ambiguous evidence fails closed' {
    # A non-zero exit with no marker at all attributes nothing to the product.
    $bare = Get-InstrumentedFailureClassification -Status 'RUNNER_FAILURE' -Lines @('') -ExitCode 7
    Assert-True ($bare -eq 'RUNNER_ERROR') "a bare non-zero exit classified as $bare"
    $missing = Test-InstrumentedShardResult -ShardIndex 1 -ExpectedTestCount 4 -Lines @('INSTRUMENTATION_CODE: -1') -ExitCode 0
    Assert-True ((Get-InstrumentedFailureClassification -Status $missing.Status -Lines @('INSTRUMENTATION_CODE: -1')) -eq 'RUNNER_ERROR') `
        'a missing result was attributed rather than failing closed'
    $mismatch = Test-InstrumentedShardResult -ShardIndex 1 -ExpectedTestCount 9 -Lines (New-PassLines -Count 4) -ExitCode 0
    Assert-True ($mismatch.Status -eq 'COUNT_MISMATCH') "status was $($mismatch.Status)"
    Assert-True ((Get-InstrumentedFailureClassification -Status $mismatch.Status -Lines (New-PassLines -Count 4)) -eq 'RUNNER_ERROR') `
        'a count mismatch was attributed rather than failing closed'
    $unknown = Get-InstrumentedFailureClassification -Status 'SOMETHING_NEW' -Lines @('nothing familiar here')
    Assert-True ($unknown -eq 'RUNNER_ERROR') "an unknown status classified as $unknown"
}

$results | ForEach-Object {
    $status = if ($_.Pass) { 'PASS' } else { 'FAIL' }
    Write-Output "$($_.Id) | $status | $($_.Name) | $($_.Evidence)"
}
$failed = @($results | Where-Object { -not $_.Pass })
if ($failed.Count -ne 0) {
    Write-Error -Message "$($failed.Count) TESTRUNTIME check(s) failed." -ErrorAction Continue
    exit 1
}
Write-Output "TESTRUNTIME-01..24 PASS ($($results.Count) checks)."
exit 0
