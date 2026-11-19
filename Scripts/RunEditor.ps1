[CmdletBinding()]
param(
    [string]$EngineVersion = '5.7',
    [switch]$Wait,
    [string[]]$AdditionalArguments = @()
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $repositoryRoot 'VELRATH.uproject'
$engineRoot = & (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1') -Version $EngineVersion
$editor = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$arguments = @($projectFile) + $AdditionalArguments

if ($Wait) {
    & $editor @arguments
    exit $LASTEXITCODE
}

$process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $repositoryRoot -PassThru
Write-Output "Unreal Editor started with PID $($process.Id)."

