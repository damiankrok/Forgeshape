param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('before', 'after')]
    [string]$Phase,

    [string]$Serial = 'emulator-5580'
)

$ErrorActionPreference = 'Stop'
$adb = 'C:\Users\damia\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$package = 'com.forgeshape.app'
$activity = "$package/.ForgeShapeActivity"
$artifactRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$phaseRoot = Join-Path $artifactRoot $Phase
$remoteXml = "/sdcard/uiarchr1-$Phase.xml"
$localXml = Join-Path $phaseRoot 'current.xml'
$rows = [System.Collections.Generic.List[object]]::new()

New-Item -ItemType Directory -Force -Path $phaseRoot | Out-Null

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

function Get-Node {
    param(
        [Parameter(Mandatory = $true)][xml]$Document,
        [Parameter(Mandatory = $true)][string]$Id
    )
    $fullId = "$package`:id/$Id"
    return $Document.SelectSingleNode("//*[@resource-id='$fullId']")
}

function Get-NodeCenter {
    param([Parameter(Mandatory = $true)][string]$Id)
    $document = Get-UiDocument
    $node = Get-Node -Document $document -Id $Id
    if ($null -eq $node -or $node.bounds -notmatch '\[(\d+),(\d+)\]\[(\d+),(\d+)\]') {
        throw "UI node has no usable bounds: $package`:id/$Id"
    }
    return @(
        [int](([int]$matches[1] + [int]$matches[3]) / 2),
        [int](([int]$matches[2] + [int]$matches[4]) / 2)
    )
}

function Tap-Id {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [int]$SettleMilliseconds = 700
    )
    $center = Get-NodeCenter -Id $Id
    Invoke-Adb shell input tap $center[0] $center[1] | Out-Null
    Start-Sleep -Milliseconds $SettleMilliseconds
}

function Confirm-Foreground {
    $activities = Invoke-Adb shell dumpsys activity activities
    if (($activities -join "`n") -notmatch 'com\.forgeshape\.app/.ForgeShapeActivity') {
        throw 'ForgeShape is not the foreground activity; evidence is invalid.'
    }
}

function Add-BoundsRow {
    param(
        [Parameter(Mandatory = $true)][string]$State,
        [Parameter(Mandatory = $true)][string]$Element,
        $Node
    )
    if ($null -eq $Node -or $Node.bounds -notmatch '\[(\d+),(\d+)\]\[(\d+),(\d+)\]') {
        $rows.Add([pscustomobject]@{
            Phase = $Phase; State = $State; Element = $Element; Present = $false
            Left = ''; Top = ''; Right = ''; Bottom = ''; Width = ''; Height = ''
            Enabled = ''; Selected = ''; Checked = ''
        })
        return
    }
    $left = [int]$matches[1]
    $top = [int]$matches[2]
    $right = [int]$matches[3]
    $bottom = [int]$matches[4]
    $rows.Add([pscustomobject]@{
        Phase = $Phase; State = $State; Element = $Element; Present = $true
        Left = $left; Top = $top; Right = $right; Bottom = $bottom
        Width = $right - $left; Height = $bottom - $top
        Enabled = $Node.enabled; Selected = $Node.selected; Checked = $Node.checked
    })
}

function Save-State {
    param([Parameter(Mandatory = $true)][string]$State)
    Confirm-Foreground
    $document = Get-UiDocument
    $ids = @(
        'workspace_trailing_host', 'tool_rail', 'precision_toggle',
        'transform_mode_group', 'transform_space_group',
        'transform_mode_move', 'transform_mode_rotate', 'transform_mode_scale',
        'transform_space_world', 'transform_space_local',
        'property_inspector', 'display_settings_popover',
        'objects_capsule', 'history_group', 'brush_edge_controls'
    )
    foreach ($id in $ids) {
        Add-BoundsRow -State $State -Element $id -Node (Get-Node -Document $document -Id $id)
    }

    $clusterNodes = @('tool_rail', 'precision_toggle', 'transform_mode_group',
            'transform_space_group') | ForEach-Object {
        Get-Node -Document $document -Id $_
    } | Where-Object { $null -ne $_ -and $_.bounds -match '\[(\d+),(\d+)\]\[(\d+),(\d+)\]' }
    if ($clusterNodes.Count -gt 0) {
        $rects = foreach ($node in $clusterNodes) {
            if ($node.bounds -match '\[(\d+),(\d+)\]\[(\d+),(\d+)\]') {
                [pscustomobject]@{ Left = [int]$matches[1]; Top = [int]$matches[2]
                    Right = [int]$matches[3]; Bottom = [int]$matches[4] }
            }
        }
        $left = ($rects.Left | Measure-Object -Minimum).Minimum
        $top = ($rects.Top | Measure-Object -Minimum).Minimum
        $right = ($rects.Right | Measure-Object -Maximum).Maximum
        $bottom = ($rects.Bottom | Measure-Object -Maximum).Maximum
        $rows.Add([pscustomobject]@{
            Phase = $Phase; State = $State; Element = 'right_cluster_union'; Present = $true
            Left = $left; Top = $top; Right = $right; Bottom = $bottom
            Width = $right - $left; Height = $bottom - $top
            Enabled = ''; Selected = ''; Checked = ''
        })
    }

    $remotePng = "/sdcard/uiarchr1-$Phase-$State.png"
    $localPng = Join-Path $phaseRoot "$State.png"
    Invoke-Adb @('shell', 'screencap', '-p', $remotePng) | Out-Null
    Invoke-Adb pull $remotePng $localPng | Out-Null
    Write-Host "Captured $Phase/$State"
}

function Start-FreshConstruction {
    Invoke-Adb shell am force-stop $package | Out-Null
    Invoke-Adb shell pm clear $package | Out-Null
    Invoke-Adb shell am start -n $activity | Out-Null
    Start-Sleep -Seconds 3
    Tap-Id -Id 'start_option_construction' -SettleMilliseconds 1400
}

$avdName = (Invoke-Adb emu avd name | Select-Object -First 1).Trim()
if ($avdName -ne 'ForgeShape_Stage006') {
    throw "Unexpected AVD '$avdName'; refusing to capture evidence."
}

Invoke-Adb shell wm size reset | Out-Null
Invoke-Adb shell wm density reset | Out-Null
Invoke-Adb shell settings put system font_scale 1.0 | Out-Null
Invoke-Adb shell settings put secure show_ime_with_hard_keyboard 1 | Out-Null

Start-FreshConstruction
Save-State '01-construction-resting'
Tap-Id 'tool_rail_place'
Tap-Id 'transform_mode_move'
Save-State '02-move'
Tap-Id 'transform_mode_rotate'
Save-State '03-rotate'
Tap-Id 'transform_mode_scale'
Save-State '04-scale'
Tap-Id 'precision_toggle'
Save-State '05-exact-transform'
Tap-Id -Id 'field_pos_x' -SettleMilliseconds 1400
Save-State '06-exact-transform-ime'
Invoke-Adb shell input keyevent 4 | Out-Null
Start-Sleep -Milliseconds 700
Tap-Id 'display_settings_button'
Save-State '07-display'

Start-FreshConstruction
Invoke-Adb shell wm size 2400x1080 | Out-Null
Invoke-Adb shell wm density 420 | Out-Null
Start-Sleep -Seconds 2
Save-State '08-short-landscape'

Invoke-Adb shell wm size 1600x2560 | Out-Null
Invoke-Adb shell wm density 240 | Out-Null
Start-Sleep -Seconds 2
Save-State '09-expanded-tablet'

Invoke-Adb shell wm size reset | Out-Null
Invoke-Adb shell wm density reset | Out-Null
Start-Sleep -Seconds 2
Start-FreshConstruction
Tap-Id -Id 'freeze_to_sculpt' -SettleMilliseconds 5000
Save-State '10-sculpt-resting'
Tap-Id 'precision_toggle'
Save-State '11-sculpt-details'

$rows | Export-Csv -LiteralPath (Join-Path $phaseRoot 'bounds.csv') -NoTypeInformation
if (Test-Path -LiteralPath $localXml) {
    Remove-Item -LiteralPath $localXml -Force
}

Invoke-Adb shell wm size reset | Out-Null
Invoke-Adb shell wm density reset | Out-Null
Invoke-Adb shell settings put system font_scale 1.0 | Out-Null
Write-Host "UI-ARCH-R1 $Phase evidence capture complete."
