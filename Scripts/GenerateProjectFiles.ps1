[CmdletBinding()]
param(
    [string]$EngineVersion = '5.7'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $repositoryRoot 'VELRATH.uproject'
$engineRoot = & (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1') -Version $EngineVersion
$dotnet = Join-Path $engineRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.412\win-x64\dotnet.exe'
$unrealBuildTool = Join-Path $engineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'

if (-not (Test-Path -LiteralPath $dotnet)) {
    throw "Bundled dotnet was not found: $dotnet"
}

& $dotnet $unrealBuildTool -ProjectFiles "-Project=$projectFile" -Game -Engine -Progress
if ($LASTEXITCODE -ne 0) {
    throw "Project file generation failed with exit code $LASTEXITCODE."
}

