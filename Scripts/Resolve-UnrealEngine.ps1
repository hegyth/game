[CmdletBinding()]
param(
    [string]$Version = '5.7'
)

$registryPaths = @(
    "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Version",
    "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
)

$machineInstall = Get-ItemPropertyValue -Path $registryPaths[0] -Name InstalledDirectory -ErrorAction SilentlyContinue
if ($machineInstall -and (Test-Path -LiteralPath $machineInstall)) {
    return (Resolve-Path -LiteralPath $machineInstall).Path
}

$fallback = "C:\Program Files\Epic Games\UE_$Version"
if (Test-Path -LiteralPath $fallback) {
    return (Resolve-Path -LiteralPath $fallback).Path
}

throw "Unreal Engine $Version was not found. Install it with Epic Games Launcher or update the version used by the scripts."

