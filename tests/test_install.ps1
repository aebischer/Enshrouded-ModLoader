$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$target = Join-Path $root ('work\install-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath 'D:\SteamLibrary\steamapps\common\Enshrouded\enshrouded.exe' -Destination $target
'save sentinel' | Set-Content -LiteralPath (Join-Path $target 'save.keep')
$installer = Join-Path $root 'install.ps1'
function Run-Install([bool]$remove, [bool]$shouldPass) {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $installer + '" -GamePath "' + $target + '"'
    if ($remove) { $arguments += ' -Uninstall' }
    $process = Start-Process powershell.exe -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $target 'stdout.txt') -RedirectStandardError (Join-Path $target 'stderr.txt')
    if (($process.ExitCode -eq 0) -ne $shouldPass) { throw "Unexpected installer exit: $($process.ExitCode) : $(Get-Content (Join-Path $target 'stderr.txt') -Raw)" }
}
Run-Install $false $true
Run-Install $false $true
$dll = Join-Path $target 'dinput8.dll'
if ((Get-FileHash $dll).Hash -ne (Get-FileHash (Join-Path $root 'dist\dinput8.dll')).Hash) { throw 'DLL differs' }
'foreign dll sentinel' | Set-Content -LiteralPath $dll
$foreignHash = (Get-FileHash $dll).Hash
Run-Install $false $false
Run-Install $true $false
if ((Get-FileHash $dll).Hash -ne $foreignHash) { throw 'Foreign DLL overwritten' }
Copy-Item -LiteralPath (Join-Path $root 'dist\dinput8.dll') -Destination $dll
Run-Install $true $true
if (Test-Path -LiteralPath $dll) { throw 'DLL still present' }
if ((Get-Content -LiteralPath (Join-Path $target 'save.keep')) -ne 'save sentinel') { throw 'Sentinel changed' }
# Failure before publishing the DLL: a receipt directory must leave no DLL.
$receipt = Join-Path $target 'RandomWorld\installed.sha256'
New-Item -ItemType Directory -Path $receipt | Out-Null
Run-Install $false $false
if (Test-Path -LiteralPath $dll) { throw 'Failed install left a DLL' }
[IO.Directory]::Delete($receipt)
Run-Install $false $true
# Use a recognisable previous build (valid PE plus harmless trailing fixture bytes).
$stream = [IO.File]::Open($dll, [IO.FileMode]::Append)
$stream.WriteByte(42); $stream.Dispose()
$oldHash = (Get-FileHash -LiteralPath $dll).Hash
$oldHash | Set-Content -LiteralPath $receipt -Encoding ASCII
# Force publication of the receipt to fail after DLL replacement.
$receiptLock = [IO.File]::Open($receipt, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
try { Run-Install $false $false } finally { $receiptLock.Dispose() }
if ((Get-FileHash -LiteralPath $dll).Hash -ne $oldHash) { throw 'DLL rollback failed' }
if ((Get-Content -LiteralPath $receipt -Raw).Trim() -ne $oldHash) { throw 'Receipt rollback failed' }
$pending = Join-Path $target 'RandomWorld\install.pending.json'
if (Test-Path -LiteralPath $pending) { throw 'Recovered transaction still pending' }
# Emulate interruption after DLL publication but before receipt publication.
Copy-Item -LiteralPath $dll -Destination (Join-Path $target 'RandomWorld\install.previous.dll') -Force
Copy-Item -LiteralPath (Join-Path $root 'dist\dinput8.dll') -Destination $dll -Force
@{oldHash=$oldHash; newHash=(Get-FileHash -LiteralPath $dll).Hash} | ConvertTo-Json | Set-Content -LiteralPath $pending -Encoding ASCII
Run-Install $true $true
if ((Test-Path -LiteralPath $dll) -or (Test-Path -LiteralPath $pending)) { throw 'Interrupted transaction recovery failed' }
if ((Get-Content -LiteralPath (Join-Path $target 'save.keep')) -ne 'save sentinel') { throw 'Sentinel changed' }
Write-Output 'PASS: install/reinstall, foreign DLL, uninstall, receipt failure before/after publish, rollback, interrupted recovery, saves preserved.'
