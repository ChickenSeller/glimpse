# Installs a Glimpse update; started by Glimpse (src/update/Updater.cpp) right
# before it quits. Unpacks the downloaded package, waits for Glimpse to exit,
# copies the new files over the old ones and starts Glimpse again (the old
# version, should anything fail). Problems are logged to
# %TEMP%\glimpse-update\update.log.
param(
    [Parameter(Mandatory)] [string] $Zip,
    [Parameter(Mandatory)] [string] $Target,
    [Parameter(Mandatory)] [int] $ProcessId,
    [Parameter(Mandatory)] [string] $Exe
)

$ErrorActionPreference = 'Stop'
$log = Join-Path (Split-Path $Zip) 'update.log'
$staging = Join-Path (Split-Path $Zip) ('staging-' + [guid]::NewGuid())

try {
    Expand-Archive -LiteralPath $Zip -DestinationPath $staging -Force
    # The package holds one folder, Glimpse-<version>-windows-x64.
    $root = Get-ChildItem -LiteralPath $staging -Directory | Select-Object -First 1
    if (-not $root -or -not (Test-Path (Join-Path $root.FullName 'glimpse.exe'))) {
        throw "Unexpected package layout in $Zip"
    }

    try { Wait-Process -Id $ProcessId -Timeout 60 } catch { }
    Start-Sleep -Milliseconds 500

    # Copies everything, retrying files still in use for a while; files the
    # new version no longer ships stay (harmless) rather than risk deleting
    # anything of the user's.
    robocopy $root.FullName $Target /E /R:10 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copying the new files failed (robocopy $LASTEXITCODE)" }

    Remove-Item -LiteralPath $Zip -Force
}
catch {
    "$(Get-Date -Format s) $_" | Out-File -FilePath $log -Append -Encoding utf8
}
finally {
    if (Test-Path $staging) { Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue }
}

Start-Process -FilePath $Exe
