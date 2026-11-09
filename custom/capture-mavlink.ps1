$ErrorActionPreference = 'Stop'
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an administrator PowerShell window.'
}

$sourceDirectory = Split-Path -Parent $PSScriptRoot
$captureDirectory = Join-Path $sourceDirectory ('build-herelink-debug\captures\' + (Get-Date -Format 'yyyy-MM-dd_HH-mm-ss-fff'))
New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null
$etlFile = Join-Path $captureDirectory 'herelink.etl'
$pcapFile = Join-Path $captureDirectory 'herelink.pcapng'
$filterName = 'HerelinkGcs_' + [guid]::NewGuid().ToString('N').Substring(0, 8)

# Do not stop another capture or remove another client's filters.
$status = (& pktmon status 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw $status }
if ($status -notmatch '(?i)(not running|Status\s*:\s*Stopped|Packet Monitor is stopped)') {
    Write-Host $status
    throw 'Cannot confirm that PktMon is stopped. An existing capture will not be modified.'
}
$filters = (& pktmon filter list 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw $filters }
$filterSummary = ($filters -replace '\s+', ' ').Trim()
$emptyFilterList = $filterSummary -match '(?i)^Packet Filters:\s*None\.?$' `
    -or $filterSummary -match '(?i)^No\s+(packet\s+)?filters(\s+found)?\.?$'
if (-not $emptyFilterList) {
    Write-Host $filters
    throw 'Cannot confirm an empty PktMon filter list. Existing filters were not changed.'
}

$filterAdded = $false
$captureStarted = $false
try {
    & pktmon filter add $filterName -i 192.168.144.11 -t UDP -p 14552
    if ($LASTEXITCODE -ne 0) { throw 'Could not add the Herelink capture filter.' }
    $filterAdded = $true
    # Trace the filtered traffic through the Windows stack as well as the NIC.
    & pktmon start --capture --comp all --pkt-size 0 --file-size 128 --log-mode circular --file-name $etlFile
    if ($LASTEXITCODE -ne 0) { throw 'Could not start capture. No existing capture will be stopped.' }
    $captureStarted = $true
    Write-Host 'Capturing only UDP port 14552 to/from 192.168.144.11 (128 MB circular buffer).'
    Write-Host 'Keep this window open. Use Herelink GCS normally in safe conditions.'
    Read-Host 'After the error, note the time, return here and press ENTER to save' | Out-Null
} finally {
    if ($captureStarted) {
        & pktmon stop
        if ($LASTEXITCODE -ne 0) { Write-Warning 'Capture stop failed. Run pktmon stop from this administrator window.' }
    }
    if ($filterAdded) { & pktmon filter remove $filterName }
}

if (Test-Path -LiteralPath $etlFile) {
    & pktmon etl2pcap $etlFile --out $pcapFile
    if ($LASTEXITCODE -ne 0) { throw "Conversion failed; the original ETL is preserved: $etlFile" }
    Write-Host "Saved capture: $pcapFile"
    $latestSessionFile = Join-Path $sourceDirectory 'build-herelink-debug\logs\latest-session.txt'
    if (Test-Path -LiteralPath $latestSessionFile) {
        Get-Content -LiteralPath $latestSessionFile | Set-Content -LiteralPath (Join-Path $captureDirectory 'gcs-session.txt')
    }
}
