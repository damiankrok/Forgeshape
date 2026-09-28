<#
    TEST-RUNTIME-R1: the runner's fingerprint, checkpoint, resume, budget,
    attempt-accounting and failure-classification logic.

    Everything here is PURE. No adb call, no Gradle call, no device contact and
    no file outside the run directory it is handed. That is deliberate and it is
    what makes `test-instrumented-runtime.ps1` able to prove the whole of
    TESTRUNTIME-01..26 in seconds against synthetic fixtures, with the shard
    executor injected as a scriptblock, instead of needing a five-shard
    aggregate on a device.

    The one rule the rest of this file exists to protect:

        FULL_SHARDED_SUITE_PASS may be emitted only when every shard
        contributing to it belongs to the SAME tested fingerprint.

    A resume therefore never combines shards across a changed app APK, test
    APK, inventory, partition, shard count or device. When any of those moves,
    the previous PASS shards describe a tree that no longer exists and the
    runner refuses to reuse them by name.
#>

Set-StrictMode -Version 2.0

# The checkpoint format's own version. A checkpoint written by a NEWER runner
# is refused rather than guessed at: an older reader cannot know what a field
# it has never heard of constrains.
$script:InstrumentedCheckpointSchemaVersion = 1

# TEST-OWNER-02 defaults, in minutes. The target is a warning and the hard stop
# is a stop; see Get-InstrumentedBudgetState.
$script:InstrumentedDefaultTargetMinutes = 90
$script:InstrumentedDefaultHardStopMinutes = 120

# The most automatic full aggregate attempts allowed per tested fingerprint.
# A third needs an explicit, logged owner override.
$script:InstrumentedMaxAggregateAttempts = 2

<#
    Whether this host is Windows.

    Windows PowerShell 5.1 (the Desktop edition) runs only on Windows and has no
    `$IsWindows`, which under strict mode is an error to read, so the edition
    decides first and the variable is only looked up on PowerShell 7.
#>
function Test-InstrumentedHostIsWindows {
    if ($PSVersionTable.PSEdition -eq 'Desktop') { return $true }
    return [bool](Get-Variable -Name IsWindows -ValueOnly -ErrorAction SilentlyContinue)
}

<#
    The Gradle wrapper call that builds the two APKs.

    The runner's one platform difference, and nothing else: Windows runs
    `gradlew.bat`; everywhere else the POSIX wrapper runs through `bash`,
    because the checked-in `gradlew` carries no executable bit. The tasks are
    the same two on both, so the APKs a run describes are built the same way.
#>
function Get-InstrumentedGradleInvocation {
    param([Parameter(Mandatory = $true)][bool]$OnWindows)
    $tasks = @(':app:assembleDebug', ':app:assembleDebugAndroidTest')
    if ($OnWindows) {
        return [pscustomobject]@{ Command = '.\gradlew.bat'; Arguments = $tasks }
    }
    return [pscustomobject]@{ Command = 'bash'; Arguments = @('./gradlew') + $tasks }
}

<#
    Where the build leaves the two APKs, relative to the repository root.

    Built with the host's own separator because both paths are handed to
    `adb install` as native arguments, which no PowerShell provider rewrites:
    on Windows they are exactly the backslash paths the runner always used.
#>
function Get-InstrumentedApkPaths {
    [pscustomobject]@{
        App = [System.IO.Path]::Combine('app', 'build', 'outputs', 'apk', 'debug', 'app-debug.apk')
        Test = [System.IO.Path]::Combine('app', 'build', 'outputs', 'apk', 'androidTest', 'debug', 'app-debug-androidTest.apk')
    }
}

function Get-InstrumentedSha256OfText {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes)) -replace '-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Get-InstrumentedFileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Cannot hash '$Path': the file does not exist."
    }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead((Resolve-Path -LiteralPath $Path).ProviderPath)
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($stream)) -replace '-', '').ToLowerInvariant()
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
}

<#
    The inventory, as one canonical sorted text.

    One line per discovered test, carrying the shard it was assigned to, so a
    test moving between shards changes this hash even when the inventory itself
    is identical. Sorted ordinally, so two machines and two PowerShell versions
    produce the same bytes.
#>
function Get-InstrumentedInventoryText {
    param(
        [Parameter(Mandatory = $true)]$Discovery,
        [Parameter(Mandatory = $true)][object[]]$Shards
    )
    $shardOfTest = @{}
    foreach ($shard in $Shards) {
        foreach ($testId in $shard.TestIds) {
            if ($shardOfTest.ContainsKey($testId)) {
                throw "Inventory canonicalization found '$testId' in more than one shard."
            }
            $shardOfTest[$testId] = $shard.Index
        }
    }
    $lines = @()
    foreach ($test in $Discovery.Tests) {
        $assigned = if ($shardOfTest.ContainsKey($test.TestId)) { $shardOfTest[$test.TestId] } else { 0 }
        $lines += "$($test.TestId)|$assigned"
    }
    return (($lines | Sort-Object -CaseSensitive) -join "`n")
}

<# The partition, as one canonical sorted text: one line per shard and class. #>
function Get-InstrumentedPartitionText {
    param([Parameter(Mandatory = $true)][object[]]$Shards)
    $lines = @()
    foreach ($shard in $Shards) {
        foreach ($className in $shard.Classes) {
            $lines += "$($shard.Index)|$className"
        }
    }
    return (($lines | Sort-Object -CaseSensitive) -join "`n")
}

<#
    The tested-tree fingerprint.

    Git HEAD is deliberately NOT part of it. ForgeShape routinely tests a dirty
    candidate before it is committed, so a commit hash would call two different
    trees the same tree. What identifies the tested tree is the BYTES that were
    installed and the inventory they produced: the app APK's hash, the test
    APK's hash, the discovered inventory with its shard assignment, the
    partition, the shard count, the instrumentation target, and the device the
    result was produced on.
#>
function New-InstrumentedFingerprint {
    param(
        [Parameter(Mandatory = $true)][string]$AppApkSha256,
        [Parameter(Mandatory = $true)][string]$TestApkSha256,
        [Parameter(Mandatory = $true)]$Discovery,
        [Parameter(Mandatory = $true)][object[]]$Shards,
        [Parameter(Mandatory = $true)][int]$ShardCount,
        [Parameter(Mandatory = $true)][string]$DeviceSerial,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$DeviceIdentity,
        [Parameter(Mandatory = $true)][string]$InstrumentTarget
    )

    $inventoryText = Get-InstrumentedInventoryText -Discovery $Discovery -Shards $Shards
    $partitionText = Get-InstrumentedPartitionText -Shards $Shards
    $inventorySha = Get-InstrumentedSha256OfText -Text $inventoryText
    $partitionSha = Get-InstrumentedSha256OfText -Text $partitionText

    # Field-per-line, named, in a fixed order. Not JSON: JSON property order is
    # a serializer's choice and this must be a byte-stable input.
    $canonical = @(
        "appApkSha256=$AppApkSha256"
        "testApkSha256=$TestApkSha256"
        "inventorySha256=$inventorySha"
        "partitionSha256=$partitionSha"
        "shardCount=$ShardCount"
        "instrumentTarget=$InstrumentTarget"
        "deviceSerial=$DeviceSerial"
        "deviceIdentity=$DeviceIdentity"
        "classCount=$($Discovery.ClassCount)"
        "testCount=$($Discovery.TestCount)"
    ) -join "`n"

    $fingerprint = Get-InstrumentedSha256OfText -Text $canonical
    [pscustomobject]@{
        AppApkSha256 = $AppApkSha256
        TestApkSha256 = $TestApkSha256
        InventorySha256 = $inventorySha
        PartitionSha256 = $partitionSha
        ShardCount = $ShardCount
        InstrumentTarget = $InstrumentTarget
        DeviceSerial = $DeviceSerial
        DeviceIdentity = $DeviceIdentity
        ClassCount = $Discovery.ClassCount
        TestCount = $Discovery.TestCount
        FingerprintSha256 = $fingerprint
        ShortId = $fingerprint.Substring(0, 12)
        CanonicalText = $canonical
    }
}

function New-InstrumentedCheckpoint {
    param(
        [Parameter(Mandatory = $true)]$Fingerprint,
        [Parameter(Mandatory = $true)][string]$Mode,
        [Parameter(Mandatory = $true)][int]$AggregateAttempt,
        [Parameter(Mandatory = $false)][object[]]$ShardResults = @()
    )
    [pscustomobject]@{
        schemaVersion = $script:InstrumentedCheckpointSchemaVersion
        testedTreeFingerprint = $Fingerprint.FingerprintSha256
        fingerprintShortId = $Fingerprint.ShortId
        appApkSha256 = $Fingerprint.AppApkSha256
        testApkSha256 = $Fingerprint.TestApkSha256
        inventorySha256 = $Fingerprint.InventorySha256
        partitionSha256 = $Fingerprint.PartitionSha256
        shardCount = $Fingerprint.ShardCount
        deviceSerial = $Fingerprint.DeviceSerial
        deviceIdentity = $Fingerprint.DeviceIdentity
        instrumentTarget = $Fingerprint.InstrumentTarget
        discoveredClasses = $Fingerprint.ClassCount
        discoveredTests = $Fingerprint.TestCount
        mode = $Mode
        aggregateAttempt = $AggregateAttempt
        startedAt = (Get-Date).ToUniversalTime().ToString('o')
        updatedAt = (Get-Date).ToUniversalTime().ToString('o')
        shardResults = @($ShardResults)
    }
}

<#
    Writes the checkpoint atomically: a temp file beside it, then a replace.

    A half-written checkpoint is worse than none — it would be read on the next
    resume as authority over which shards may be skipped.
#>
function Save-InstrumentedCheckpoint {
    param(
        [Parameter(Mandatory = $true)]$Checkpoint,
        [Parameter(Mandatory = $true)][string]$Path
    )
    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    $Checkpoint.updatedAt = (Get-Date).ToUniversalTime().ToString('o')
    $temp = "$Path.writing"
    $json = $Checkpoint | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($temp, $json, (New-Object System.Text.UTF8Encoding($false)))
    Move-Item -LiteralPath $temp -Destination $Path -Force
    return $Path
}

function Read-InstrumentedCheckpoint {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "RESUME_INVALID_CHECKPOINT_MISSING: no checkpoint at '$Path'."
    }
    $raw = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    try {
        $checkpoint = $raw | ConvertFrom-Json
    } catch {
        throw "RESUME_INVALID_CHECKPOINT_CORRUPT: '$Path' is not readable JSON."
    }
    if ($null -eq $checkpoint) {
        throw "RESUME_INVALID_CHECKPOINT_CORRUPT: '$Path' contained no object."
    }
    foreach ($required in @('schemaVersion', 'testedTreeFingerprint', 'appApkSha256', 'testApkSha256',
            'inventorySha256', 'partitionSha256', 'shardCount', 'deviceSerial', 'aggregateAttempt',
            'shardResults')) {
        if (-not ($checkpoint.PSObject.Properties.Name -contains $required)) {
            throw "RESUME_INVALID_CHECKPOINT_CORRUPT: '$Path' is missing '$required'."
        }
    }
    if ([int]$checkpoint.schemaVersion -gt $script:InstrumentedCheckpointSchemaVersion) {
        throw ("RESUME_INVALID_CHECKPOINT_SCHEMA: checkpoint schema $($checkpoint.schemaVersion) is newer " +
            "than this runner's $($script:InstrumentedCheckpointSchemaVersion).")
    }
    if ([int]$checkpoint.schemaVersion -lt 1) {
        throw "RESUME_INVALID_CHECKPOINT_SCHEMA: checkpoint schema $($checkpoint.schemaVersion) is not supported."
    }
    return $checkpoint
}

<#
    Whether a checkpoint may be resumed against the fingerprint just computed.

    Every mismatch has its own name, because "resume refused" without a reason
    is what makes an operator start again from shard 1 — the loop this stage
    exists to end.
#>
function Test-InstrumentedCheckpointResumable {
    param(
        [Parameter(Mandatory = $true)]$Checkpoint,
        [Parameter(Mandatory = $true)]$Fingerprint
    )
    $reasons = @()
    if ($Checkpoint.appApkSha256 -ne $Fingerprint.AppApkSha256) { $reasons += 'RESUME_INVALID_APP_APK_CHANGED' }
    if ($Checkpoint.testApkSha256 -ne $Fingerprint.TestApkSha256) { $reasons += 'RESUME_INVALID_TEST_APK_CHANGED' }
    if ($Checkpoint.inventorySha256 -ne $Fingerprint.InventorySha256) { $reasons += 'RESUME_INVALID_TEST_INVENTORY_CHANGED' }
    if ($Checkpoint.partitionSha256 -ne $Fingerprint.PartitionSha256) { $reasons += 'RESUME_INVALID_PARTITION_CHANGED' }
    if ([int]$Checkpoint.shardCount -ne [int]$Fingerprint.ShardCount) { $reasons += 'RESUME_INVALID_SHARD_COUNT_CHANGED' }
    if ($Checkpoint.deviceSerial -ne $Fingerprint.DeviceSerial) { $reasons += 'RESUME_INVALID_DEVICE_CHANGED' }
    if (($Checkpoint.PSObject.Properties.Name -contains 'deviceIdentity') -and
        $Checkpoint.deviceIdentity -ne $Fingerprint.DeviceIdentity) {
        $reasons += 'RESUME_INVALID_DEVICE_CHANGED'
    }
    # The composite is checked last and independently: if the parts all match
    # but the whole does not, something canonicalized differently and the
    # honest answer is to refuse rather than to trust the parts.
    if ($reasons.Count -eq 0 -and $Checkpoint.testedTreeFingerprint -ne $Fingerprint.FingerprintSha256) {
        $reasons += 'RESUME_INVALID_FINGERPRINT_MISMATCH'
    }
    $unique = @($reasons | Select-Object -Unique)
    [pscustomobject]@{
        Resumable = ($unique.Count -eq 0)
        Reasons = $unique
        Reason = if ($unique.Count -eq 0) { 'same tested fingerprint' } else { $unique -join ',' }
    }
}

<#
    Which shards this invocation will actually execute.

    RUN / SKIP_PASS / SKIP_NOT_SELECTED, one per shard, decided before anything
    is launched, so -PlanOnly can print exactly what a real run would do.
    A shard is skipped as already-passed ONLY from a checkpoint that
    Test-InstrumentedCheckpointResumable accepted.
#>
function Get-InstrumentedExecutionPlan {
    param(
        [Parameter(Mandatory = $true)][object[]]$Shards,
        [Parameter(Mandatory = $true)][ValidateSet('FULL_AGGREGATE', 'RESUME', 'SHARD_ONLY')][string]$Mode,
        [Parameter(Mandatory = $false)]$Checkpoint = $null,
        [Parameter(Mandatory = $false)][int]$ShardOnly = 0
    )

    $passedIndexes = @()
    if ($Mode -eq 'RESUME') {
        if ($null -eq $Checkpoint) { throw 'RESUME requires a validated checkpoint.' }
        foreach ($entry in @($Checkpoint.shardResults)) {
            if ($entry.status -eq 'PASS') { $passedIndexes += [int]$entry.shardNumber }
        }
    }

    if ($Mode -eq 'SHARD_ONLY') {
        $indexes = @($Shards | ForEach-Object { $_.Index })
        if ($indexes -notcontains $ShardOnly) {
            throw "Shard $ShardOnly is not in the plan (shards: $($indexes -join ', '))."
        }
    }

    $plan = @()
    foreach ($shard in $Shards) {
        $action = 'RUN'
        $why = 'selected'
        if ($Mode -eq 'SHARD_ONLY' -and $shard.Index -ne $ShardOnly) {
            $action = 'SKIP_NOT_SELECTED'
            $why = 'shard-only run of another shard'
        } elseif ($Mode -eq 'RESUME' -and $passedIndexes -contains $shard.Index) {
            $action = 'SKIP_PASS'
            $why = 'already PASS for this exact fingerprint'
        }
        $plan += [pscustomobject]@{
            Index = $shard.Index
            ClassCount = $shard.ClassCount
            TestCount = $shard.TestCount
            Classes = @($shard.Classes)
            Action = $action
            Reason = $why
        }
    }
    return $plan
}

<#
    Maps a shard's raw status onto a conservative category.

    The distinction that matters: a PRODUCT_TEST_FAILURE is claimed only when
    instrumentation actually produced attributable assertion evidence. A
    non-zero exit, a missing result or a count mismatch is NOT product
    evidence — it is a runner or environment problem, and calling it a product
    failure sends somebody to read code that never ran.
#>
function Get-InstrumentedFailureClassification {
    param(
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $false)][AllowEmptyCollection()][string[]]$Lines = @(),
        [Parameter(Mandatory = $false)][int]$ExitCode = 0
    )
    $text = ($Lines -join "`n")
    $infrastructureSignature =
        'INSTRUMENTATION_ABORTED|System has crashed|Process crashed|shortMsg=|' +
        'device (?:offline|not found|unauthorized)|error: closed|' +
        'no devices/emulators found|INSTALL_FAILED|Failed to (?:stat|install)|' +
        'adb: device .* not found|instrumentation process (?:died|crashed)'

    switch ($Status) {
        'PASS' { return 'PASS' }
        'NOT_RUN' { return 'NOT_RUN' }
        'SKIPPED_PASS' { return 'PASS' }
        'USER_ABORTED' { return 'USER_ABORTED' }
        'TIME_BUDGET_EXCEEDED' { return 'TIME_BUDGET_EXCEEDED' }
        'INSTRUMENTATION_ABORT' { return 'INFRASTRUCTURE_FAILURE' }
        'ASSERTION_FAILURE' { return 'PRODUCT_TEST_FAILURE' }
        default {
            if ($text -match $infrastructureSignature) { return 'INFRASTRUCTURE_FAILURE' }
            # RUNNER_FAILURE / MISSING_RESULT / COUNT_MISMATCH and anything new:
            # ambiguous evidence fails closed rather than being attributed.
            return 'RUNNER_ERROR'
        }
    }
}

function Assert-InstrumentedBudget {
    param(
        [Parameter(Mandatory = $true)]$TargetMinutes,
        [Parameter(Mandatory = $true)]$HardStopMinutes
    )
    foreach ($pair in @(@{ Name = 'TargetMinutes'; Value = $TargetMinutes }, @{ Name = 'HardStopMinutes'; Value = $HardStopMinutes })) {
        $value = $pair.Value
        if ($null -eq $value) { throw "BUDGET_INVALID: $($pair.Name) is required." }
        $numeric = 0.0
        if (-not [double]::TryParse([string]$value, [ref]$numeric)) {
            throw "BUDGET_INVALID: $($pair.Name) '$value' is not a number."
        }
        if ([double]::IsNaN($numeric) -or [double]::IsInfinity($numeric)) {
            throw "BUDGET_INVALID: $($pair.Name) must be finite."
        }
        if ($numeric -le 0) { throw "BUDGET_INVALID: $($pair.Name) must be greater than zero." }
        if ($numeric -gt 1440) { throw "BUDGET_INVALID: $($pair.Name) must not exceed 1440 minutes (24 h)." }
    }
    if ([double]$HardStopMinutes -lt [double]$TargetMinutes) {
        throw "BUDGET_INVALID: HardStopMinutes ($HardStopMinutes) must be greater than or equal to TargetMinutes ($TargetMinutes)."
    }
    return $true
}

function Get-InstrumentedBudgetState {
    param(
        [Parameter(Mandatory = $true)][double]$ElapsedSeconds,
        [Parameter(Mandatory = $true)][double]$TargetMinutes,
        [Parameter(Mandatory = $true)][double]$HardStopMinutes
    )
    $elapsedMinutes = $ElapsedSeconds / 60.0
    [pscustomobject]@{
        ElapsedMinutes = [math]::Round($elapsedMinutes, 2)
        TargetMinutes = $TargetMinutes
        HardStopMinutes = $HardStopMinutes
        TargetExceeded = ($elapsedMinutes -ge $TargetMinutes)
        HardStopReached = ($elapsedMinutes -ge $HardStopMinutes)
        RemainingMinutes = [math]::Round(($HardStopMinutes - $elapsedMinutes), 2)
    }
}

<#
    Whether this invocation may start another full aggregate attempt.

    Counted per tested fingerprint, not globally: a new APK is a new subject
    and starts its own count. A resume continues an attempt rather than
    starting one, and shard-only and plan-only runs are diagnostics that cost
    no attempt at all.
#>
function Get-InstrumentedAttemptDecision {
    param(
        [Parameter(Mandatory = $true)][ValidateSet('FULL_AGGREGATE', 'RESUME', 'SHARD_ONLY', 'PLAN_ONLY')][string]$Mode,
        [Parameter(Mandatory = $false)]$Checkpoint = $null,
        [Parameter(Mandatory = $false)]$Fingerprint = $null,
        [Parameter(Mandatory = $false)][bool]$OwnerOverride = $false
    )
    if ($Mode -ne 'FULL_AGGREGATE') {
        $carried = 0
        if ($null -ne $Checkpoint -and ($Checkpoint.PSObject.Properties.Name -contains 'aggregateAttempt')) {
            $carried = [int]$Checkpoint.aggregateAttempt
        }
        return [pscustomobject]@{
            Allowed = $true
            Attempt = $carried
            Counted = $false
            Reason = "$Mode does not consume a full aggregate attempt"
            OverrideUsed = $false
        }
    }

    $previous = 0
    if ($null -ne $Checkpoint -and ($Checkpoint.PSObject.Properties.Name -contains 'aggregateAttempt')) {
        # Only a checkpoint for the SAME fingerprint carries a count forward.
        $sameSubject = $true
        if ($null -ne $Fingerprint) {
            $sameSubject = ($Checkpoint.testedTreeFingerprint -eq $Fingerprint.FingerprintSha256)
        }
        if ($sameSubject) { $previous = [int]$Checkpoint.aggregateAttempt }
    }
    $next = $previous + 1
    if ($next -le $script:InstrumentedMaxAggregateAttempts) {
        return [pscustomobject]@{
            Allowed = $true
            Attempt = $next
            Counted = $true
            Reason = "attempt $next of $($script:InstrumentedMaxAggregateAttempts) for this fingerprint"
            OverrideUsed = $false
        }
    }
    if ($OwnerOverride) {
        return [pscustomobject]@{
            Allowed = $true
            Attempt = $next
            Counted = $true
            Reason = "ATTEMPT_LIMIT_OVERRIDDEN: attempt $next exceeds the limit of $($script:InstrumentedMaxAggregateAttempts) and was allowed by an explicit owner override"
            OverrideUsed = $true
        }
    }
    return [pscustomobject]@{
        Allowed = $false
        Attempt = $next
        Counted = $false
        Reason = "ATTEMPT_LIMIT_REACHED: $($script:InstrumentedMaxAggregateAttempts) automatic aggregate attempts have already been made for this fingerprint; rerun the failing shard focused, stabilise the tree, or pass the owner override"
        OverrideUsed = $false
    }
}

<#
    Runs the plan, one shard at a time, and stops on the first non-PASS.

    The executor is injected: on a device it is the runner's scoped
    `adb -s <serial> shell am instrument` call, and in the tooling self-tests it
    is a scriptblock returning canned lines. That is the whole reason
    TESTRUNTIME-11..24 need no device.

    It does NOT restart anything. On a failure it returns what happened and the
    caller writes the checkpoint and tells the operator which single command
    resumes — see PART I of the brief: a blind restart from shard 1 is the loop
    this replaces.
#>
function Invoke-InstrumentedShardSequence {
    param(
        [Parameter(Mandatory = $true)][object[]]$Plan,
        [Parameter(Mandatory = $true)][object[]]$Shards,
        [Parameter(Mandatory = $true)][scriptblock]$Executor,
        [Parameter(Mandatory = $false)][scriptblock]$Clock = { (Get-Date) },
        [Parameter(Mandatory = $false)][scriptblock]$OnShardComplete = $null,
        [Parameter(Mandatory = $false)][double]$TargetMinutes = 90,
        [Parameter(Mandatory = $false)][double]$HardStopMinutes = 120,
        [Parameter(Mandatory = $false)]$PriorResults = @()
    )

    $start = & $Clock
    $results = @()
    $stopped = $false
    $stopReason = ''

    # Shards a resume already proved, carried into this run's result set so the
    # aggregate can be judged over the whole plan rather than over the part
    # this invocation happened to execute.
    foreach ($prior in @($PriorResults)) {
        $results += $prior
    }

    foreach ($entry in $Plan) {
        $shard = $Shards | Where-Object Index -eq $entry.Index | Select-Object -First 1
        if ($entry.Action -ne 'RUN') { continue }

        if ($stopped) {
            $results += [pscustomobject]@{
                ShardIndex = $entry.Index; ExpectedTestCount = $entry.TestCount; ActualTestCount = 0
                ExitCode = 0; Status = 'NOT_RUN'; Reason = $stopReason; Pass = $false
                Classification = 'NOT_RUN'; DurationSeconds = 0; StartedAt = $null; FinishedAt = $null
            }
            continue
        }

        $elapsed = (& $Clock) - $start
        $budget = Get-InstrumentedBudgetState -ElapsedSeconds $elapsed.TotalSeconds `
            -TargetMinutes $TargetMinutes -HardStopMinutes $HardStopMinutes
        if ($budget.HardStopReached) {
            $stopped = $true
            $stopReason = "hard time budget of $HardStopMinutes minute(s) reached before this shard started"
            $results += [pscustomobject]@{
                ShardIndex = $entry.Index; ExpectedTestCount = $entry.TestCount; ActualTestCount = 0
                ExitCode = 0; Status = 'TIME_BUDGET_EXCEEDED'; Reason = $stopReason; Pass = $false
                Classification = 'TIME_BUDGET_EXCEEDED'; DurationSeconds = 0; StartedAt = $null; FinishedAt = $null
            }
            continue
        }

        $shardStart = & $Clock
        # An executor is free to write a transcript as it goes, and the real one
        # does. Everything it emits therefore lands in this pipeline, so the
        # RESULT is the last object carrying a Lines property rather than
        # whatever happened to be emitted last.
        $emitted = @(& $Executor $shard)
        $execution = $emitted |
            Where-Object { $null -ne $_ -and $_.PSObject.Properties.Name -contains 'Lines' } |
            Select-Object -Last 1
        $shardEnd = & $Clock
        $lines = @()
        $exitCode = 0
        if ($null -ne $execution) {
            $lines = @($execution.Lines)
            if ($execution.PSObject.Properties.Name -contains 'ExitCode') { $exitCode = [int]$execution.ExitCode }
        }

        $result = Test-InstrumentedShardResult -ShardIndex $entry.Index -ExpectedTestCount $entry.TestCount `
            -Lines $lines -ExitCode $exitCode
        $classification = Get-InstrumentedFailureClassification -Status $result.Status -Lines $lines -ExitCode $exitCode
        $enriched = [pscustomobject]@{
            ShardIndex = $result.ShardIndex
            ExpectedTestCount = $result.ExpectedTestCount
            ActualTestCount = $result.ActualTestCount
            ExitCode = $result.ExitCode
            Status = $result.Status
            Reason = $result.Reason
            Pass = $result.Pass
            Classification = $classification
            DurationSeconds = [math]::Round(($shardEnd - $shardStart).TotalSeconds, 1)
            StartedAt = $shardStart.ToUniversalTime().ToString('o')
            FinishedAt = $shardEnd.ToUniversalTime().ToString('o')
        }
        $results += $enriched
        if ($null -ne $OnShardComplete) { & $OnShardComplete $enriched }

        if (-not $enriched.Pass) {
            $stopped = $true
            $stopReason = "shard $($entry.Index) ended $($enriched.Status) ($classification)"
        }
    }

    $total = ((& $Clock) - $start)
    [pscustomobject]@{
        Results = @($results | Sort-Object ShardIndex)
        Stopped = $stopped
        StopReason = $stopReason
        TotalSeconds = [math]::Round($total.TotalSeconds, 1)
        Budget = (Get-InstrumentedBudgetState -ElapsedSeconds $total.TotalSeconds -TargetMinutes $TargetMinutes -HardStopMinutes $HardStopMinutes)
    }
}

<# The checkpoint's per-shard records, from this run's result objects. #>
function ConvertTo-InstrumentedCheckpointShardResults {
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$Results)
    $out = @()
    foreach ($result in $Results) {
        $out += [pscustomobject]@{
            shardNumber = $result.ShardIndex
            assignedCount = $result.ExpectedTestCount
            executedCount = $result.ActualTestCount
            status = $result.Status
            failureClassification = $(if ($result.PSObject.Properties.Name -contains 'Classification') { $result.Classification } else { 'RUNNER_ERROR' })
            durationSeconds = $(if ($result.PSObject.Properties.Name -contains 'DurationSeconds') { $result.DurationSeconds } else { 0 })
            startedAt = $(if ($result.PSObject.Properties.Name -contains 'StartedAt') { $result.StartedAt } else { $null })
            finishedAt = $(if ($result.PSObject.Properties.Name -contains 'FinishedAt') { $result.FinishedAt } else { $null })
            logPath = $(if ($result.PSObject.Properties.Name -contains 'LogPath') { $result.LogPath } else { $null })
        }
    }
    # Returned WITHOUT a leading comma on purpose. Callers wrap in @(), and a
    # comma here would hand them an array containing one array — which counted
    # forty-one carried shards as one and was caught on the device.
    return @($out)
}

<# A checkpoint's PASS records, restored as result objects a later aggregate
   can be judged over. Only ever called after a resume was accepted. #>
function ConvertFrom-InstrumentedCheckpointShardResults {
    param([Parameter(Mandatory = $true)]$Checkpoint)
    $out = @()
    foreach ($entry in @($Checkpoint.shardResults)) {
        if ($entry.status -ne 'PASS') { continue }
        $out += [pscustomobject]@{
            ShardIndex = [int]$entry.shardNumber
            ExpectedTestCount = [int]$entry.assignedCount
            ActualTestCount = [int]$entry.executedCount
            ExitCode = 0
            Status = 'PASS'
            Reason = 'carried from a checkpoint with the identical tested fingerprint'
            Pass = $true
            Classification = 'PASS'
            DurationSeconds = $(if ($entry.PSObject.Properties.Name -contains 'durationSeconds') { $entry.durationSeconds } else { 0 })
            StartedAt = $(if ($entry.PSObject.Properties.Name -contains 'startedAt') { $entry.startedAt } else { $null })
            FinishedAt = $(if ($entry.PSObject.Properties.Name -contains 'finishedAt') { $entry.finishedAt } else { $null })
            CarriedFromCheckpoint = $true
        }
    }
    return @($out)
}
