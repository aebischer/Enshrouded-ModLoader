$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$target = Join-Path $root ('work/package-migration-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath 'D:\SteamLibrary\steamapps\common\Enshrouded\enshrouded.exe' -Destination $target
$legacy = Join-Path $target 'Mods'
$dedicated = Join-Path $legacy 'EnshroudedModLoader'
function Write-File([string]$path,[string]$value) {
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($path,$value,[Text.UTF8Encoding]::new($false))
}
function Package([string]$name) {
    Write-File (Join-Path $legacy "$name/$name.dll") 'non-executable package fixture'
    Write-File (Join-Path $legacy "$name/$name.dll.mod") "ENSHROUDED_PACKAGE_V1`nkind=mod`nversion=1.0`nabi=1`n"
}
function Install([bool]$pass,[bool]$remove=$false) {
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $root 'install.ps1')+'"'),'-GamePath',('"'+$target+'"'))
    if($remove) { $arguments += '-Uninstall' }
    $p=Start-Process powershell.exe -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $target 'out.txt') -RedirectStandardError (Join-Path $target 'err.txt')
    if(($p.ExitCode -eq 0) -ne $pass) { throw (Get-Content (Join-Path $target 'err.txt') -Raw) }
}
Package 'carryon'; Package 'lantern'
Write-File (Join-Path $legacy 'loose.dll') 'loose package fixture'
Write-File (Join-Path $legacy 'loose.dll.mod') "ENSHROUDED_PACKAGE_V1`r`nkind=mod`r`nversion=1.0`r`nabi=1`r`n"
Write-File (Join-Path $legacy 'lantern/equipment-markers/character.json') '{"saved":true}'
Write-File (Join-Path $legacy 'carryon/assets/nested/model.bin') 'asset sentinel'
Write-File (Join-Path $legacy 'OtherFramework/foreign.dll') 'foreign sentinel'
Write-File (Join-Path $legacy 'OtherFramework/foreign.dll.mod') 'OTHER_FORMAT'
$world = Join-Path $target 'ModLoader/worlds/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.mods'
$settings = Join-Path $target 'ModLoader/worlds/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.settings-v2/key.ini'
Write-File $world "ENSHROUDED_MODS_V1`ncarryon/carryon.dll`nlantern/lantern.dll`n"
Write-File $settings "inspect_objects=true`n"
$saved = @{}
foreach($file in Get-ChildItem -LiteralPath $target -Recurse -File) { $saved[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName).Hash }
Install $true
foreach($old in $saved.Keys) {
    $new=$old.Replace("ModLoader\worlds\","Mods\EnshroudedModLoader\worlds\")
    foreach($name in @('carryon','lantern')) { $new=$new.Replace((Join-Path $legacy "$name\"),(Join-Path $dedicated "$name\")) }
    foreach($name in @('loose.dll','loose.dll.mod')) { if($old -eq (Join-Path $legacy $name)) { $new=Join-Path $dedicated $name } }
    if((Get-FileHash -LiteralPath $new).Hash -ne $saved[$old]) { throw "Changed file: $new" }
}
if((Test-Path (Join-Path $legacy 'carryon')) -or (Test-Path (Join-Path $legacy 'lantern'))) { throw 'Legacy duplicates remain' }
Install $true
# A failure after migration must restore both packages and the old proxy/receipt.
$rollbackName='récupération'
Package $rollbackName
$receipt=Join-Path $target 'RandomWorld/installed.sha256'
$lock=[IO.File]::Open($receipt,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try { Install $false } finally { $lock.Dispose() }
if(-not (Test-Path (Join-Path $legacy "$rollbackName/$rollbackName.dll")) -or (Test-Path (Join-Path $dedicated $rollbackName))) { throw 'Package rollback failed' }
# Simulate a process stopping after a package move, before proxy publication.
$dll=Join-Path $target 'dinput8.dll'; $hash=(Get-FileHash $dll).Hash
Move-Item -LiteralPath (Join-Path $legacy $rollbackName) -Destination (Join-Path $dedicated $rollbackName)
Package 'notmovedyet'
@{oldHash=$hash;newHash=$hash;packageMoves=@($rollbackName,'notmovedyet')} | ConvertTo-Json | Set-Content (Join-Path $target 'RandomWorld/install.pending.json') -Encoding UTF8
Install $true $true
if(-not (Test-Path (Join-Path $legacy "$rollbackName/$rollbackName.dll")) -or (Test-Path (Join-Path $dedicated $rollbackName))) { throw 'Interrupted migration recovery failed' }
Install $true
# A mixed directory is never silently moved with another framework's runtime.
Package 'mixed'
Write-File (Join-Path $legacy 'mixed/foreign.dll') 'foreign'
Install $false
if(-not (Test-Path (Join-Path $legacy 'mixed/foreign.dll')) -or (Test-Path (Join-Path $dedicated 'mixed'))) { throw 'Mixed folder moved' }
# Remove our identifying header, leaving this foreign directory alone.
Write-File (Join-Path $legacy 'mixed/mixed.dll.mod') 'OTHER_FORMAT'
Package 'collision'
Write-File (Join-Path $dedicated 'collision/keep.txt') 'destination sentinel'
Install $false
if(-not (Test-Path (Join-Path $legacy 'collision/collision.dll')) -or (Get-Content (Join-Path $dedicated 'collision/keep.txt')) -ne 'destination sentinel') { throw 'Collision overwritten' }
foreach($path in @($world,$settings)) { if((Get-FileHash ($path.Replace("ModLoader\worlds\","Mods\EnshroudedModLoader\worlds\"))).Hash -ne $saved[$path]) { throw 'World settings changed' } }
Write-Output 'PASS: package migration, IDs/settings/assets/markers preserved, foreign packages ignored, reinstall, rollback, interrupted recovery, mixed-folder and collision refusal.'
