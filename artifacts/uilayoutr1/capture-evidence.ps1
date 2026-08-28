param(
    [string]$Serial = "emulator-5580"
)

$ErrorActionPreference = "Stop"
$adb = "C:\Users\damia\AppData\Local\Android\Sdk\platform-tools\adb.exe"
$package = "com.forgeshape.app"
$activity = "$package/.ForgeShapeActivity"
$artifactRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$remoteXml = "/sdcard/uilayoutr1-current.xml"
$localXml = Join-Path $artifactRoot "current.xml"
$remoteVideo = "/sdcard/uilayoutr1-walkthrough.mp4"

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
        [int]$SettleMilliseconds = 900
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
    Invoke-Adb pull $remoteVideo (Join-Path $artifactRoot "uilayoutr1-walkthrough.mp4") | Out-Null
    Write-Host "Captured uilayoutr1-walkthrough.mp4"
}

$avdName = (Invoke-Adb emu avd name | Select-Object -First 1).Trim()
if ($avdName -ne "ForgeShape_UILAYOUTR1") {
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
Tap-Id "tool_rail_place"
Tap-Id "transform_mode_move"
Save-Screenshot "02-construction-move.png"
Tap-Id "transform_mode_rotate"
Save-Screenshot "03-construction-rotate.png"
Tap-Id "transform_mode_scale"
Save-Screenshot "04-construction-scale.png"
Tap-Id "precision_toggle"
Save-Screenshot "05-exact-transform.png"
Tap-Id -Id "field_pos_x" -SettleMilliseconds 1800
Save-Screenshot "06-exact-transform-ime.png"
Invoke-Adb shell input keyevent 4 | Out-Null
Start-Sleep -Milliseconds 900
Tap-Id "objects_capsule_add"
Save-Screenshot "07-add-primitive.png"
Tap-Id "objects_capsule_active"
Save-Screenshot "08-objects.png"
Tap-Id "display_settings_button"
Save-Screenshot "09-display.png"
Tap-Id -Id "freeze_to_sculpt" -SettleMilliseconds 6000
Save-Screenshot "12-sculpt-resting.png"
Tap-Id "precision_toggle"
Save-Screenshot "13-sculpt-details.png"
Tap-Id "objects_capsule_active"
Save-Screenshot "14-sculpt-objects.png"

Stop-WalkthroughRecording -Recorder $recorder

Start-FreshConstruction
Invoke-Adb shell wm size 2400x1080 | Out-Null
Invoke-Adb shell wm density 420 | Out-Null
Start-Sleep -Seconds 3
Save-Screenshot "10-short-landscape.png"

Invoke-Adb shell wm size 1600x2560 | Out-Null
Invoke-Adb shell wm density 240 | Out-Null
Start-Sleep -Seconds 3
Save-Screenshot "11-expanded-tablet.png"

Invoke-Adb shell wm size reset | Out-Null
Invoke-Adb shell wm density reset | Out-Null
Invoke-Adb shell settings put system font_scale 1.0 | Out-Null
Start-Sleep -Seconds 2

if (Test-Path -LiteralPath $localXml) {
    Remove-Item -LiteralPath $localXml -Force
}

Write-Host "UI-LAYOUT-R1 visual evidence capture complete."
