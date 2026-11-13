$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$fixture=Join-Path $root ('build/install-format-069-'+[guid]::NewGuid().ToString('N'))
$source=Join-Path $fixture 'source'; $target=Join-Path $fixture 'game'
New-Item -ItemType Directory -Path (Join-Path $source 'dist'),$target | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'install.ps1') -Destination $source
$exe='D:/SteamLibrary/steamapps/common/Enshrouded/enshrouded.exe'
Copy-Item -LiteralPath $exe -Destination (Join-Path $target 'enshrouded.exe')
$dll=Join-Path $source 'dist/dinput8.dll'
$valid=[IO.File]::ReadAllBytes((Join-Path $root 'dist/dinput8.dll'))
[IO.File]::WriteAllBytes($dll,$valid)
& (Join-Path $source 'install.ps1') -GamePath $target
$installed=Join-Path $target 'dinput8.dll'; $receipt=Join-Path $target 'RandomWorld/installed.sha256'
$hash=(Get-FileHash $installed).Hash; $receiptHash=(Get-FileHash $receipt).Hash
$cases=@{}
$cases['renamed-exe']=[IO.File]::ReadAllBytes($exe)
$cases['plugin-not-loader']=[IO.File]::ReadAllBytes((Join-Path $root 'build/mod_test_plugin.dll'))
$cases['truncated']=[byte[]]$valid[0..127]
$bad=[byte[]]$valid.Clone();$pe=[BitConverter]::ToInt32($bad,60)
[BitConverter]::GetBytes([uint32]4294967280).CopyTo($bad,$pe+136)
$cases['invalid-export-rva']=$bad
foreach($case in $cases.Keys){
 [IO.File]::WriteAllBytes($dll,$cases[$case]);$refused=$false
 try {& (Join-Path $source 'install.ps1') -GamePath $target} catch {$refused=$true}
 if(-not $refused -or (Get-FileHash $installed).Hash -ne $hash -or (Get-FileHash $receipt).Hash -ne $receiptHash){throw "Invalid artifact accepted or installed files changed: $case"}
 Write-Output "PASS: $case refused; installed DLL and receipt preserved."
}

