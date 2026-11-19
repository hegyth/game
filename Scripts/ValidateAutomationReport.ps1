[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
    throw "Automation report was not created: $ReportPath"
}

try {
    $report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json -ErrorAction Stop
}
catch {
    throw "Automation report is not valid JSON: $ReportPath"
}

$counts = @{}
foreach ($propertyName in @('succeeded', 'failed', 'notRun')) {
    $property = $report.PSObject.Properties[$propertyName]
    if ($null -eq $property) {
        throw "Automation report is missing required property '$propertyName': $ReportPath"
    }

    try {
        $value = [Convert]::ToInt32($property.Value)
    }
    catch {
        throw "Automation report property '$propertyName' is not an integer: $ReportPath"
    }

    if ($value -lt 0) {
        throw "Automation report property '$propertyName' cannot be negative: $ReportPath"
    }

    $counts[$propertyName] = $value
}

$executedCount = $counts.succeeded + $counts.failed + $counts.notRun
$reportedTests = @($report.tests).Count

if ($executedCount -eq 0 -or $reportedTests -eq 0) {
    throw "Automation run completed without executing any tests: $ReportPath"
}

if ($counts.failed -gt 0 -or $counts.notRun -gt 0) {
    throw ("Automation run was not successful: {0} succeeded, {1} failed, {2} not run. Report: {3}" -f `
        $counts.succeeded, $counts.failed, $counts.notRun, $ReportPath)
}

[pscustomobject]@{
    Succeeded = $counts.succeeded
    Failed = $counts.failed
    NotRun = $counts.notRun
    ReportPath = $ReportPath
}
