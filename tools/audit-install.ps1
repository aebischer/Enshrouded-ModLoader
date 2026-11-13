$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$target = Join-Path $root ('build\audit-install-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath 'D:\SteamLibrary\steamapps\common\Enshrouded\enshrouded.exe' -Destination $target
$installer = Join-Path $root 'install.ps1'
function Run-Installer([bool]$remove) {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $installer + '" -GamePath "' + $target + '"'
    if ($remove) { $arguments += ' -Uninstall' }
    $process = Start-Process powershell.exe -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $target 'stdout.txt') -RedirectStandardError (Join-Path $target 'stderr.txt')
    return $process.ExitCode
}
$dll = Join-Path $target 'dinput8.dll'
$receipt = Join-Path $target 'RandomWorld\installed.sha256'
Write-Output "Install exit: $(Run-Installer $false)"
Write-Output "Repeat exit: $(Run-Installer $false)"
'foreign sentinel' | Set-Content -LiteralPath $dll
$foreignHash = (Get-FileHash -LiteralPath $dll).Hash
Write-Output "Foreign install refused: $((Run-Installer $false) -ne 0)"
Write-Output "Foreign uninstall refused: $((Run-Installer $true) -ne 0)"
Write-Output "Foreign DLL preserved: $((Get-FileHash -LiteralPath $dll).Hash -eq $foreignHash)"
Copy-Item -LiteralPath (Join-Path $root 'dist\dinput8.dll') -Destination $dll
Write-Output "Uninstall exit: $(Run-Installer $true)"
# A non-writable receipt destination simulates failure after DLL replacement.
New-Item -ItemType Directory -Path $receipt | Out-Null
Write-Output "Receipt failure exit: $(Run-Installer $false)"
Write-Output "DLL remains after failed install: $(Test-Path -LiteralPath $dll -PathType Leaf)"
Write-Output "Valid receipt exists: $(Test-Path -LiteralPath $receipt -PathType Leaf)"
Write-Output "Retry refused: $((Run-Installer $false) -ne 0)"
Write-Output "Fixture: $target"
