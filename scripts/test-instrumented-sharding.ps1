param()

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'instrumented-sharding.ps1')

$results = @()

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Invoke-THR1Check {
    param([string]$Id, [string]$Name, [scriptblock]$Body)
    try {
        & $Body
        $script:results += [pscustomobject]@{ Id = $Id; Name = $Name; Pass = $true; Evidence = 'assertions passed' }
    } catch {
        $script:results += [pscustomobject]@{ Id = $Id; Name = $Name; Pass = $false; Evidence = $_.Exception.Message }
    }
}

function New-DiscoveryFixture {
    $definitions = [ordered]@{
        'com.forgeshape.AlphaTest' = @('a1', 'a2', 'a3')
        'com.forgeshape.BetaTest' = @('b1', 'b2')
        'com.forgeshape.GammaTest' = @('g1')
    }
    $total = 6
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

function New-PassingResults {
    param([object[]]$Shards)
    $out = @()
    foreach ($shard in $Shards) {
        $out += Test-InstrumentedShardResult -ShardIndex $shard.Index -ExpectedTestCount $shard.TestCount `
            -Lines @("OK ($($shard.TestCount) tests)") -ExitCode 0
    }
    return $out
}

$fixtureLines = @(New-DiscoveryFixture)
$discovery = ConvertFrom-InstrumentedDiscoveryOutput -Lines $fixtureLines -ExitCode 0
$plan2 = @(New-InstrumentedShardPlan -Discovery $discovery -ShardCount 2)

Invoke-THR1Check 'THR1-01' 'discovery finds complete inventory' {
    Assert-True ($discovery.ClassCount -eq 3 -and $discovery.TestCount -eq 6) 'complete fixture inventory was not discovered'
    $incomplete = @($fixtureLines | Where-Object { $_ -ne 'INSTRUMENTATION_STATUS: current=6' })
    $rejected = $false
    try { ConvertFrom-InstrumentedDiscoveryOutput -Lines $incomplete -ExitCode 0 | Out-Null } catch { $rejected = $true }
    Assert-True $rejected 'incomplete current sequence was accepted'
}

Invoke-THR1Check 'THR1-02' 'partition is deterministic' {
    $again = @(New-InstrumentedShardPlan -Discovery $discovery -ShardCount 2)
    $left = $plan2 | ConvertTo-Json -Depth 6 -Compress
    $right = $again | ConvertTo-Json -Depth 6 -Compress
    Assert-True ($left -ceq $right) 'identical inventory produced a different plan'
}

Invoke-THR1Check 'THR1-03' 'zero duplicate assignment' {
    $proof = Get-InstrumentedShardAssignmentProof -Discovery $discovery -Shards $plan2
    Assert-True ($proof.DuplicateCount -eq 0) 'duplicate assignment found'
    $duplicated = @($plan2 | ForEach-Object {
        [pscustomobject]@{ Index = $_.Index; Classes = @($_.Classes); ClassCount = $_.ClassCount; TestIds = @($_.TestIds); TestCount = $_.TestCount }
    })
    $duplicated[1].TestIds += $duplicated[0].TestIds[0]
    $rejected = Get-InstrumentedShardAssignmentProof -Discovery $discovery -Shards $duplicated
    Assert-True (-not $rejected.Pass -and $rejected.DuplicateCount -eq 1) 'injected duplicate was not detected'
}

Invoke-THR1Check 'THR1-04' 'zero missing assignment' {
    $proof = Get-InstrumentedShardAssignmentProof -Discovery $discovery -Shards $plan2
    Assert-True ($proof.MissingCount -eq 0) 'missing assignment found'
    $incomplete = @($plan2 | ForEach-Object {
        [pscustomobject]@{ Index = $_.Index; Classes = @($_.Classes); ClassCount = $_.ClassCount; TestIds = @($_.TestIds); TestCount = $_.TestCount }
    })
    $incomplete[0].TestIds = @($incomplete[0].TestIds | Select-Object -Skip 1)
    $rejected = Get-InstrumentedShardAssignmentProof -Discovery $discovery -Shards $incomplete
    Assert-True (-not $rejected.Pass -and $rejected.MissingCount -eq 1) 'injected missing assignment was not detected'
}

Invoke-THR1Check 'THR1-05' 'changing shard count preserves union' {
    $plan3 = @(New-InstrumentedShardPlan -Discovery $discovery -ShardCount 3)
    $union2 = @($plan2 | ForEach-Object TestIds | Sort-Object -Unique)
    $union3 = @($plan3 | ForEach-Object TestIds | Sort-Object -Unique)
    Assert-True (($union2 -join '|') -ceq ($union3 -join '|')) 'union changed with shard count'
}

Invoke-THR1Check 'THR1-06' 'shard failure makes aggregate fail' {
    $injected = @(New-PassingResults -Shards $plan2)
    $injected[0] = Test-InstrumentedShardResult -ShardIndex $plan2[0].Index -ExpectedTestCount $plan2[0].TestCount `
        -Lines @('FAILURES!!!', 'Tests run: 3,  Failures: 1') -ExitCode 0
    $aggregate = New-FullShardedAggregate -Discovery $discovery -Shards $plan2 -Results $injected
    Assert-True (-not $aggregate.Pass -and $injected[0].Status -eq 'ASSERTION_FAILURE') 'assertion failure did not invalidate aggregate'
}

Invoke-THR1Check 'THR1-07' 'instrumentation abort makes aggregate fail' {
    $injected = @(New-PassingResults -Shards $plan2)
    $injected[0] = Test-InstrumentedShardResult -ShardIndex $plan2[0].Index -ExpectedTestCount $plan2[0].TestCount `
        -Lines @('INSTRUMENTATION_RESULT: shortMsg=Process crashed.', 'INSTRUMENTATION_ABORTED: System has crashed.') -ExitCode 0
    $aggregate = New-FullShardedAggregate -Discovery $discovery -Shards $plan2 -Results $injected
    Assert-True (-not $aggregate.Pass -and $aggregate.AbortedShardCount -eq 1) 'abort did not invalidate aggregate'
}

Invoke-THR1Check 'THR1-08' 'missing result makes aggregate fail' {
    $injected = @(New-PassingResults -Shards $plan2)
    $injected[0] = Test-InstrumentedShardResult -ShardIndex $plan2[0].Index -ExpectedTestCount $plan2[0].TestCount `
        -Lines @('INSTRUMENTATION_CODE: -1') -ExitCode 0
    $aggregate = New-FullShardedAggregate -Discovery $discovery -Shards $plan2 -Results $injected
    Assert-True (-not $aggregate.Pass -and $injected[0].Status -eq 'MISSING_RESULT') 'missing final result did not invalidate aggregate'
}

Invoke-THR1Check 'THR1-09' 'focused mode cannot emit full-suite PASS' {
    $focusedMarker = Get-FullSuiteMarker -IsFullSharded $false -AggregatePass $true
    Assert-True ([string]::IsNullOrEmpty($focusedMarker)) 'focused mode produced a full-suite marker'
    $engine = (Get-Process -Id $PID).Path
    $runner = Join-Path $PSScriptRoot 'run-instrumented-tests.ps1'
    $focusedOutput = @(& $engine -NoProfile -File $runner -Serial emulator-5554 `
        -TestClass com.forgeshape.FocusedTest 2>&1 | ForEach-Object { "$_" })
    $focusedExit = $LASTEXITCODE
    Assert-True ($focusedExit -ne 0) 'reserved-serial focused fixture did not stop before adb'
    Assert-True (($focusedOutput -join "`n") -notmatch 'FULL_SHARDED_SUITE_PASS') `
        'actual focused runner invocation emitted full-suite PASS'
}

Invoke-THR1Check 'THR1-10' 'aggregate totals equal discovered totals' {
    $passing = @(New-PassingResults -Shards $plan2)
    $aggregate = New-FullShardedAggregate -Discovery $discovery -Shards $plan2 -Results $passing
    $reported = ($passing | Measure-Object ActualTestCount -Sum).Sum
    Assert-True ($aggregate.Pass -and $reported -eq $discovery.TestCount -and $aggregate.ExecutedUnionCount -eq $discovery.TestCount) `
        'aggregate totals did not equal discovery'
}

$results | ForEach-Object {
    $status = if ($_.Pass) { 'PASS' } else { 'FAIL' }
    Write-Output "$($_.Id) | $status | $($_.Name) | $($_.Evidence)"
}
$failed = @($results | Where-Object { -not $_.Pass })
if ($failed.Count -ne 0) {
    Write-Error -Message "$($failed.Count) THR1 check(s) failed." -ErrorAction Continue
    exit 1
}
Write-Output 'THR1-01..10 PASS.'
exit 0
