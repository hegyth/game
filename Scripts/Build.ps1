[CmdletBinding()]
param(
    [ValidateSet('Editor', 'Game', 'Server')]
    [string]$Target = 'Editor',

    [ValidateSet('Debug', 'DebugGame', 'Development', 'Shipping', 'Test')]
    [string]$Configuration = 'Development',

    [string]$EngineVersion = '5.7'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $repositoryRoot 'VELRATH.uproject'
$engineRoot = & (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1') -Version $EngineVersion
$buildScript = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'

$targetName = switch ($Target) {
    'Editor' { 'VELRATHEditor' }
    'Game' { 'VELRATH' }
    'Server' { 'VELRATHServer' }
}

& $buildScript $targetName Win64 $Configuration "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) {
    throw "Unreal build failed with exit code $LASTEXITCODE."
}

