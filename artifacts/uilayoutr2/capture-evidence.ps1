param(
    [string]$Serial = "emulator-5580"
)

$ErrorActionPreference = "Stop"
$adb = "C:\Users\damia\AppData\Local\Android\Sdk\platform-tools\adb.exe"
$package = "com.forgeshape.app"
$activity = "$package/.ForgeShapeActivity"
$artifactRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$remoteXml = "/sdcard/uilayoutr2-current.xml"
$localXml = Join-Path $artifactRoot "current.xml"
$remoteVideo = "/sdcard/uilayoutr2-walkthrough.mp4"

function Invoke-Adb {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    & $adb -s $Serial @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed: $($Arguments -join ' ')"
    }
}

function Get-UiDocument {
    Invoke-Adb shell uiautomator dump $remoteXml | Out-Null
    Invoke-Adb pull $remoteXml $localXml | Out-Null
    return [xml](Get-Content -LiteralPath $localXml -Raw)
}

function Get-NodeCenter {
    param([Parameter(Mandatory = $true)][string]$Id)
    $document = Get-UiDocument
    $fullId = "$package`:id/$Id"
    $node = $document.SelectSingleNode("//*[@resource-id='$fullId']")
    if ($null -eq $node) {
        throw "UI node not found: $fullId"
    }
    if ($node.bounds -notmatch '\[(\d+),(\d+)\]\[(\d+),(\d+)\]') {
        throw "UI node has no usable bounds: $fullId"
    }
    return @(
        [int](([int]$matches[1] + [int]$matches[3]) / 2),
        [int](([int]$matches[2] + [int]$matches[4]) / 2)
    )
}

function Tap-Id {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [int]$SettleMilliseconds = 800
    )
    $center = Get-NodeCenter -Id $Id
    Invoke-Adb shell input tap $center[0] $center[1] | Out-Null
    Start-Sleep -Milliseconds $SettleMilliseconds
}

function Save-Screenshot {
    param([Parameter(Mandatory = $true)][string]$Name)
    $remote = "/sdcard/$Name"
    $local = Join-Path $artifactRoot $Name
    Invoke-Adb @("shell", "screencap", "-p", $remote) | Out-Null
    Invoke-Adb pull $remote $local | Out-Null
    Write-Host "Captured $Name"
}

function Start-FreshConstruction {
    Invoke-Adb shell am force-stop $package | Out-Null
    Invoke-Adb shell pm clear $package | Out-Null
    Invoke-Adb shell am start -n $activity | Out-Null
    Start-Sleep -Seconds 3
    Tap-Id -Id "start_option_construction" -SettleMilliseconds 1800
}

function Start-WalkthroughRecording {
    Invoke-Adb @("shell", "rm", "-f", $remoteVideo) | Out-Null
    return Start-Process -FilePath $adb `
        -ArgumentList @("-s", $Serial, "shell", "screenrecord", "--size", "720x1600", "--bit-rate", "6000000", "--time-limit", "150", $remoteVideo) `
        -WindowStyle Hidden -PassThru
}

function Stop-WalkthroughRecording {
    param([Parameter(Mandatory = $true)]$Recorder)
    Invoke-Adb @("shell", "pkill", "-INT", "screenrecord") | Out-Null
    $Recorder.WaitForExit(10000) | Out-Null
    Start-Sleep -Seconds 2
    Invoke-Adb pull $remoteVideo (Join-Path $artifactRoot "uilayoutr2-walkthrough.mp4") | Out-Null
    Write-Host "Captured uilayoutr2-walkthrough.mp4"
}

$avdName = (Invoke-Adb emu avd name | Select-Object -First 1).Trim()
if ($avdName -ne "ForgeShape_Stage006") {
    throw "Unexpected AVD '$avdName'; refusing to capture evidence."
}

Invoke-Adb shell wm size reset | Out-Null
Invoke-Adb shell wm density reset | Out-Null
Invoke-Adb shell settings put system font_scale 1.0 | Out-Null
Invoke-Adb shell settings put secure show_ime_with_hard_keyboard 1 | Out-Null

Start-FreshConstruction
$recorder = Start-WalkthroughRecording
Start-Sleep -Seconds 2

Save-Screenshot "01-construction-resting.png"
Tap-Id "tool_rail_shape"
Save-Screenshot "02-shape-active.png"
Tap-Id "tool_rail_place"
Save-Screenshot "03-transform-active.png"
Tap-Id "transform_mode_move"
Save-Screenshot "04-move.png"
Tap-Id "transform_mode_rotate"
Save-Screenshot "05-rotate.png"
Tap-Id "transform_mode_scale"
Save-Screenshot "06-scale.png"
Tap-Id "transform_mode_rotate"
Tap-Id "transform_space_world"
Save-Screenshot "07-world.png"
Tap-Id "transform_space_local"
Save-Screenshot "08-local.png"
Tap-Id "precision_toggle"
Save-Screenshot "09-exact-transform-open.png"
Tap-Id -Id "field_pos_x" -SettleMilliseconds 1800
Save-Screenshot "10-exact-transform-ime.png"
Invoke-Adb shell input keyevent 4 | Out-Null
Start-Sleep -Milliseconds 900
Tap-Id "precision_toggle"
Save-Screenshot "11-exact-closed-restored.png"
Tap-Id "objects_capsule_add"
Tap-Id "objects_capsule_active"
Tap-Id -Id "freeze_to_sculpt" -SettleMilliseconds 6000
Tap-Id "tool_rail_grab"
Tap-Id "tool_rail_clay"
Tap-Id "tool_rail_smooth"
Tap-Id "tool_rail_inflate"
Start-Sleep -Seconds 6
Save-Screenshot "14-sculpt-resting.png"
Tap-Id "precision_toggle"
Save-Screenshot "15-sculpt-details-open.png"
Tap-Id "precision_toggle"
Save-Screenshot "16-sculpt-details-closed-restored.png"

Stop-WalkthroughRecording -Recorder $recorder

Start-FreshConstruction
Invoke-Adb shell wm size 2400x1080 | Out-Null
Invoke-Adb shell wm density 420 | Out-Null
Start-Sleep -Seconds 3
Tap-Id "tool_rail_place"
Save-Screenshot "12-short-landscape.png"

Invoke-Adb shell wm size 1600x2560 | Out-Null
Invoke-Adb shell wm density 240 | Out-Null
Start-Sleep -Seconds 3
Save-Screenshot "13-expanded-tablet.png"

Invoke-Adb shell wm size reset | Out-Null
Invoke-Adb shell wm density reset | Out-Null
Invoke-Adb shell settings put system font_scale 1.0 | Out-Null
Start-Sleep -Seconds 2

if (Test-Path -LiteralPath $localXml) {
    Remove-Item -LiteralPath $localXml -Force
}

Write-Host "UI-LAYOUT-R2 visual evidence capture complete."
