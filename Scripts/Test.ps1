[CmdletBinding()]
param(
    [string]$TestFilter = 'VELRATH',
    [string]$EngineVersion = '5.7'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $repositoryRoot 'VELRATH.uproject'
$engineRoot = & (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1') -Version $EngineVersion
$editor = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$automationRoot = Join-Path $repositoryRoot 'Saved\Automation'
$runId = '{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'), [Guid]::NewGuid().ToString('N')
$reportPath = Join-Path $automationRoot $runId
$reportFile = Join-Path $reportPath 'index.json'

$arguments = @(
    $projectFile,
    '-unattended',
    '-NullRHI',
    '-NoSplash',
    '-NoSound',
    '-NoP4',
    '-stdout',
    '-FullStdOutLogOutput',
    "-ExecCmds=Automation RunTests $TestFilter",
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$reportPath"
)

& $editor @arguments
$editorExitCode = $LASTEXITCODE
if ($editorExitCode -ne 0) {
    throw "Automation tests failed with Unreal Editor exit code $editorExitCode."
}

$summary = & (Join-Path $PSScriptRoot 'ValidateAutomationReport.ps1') -ReportPath $reportFile
Write-Output ("Automation tests passed: {0} succeeded ({1} with warnings), {2} failed, {3} not run. Report: {4}" -f `
    $summary.Succeeded, $summary.SucceededWithWarnings, $summary.Failed, $summary.NotRun, $reportFile)

