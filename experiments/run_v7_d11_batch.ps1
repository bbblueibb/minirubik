param(
    [int]$MaxNew = 10
)

$ErrorActionPreference = "Stop"

$Base = $PSScriptRoot
$RipesExe  = Join-Path $Base "Ripes.exe"
$Template  = Join-Path $Base "solver_rv32i_v7_template.elf"
$Working   = Join-Path $Base "solver_rv32i_v7_batch.elf"
$StatesFile = Join-Path $Base "d11_states.txt"

$Csv      = Join-Path $Base "v7_d11_all_results.csv"
$Summary  = Join-Path $Base "v7_d11_all_summary.txt"

$StdoutFile = Join-Path $Base "v7_d11_batch_stdout.txt"
$StderrFile = Join-Path $Base "v7_d11_batch_stderr.txt"
$ReportFile = Join-Path $Base "v7_d11_batch_report.txt"

# ------------------------------------------------------------
# Load all exact-distance-11 states.
# ------------------------------------------------------------
$states = @(
    Get-Content $StatesFile |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -match '^[1-7]{7}[1-3]{7}$' }
)

if ($states.Count -ne 2644) {
    throw "Expected 2644 D11 states, found $($states.Count)"
}

# ------------------------------------------------------------
# Load template ELF and locate the 14-byte placeholder exactly.
# ------------------------------------------------------------
$templateBytes = [System.IO.File]::ReadAllBytes($Template)

$placeholderText = "12345671111111"
$needle = [System.Text.Encoding]::ASCII.GetBytes($placeholderText)

$matches = New-Object System.Collections.Generic.List[int]

for ($i = 0; $i -le $templateBytes.Length - $needle.Length; $i++) {
    $ok = $true

    for ($j = 0; $j -lt $needle.Length; $j++) {
        if ($templateBytes[$i + $j] -ne $needle[$j]) {
            $ok = $false
            break
        }
    }

    if ($ok) {
        $matches.Add($i)
    }
}

if ($matches.Count -ne 1) {
    throw "Expected exactly one INPUT_STATE placeholder in template ELF; found $($matches.Count)"
}

$inputOffset = $matches[0]

Write-Host "INPUT_STATE file offset: $inputOffset"
Write-Host "D11 states: $($states.Count)"

# ------------------------------------------------------------
# Resume support.
# Existing CSV entries are skipped.
# ------------------------------------------------------------
$done = @{}

if (Test-Path $Csv) {
    foreach ($row in (Import-Csv $Csv)) {
        $done[$row.State] = $true
    }
}

$newCount = 0
$ordinal = 0

foreach ($state in $states) {
    $ordinal++

    if ($done.ContainsKey($state)) {
        continue
    }

    if ($MaxNew -gt 0 -and $newCount -ge $MaxNew) {
        break
    }

    # Patch only the 14 input bytes; ELF layout remains unchanged.
    $bytes = [byte[]]$templateBytes.Clone()
    $stateBytes = [System.Text.Encoding]::ASCII.GetBytes($state)

    [Array]::Copy(
        $stateBytes, 0,
        $bytes, $inputOffset,
        14
    )

    [System.IO.File]::WriteAllBytes($Working, $bytes)

    $ripesArgs = @(
        "--mode", "cli",
        "--src", $Working,
        "-t", "elf",
        "--proc", "RV32_ISS",
        "--iret",
        "--runinfo",
        "--timeout", "30000",
        "--output", $ReportFile
    )

    $p = Start-Process `
        -FilePath $RipesExe `
        -ArgumentList $ripesArgs `
        -RedirectStandardOutput $StdoutFile `
        -RedirectStandardError $StderrFile `
        -Wait `
        -PassThru

    $stdoutText = ""
    $reportText = ""

    if (Test-Path $StdoutFile) {
        $stdoutText = Get-Content $StdoutFile -Raw
    }

    if (Test-Path $ReportFile) {
        $reportText = Get-Content $ReportFile -Raw
    }

    $programExit = -999

    $mExit = [regex]::Match(
        $stdoutText,
        'Program exited with code:\s*(-?\d+)'
    )

    if ($mExit.Success) {
        $programExit = [int]$mExit.Groups[1].Value
    }

    $mIret = [regex]::Match(
        $reportText,
        'instructions retired\s+(\d+)'
    )

    if (-not $mIret.Success) {
        throw "Could not read instruction count for state $state"
    }

    $iret = [int64]$mIret.Groups[1].Value

    $result = [PSCustomObject]@{
        State        = $state
        Instructions = $iret
        ProgramExit  = $programExit
        RipesExit    = $p.ExitCode
    }

    if (Test-Path $Csv) {
        $result | Export-Csv `
            -Path $Csv `
            -NoTypeInformation `
            -Append
    }
    else {
        $result | Export-Csv `
            -Path $Csv `
            -NoTypeInformation
    }

    $done[$state] = $true
    $newCount++

    Write-Host "[$ordinal/2644] $state : $iret"

    if ($p.ExitCode -ne 0 -or $programExit -ne 0) {
        throw "Correctness failure at state $state"
    }

    if ($iret -ge 50000000) {
        throw "50M gate failed at state $state : $iret"
    }
}

# ------------------------------------------------------------
# Summarize everything completed so far.
# ------------------------------------------------------------
$rows = @(Import-Csv $Csv)

$maxRow = $rows |
    Sort-Object { [int64]$_.Instructions } -Descending |
    Select-Object -First 1

$gate = "PASS so far"

if ([int64]$maxRow.Instructions -ge 50000000) {
    $gate = "FAIL"
}
elseif ($rows.Count -eq 2644) {
    $gate = "PASS"
}

$summaryLines = @(
    "states completed: $($rows.Count) / 2644",
    "maximum instructions: $($maxRow.Instructions)",
    "worst state: $($maxRow.State)",
    "50M gate: $gate"
)

$summaryLines | Set-Content -Encoding ASCII $Summary

Write-Host ""
Write-Host "===== SUMMARY ====="
$summaryLines | ForEach-Object { Write-Host $_ }
