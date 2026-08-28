Set-StrictMode -Version 2.0

function ConvertFrom-InstrumentedDiscoveryOutput {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string[]]$Lines,
        [Parameter(Mandatory = $true)][int]$ExitCode
    )

    $text = $Lines -join "`n"
    if ($ExitCode -ne 0) {
        throw "Discovery command exited $ExitCode."
    }
    if ($text -match 'INSTRUMENTATION_ABORTED|INSTRUMENTATION_FAILED|Process crashed|shortMsg=|FAILURES!!!') {
        throw "Discovery contained an instrumentation failure or abort marker."
    }
    if ($text -notmatch '(?m)^INSTRUMENTATION_CODE:\s*-1\s*$') {
        throw "Discovery did not return the AndroidJUnitRunner completion code."
    }

    $tests = @()
    $status = @{}
    foreach ($line in $Lines) {
        if ($line -match '^INSTRUMENTATION_STATUS:\s*([^=]+)=(.*)$') {
            $status[$Matches[1].Trim()] = $Matches[2].Trim()
            continue
        }
        if ($line -match '^INSTRUMENTATION_STATUS_CODE:\s*(-?\d+)\s*$') {
            if ([int]$Matches[1] -eq 1) {
                foreach ($required in @('class', 'test', 'current', 'numtests')) {
                    if (-not $status.ContainsKey($required) -or [string]::IsNullOrWhiteSpace([string]$status[$required])) {
                        throw "Discovery start record is missing '$required'."
                    }
                }
                $tests += [pscustomobject]@{
                    ClassName  = [string]$status['class']
                    TestName   = [string]$status['test']
                    TestId     = "$($status['class'])#$($status['test'])"
                    Current    = [int]$status['current']
                    Total      = [int]$status['numtests']
                }
            }
            $status = @{}
        }
    }

    if ($tests.Count -eq 0) {
        throw "Discovery returned no test start records."
    }

    $advertisedTotals = @($tests | Select-Object -ExpandProperty Total -Unique)
    if ($advertisedTotals.Count -ne 1 -or $advertisedTotals[0] -ne $tests.Count) {
        throw "Discovery numtests does not equal the number of discovered test records."
    }

    $currents = @($tests | Select-Object -ExpandProperty Current | Sort-Object)
    for ($i = 0; $i -lt $tests.Count; $i++) {
        if ($currents[$i] -ne ($i + 1)) {
            throw "Discovery current sequence is incomplete or duplicated at position $($i + 1)."
        }
    }

    $duplicateIds = @($tests | Group-Object TestId | Where-Object Count -gt 1)
    if ($duplicateIds.Count -ne 0) {
        throw "Discovery returned duplicate test IDs: $($duplicateIds.Name -join ', ')."
    }

    $okMatches = [regex]::Matches($text, '(?m)^OK\s+\((\d+)\s+tests?\)\s*$')
    if ($okMatches.Count -ne 1 -or [int]$okMatches[0].Groups[1].Value -ne $tests.Count) {
        throw "Discovery did not contain exactly one OK result matching the discovered total."
    }

    $orderedTests = @($tests | Sort-Object ClassName, TestName)
    [pscustomobject]@{
        Classes = @($orderedTests | Select-Object -ExpandProperty ClassName -Unique)
        Tests = $orderedTests
        ClassCount = @($orderedTests | Select-Object -ExpandProperty ClassName -Unique).Count
        TestCount = $orderedTests.Count
    }
}

function New-InstrumentedShardPlan {
    param(
        [Parameter(Mandatory = $true)]$Discovery,
        [Parameter(Mandatory = $true)][int]$ShardCount
    )

    if ($ShardCount -lt 1) { throw "ShardCount must be at least 1." }
    if ($ShardCount -gt $Discovery.ClassCount) {
        throw "ShardCount $ShardCount exceeds discovered class count $($Discovery.ClassCount)."
    }

    $working = @()
    for ($i = 1; $i -le $ShardCount; $i++) {
        $working += [pscustomobject]@{
            Index = $i
            TestCount = 0
            Classes = [System.Collections.ArrayList]::new()
            TestIds = [System.Collections.ArrayList]::new()
        }
    }

    $classGroups = @($Discovery.Tests | Group-Object ClassName | Sort-Object `
        @{ Expression = { $_.Count }; Descending = $true }, `
        @{ Expression = { $_.Name }; Ascending = $true })

    foreach ($classGroup in $classGroups) {
        $target = $working | Sort-Object TestCount, Index | Select-Object -First 1
        [void]$target.Classes.Add($classGroup.Name)
        foreach ($test in @($classGroup.Group | Sort-Object TestName)) {
            [void]$target.TestIds.Add($test.TestId)
        }
        $target.TestCount += $classGroup.Count
    }

    $result = @()
    foreach ($shard in @($working | Sort-Object Index)) {
        $result += [pscustomobject]@{
            Index = $shard.Index
            Classes = @($shard.Classes | Sort-Object)
            ClassCount = $shard.Classes.Count
            TestIds = @($shard.TestIds | Sort-Object)
            TestCount = $shard.TestCount
        }
    }
    return $result
}

function Get-InstrumentedShardAssignmentProof {
    param(
        [Parameter(Mandatory = $true)]$Discovery,
        [Parameter(Mandatory = $true)][object[]]$Shards
    )

    $discoveredIds = @($Discovery.Tests | Select-Object -ExpandProperty TestId)
    $assignedIds = @($Shards | ForEach-Object { $_.TestIds })
    $missing = @($discoveredIds | Where-Object { $assignedIds -notcontains $_ })
    $duplicates = @($assignedIds | Group-Object | Where-Object Count -gt 1 | Select-Object -ExpandProperty Name)
    $unexpected = @($assignedIds | Where-Object { $discoveredIds -notcontains $_ } | Sort-Object -Unique)

    [pscustomobject]@{
        AssignedCount = $assignedIds.Count
        AssignedUnionCount = @($assignedIds | Sort-Object -Unique).Count
        Missing = $missing
        MissingCount = $missing.Count
        Duplicates = $duplicates
        DuplicateCount = $duplicates.Count
        Unexpected = $unexpected
        UnexpectedCount = $unexpected.Count
        Pass = ($missing.Count -eq 0 -and $duplicates.Count -eq 0 -and $unexpected.Count -eq 0)
    }
}

function Test-InstrumentedShardResult {
    param(
        [Parameter(Mandatory = $true)][int]$ShardIndex,
        [Parameter(Mandatory = $true)][int]$ExpectedTestCount,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string[]]$Lines,
        [Parameter(Mandatory = $true)][int]$ExitCode
    )

    $text = $Lines -join "`n"
    $okMatches = [regex]::Matches($text, '(?m)^OK\s+\((\d+)\s+tests?\)\s*$')
    $actualCount = if ($okMatches.Count -eq 1) { [int]$okMatches[0].Groups[1].Value } else { 0 }
    $status = 'PASS'
    $reason = 'valid runner result and exact expected count'

    if ($text -match 'INSTRUMENTATION_ABORTED|INSTRUMENTATION_FAILED|Process crashed|shortMsg=') {
        $status = 'INSTRUMENTATION_ABORT'
        $reason = 'instrumentation abort/failure marker present'
    } elseif ($text -match 'FAILURES!!!|(?m)^Tests run:.*Failures:') {
        $status = 'ASSERTION_FAILURE'
        $reason = 'test assertion failure marker present'
    } elseif ($ExitCode -ne 0) {
        $status = 'RUNNER_FAILURE'
        $reason = "adb/instrumentation command exited $ExitCode"
    } elseif ($okMatches.Count -ne 1) {
        $status = 'MISSING_RESULT'
        $reason = 'exactly one final OK result was not present'
    } elseif ($actualCount -ne $ExpectedTestCount) {
        $status = 'COUNT_MISMATCH'
        $reason = "runner reported $actualCount tests; expected $ExpectedTestCount"
    }

    [pscustomobject]@{
        ShardIndex = $ShardIndex
        ExpectedTestCount = $ExpectedTestCount
        ActualTestCount = $actualCount
        ExitCode = $ExitCode
        Status = $status
        Reason = $reason
        Pass = ($status -eq 'PASS')
    }
}

function New-FullShardedAggregate {
    param(
        [Parameter(Mandatory = $true)]$Discovery,
        [Parameter(Mandatory = $true)][object[]]$Shards,
        [Parameter(Mandatory = $true)][object[]]$Results
    )

    $proof = Get-InstrumentedShardAssignmentProof -Discovery $Discovery -Shards $Shards
    $passedIndexes = @($Results | Where-Object Pass | Select-Object -ExpandProperty ShardIndex)
    $executedIds = @($Shards | Where-Object { $passedIndexes -contains $_.Index } | ForEach-Object { $_.TestIds })
    $discoveredIds = @($Discovery.Tests | Select-Object -ExpandProperty TestId)
    $executionMissing = @($discoveredIds | Where-Object { $executedIds -notcontains $_ })
    $nonPass = @($Results | Where-Object { -not $_.Pass })
    $aborted = @($nonPass | Where-Object Status -eq 'INSTRUMENTATION_ABORT')
    $completeResultSet = ($Results.Count -eq $Shards.Count)
    $pass = $proof.Pass -and $completeResultSet -and $nonPass.Count -eq 0 -and `
        $executionMissing.Count -eq 0 -and @($executedIds | Sort-Object -Unique).Count -eq $Discovery.TestCount

    [pscustomobject]@{
        Pass = $pass
        Assignment = $proof
        ExecutedUnionCount = @($executedIds | Sort-Object -Unique).Count
        ExecutionMissing = $executionMissing
        ExecutionMissingCount = $executionMissing.Count
        FailedShards = @($nonPass | Select-Object -ExpandProperty ShardIndex)
        FailedShardCount = $nonPass.Count
        AbortedShards = @($aborted | Select-Object -ExpandProperty ShardIndex)
        AbortedShardCount = $aborted.Count
        CompleteResultSet = $completeResultSet
    }
}

function Get-FullSuiteMarker {
    param(
        [Parameter(Mandatory = $true)][bool]$IsFullSharded,
        [Parameter(Mandatory = $true)][bool]$AggregatePass
    )
    if (-not $IsFullSharded) { return $null }
    if ($AggregatePass) { return 'FULL_SHARDED_SUITE_PASS' }
    return 'FULL_SHARDED_SUITE_FAIL'
}
