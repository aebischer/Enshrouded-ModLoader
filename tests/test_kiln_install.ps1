$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$area=Join-Path $root ('work/unified-install-'+[guid]::NewGuid().ToString('N'))
$source=Join-Path $area 'source';$game=Join-Path $area 'game'
New-Item -ItemType Directory -Path (Join-Path $source 'dist'),$game | Out-Null
Copy-Item (Join-Path $root 'install.ps1') $source
Copy-Item (Join-Path $root 'dist/dinput8.dll') (Join-Path $source 'dist/dinput8.dll')
Copy-Item 'D:/SteamLibrary/steamapps/common/Enshrouded/enshrouded.exe' $game
function Run([bool]$pass,[bool]$remove=$false) {
 $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $source 'install.ps1')+'"'),'-GamePath',('"'+$game+'"'))
 if($remove){$args+='-Uninstall'}
 $p=Start-Process powershell.exe -ArgumentList $args -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $area 'out.txt') -RedirectStandardError (Join-Path $area 'err.txt')
 if(($p.ExitCode -eq 0) -ne $pass){throw "Unexpected installer exit $($p.ExitCode): $(Get-Content (Join-Path $area 'err.txt') -Raw)"}
}
function Put([string]$relative,[string]$text){$p=Join-Path $game $relative;New-Item -ItemType Directory -Path (Split-Path $p -Parent) -Force | Out-Null;[IO.File]::WriteAllText($p,$text)}
function Hash([string]$relative){return (Get-FileHash -LiteralPath (Join-Path $game $relative)).Hash}
Put 'ModLoader/worlds/test.mods' 'profile sentinel'
Put 'ModLoader/worlds/test.settings-v2/carryon.ini' 'settings sentinel'
Put 'Mods/EnshroudedModLoader/lantern/equipment-markers/player.json' 'equipment sentinel'
$keep=@{};foreach($file in Get-ChildItem $game -File -Recurse){$keep[$file.FullName]=(Get-FileHash $file.FullName).Hash}
Run $true
$standalone=Hash 'dinput8.dll'
Put 'kiln/chain/foreign.dll' 'foreign chain sentinel'
Move-Item (Join-Path $game 'dinput8.dll') (Join-Path $game 'kiln/chain/unknown-dinput8.dll')
Copy-Item (Join-Path $root 'build/kiln-test/dinput8.dll') (Join-Path $game 'dinput8.dll')
$proxy=Hash 'dinput8.dll'
Put 'kiln/chain.ini' "[chain]`r`nchain\foreign.dll`r`nchain\unknown-dinput8.dll`r`n[late]`r`nchain\unknown-dinput8.dll`r`n"
Put 'Mods/legacy/legacy.dll' 'legacy package'
Put 'Mods/legacy/legacy.dll.mod' "ENSHROUDED_PACKAGE_V1`nkind=mod`nversion=1.0`nabi=1`n"
Run $true
$native='kiln/native/enshrouded-modloader/modloader.dll'
$manifest='kiln/native/enshrouded-modloader/native.json'
if((Hash $native) -ne $standalone -or (Hash 'dinput8.dll') -ne $proxy){throw 'One-binary installation/proxy preservation failed'}
if(Test-Path (Join-Path $game 'kiln/chain/unknown-dinput8.dll')){throw 'Old chained loader remains'}
$ini=Get-Content (Join-Path $game 'kiln/chain.ini') -Raw
if($ini -match 'unknown-dinput8' -or $ini -notmatch 'foreign.dll'){throw 'Chain migration failed'}
if(!(Test-Path (Join-Path $game 'Mods/EnshroudedModLoader/legacy/legacy.dll'))){throw 'Legacy mod not migrated'}
$originalNative=[IO.File]::ReadAllBytes((Join-Path $game $native))
Put $native 'foreign replacement'
Run $false
if((Get-Content (Join-Path $game $native) -Raw) -ne 'foreign replacement'){throw 'Modified native overwritten'}
[IO.File]::WriteAllBytes((Join-Path $game $native),$originalNative)
Put 'kiln/native/enshrouded-modloader/.disabled' 'user disabled'
Run $true
if((Get-Content (Join-Path $game 'kiln/native/enshrouded-modloader/.disabled') -Raw) -ne 'user disabled'){throw 'Disabled policy changed'}
$actual=Get-Content (Join-Path $game $manifest) -Raw | ConvertFrom-Json
$expected=Get-Content (Join-Path $root 'assets/kiln/native.json') -Raw | ConvertFrom-Json
foreach($key in @('id','version','side','phase','init')){if($actual.$key -ne $expected.$key){throw 'Manifest contract mismatch'}}
$initialBackup=Get-ChildItem (Join-Path $game 'RandomWorld') -Filter 'dual-install.backup.*.json' | Select-Object -First 1
Run $true
# A recovery record must not escape its narrowly allowed chain filenames.
$bad=Get-Content $initialBackup.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
$bad.files[0].path='kiln/chain/..\..\dinput8.dll'
$bad | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $game 'RandomWorld/dual-install.pending.json') -Encoding UTF8
Run $false
if((Hash 'dinput8.dll') -ne $proxy -or (Hash $native) -ne $standalone){throw 'Invalid recovery journal changed files'}
Remove-Item -LiteralPath (Join-Path $game 'RandomWorld/dual-install.pending.json')
# Recovery after interrupted publication restores the transaction before retry.
Copy-Item $initialBackup.FullName (Join-Path $game 'RandomWorld/dual-install.pending.json')
Run $true
if((Hash $native) -ne $standalone -or (Hash 'dinput8.dll') -ne $proxy){throw 'Recovery failed'}
# Force failure after publishing the new DLL: locked manifest must roll back DLL.
$sourceDll=Join-Path $source 'dist/dinput8.dll'
$stream=[IO.File]::Open($sourceDll,[IO.FileMode]::Append);$stream.WriteByte(42);$stream.Dispose()
$locked=[IO.File]::Open((Join-Path $game $manifest),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
# Altered valid source changes only the DLL; lock the receipt, which is written last.
$receiptLock=[IO.File]::Open((Join-Path $game 'RandomWorld/kiln-installed.json'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {Run $false} finally {$receiptLock.Dispose();$locked.Dispose()}
if((Hash $native) -ne $standalone){throw 'Native update rollback failed'}
Run $true
if((Hash $native) -ne (Get-FileHash $sourceDll).Hash){throw 'Update failed'}
# Uninstall native mode must never remove Kiln or gameplay packages.
Run $true $true
if(Test-Path (Join-Path $game $native)){throw 'Native DLL remains'}
if((Hash 'dinput8.dll') -ne $proxy){throw 'Kiln proxy changed'}
Run $true
# Removing Kiln then rerunning installer moves back to standalone automatically.
Remove-Item -LiteralPath (Join-Path $game 'dinput8.dll')
Run $true
if(Test-Path (Join-Path $game $native)){throw 'Native duplicate after standalone switch'}
if((Hash 'dinput8.dll') -ne (Get-FileHash $sourceDll).Hash){throw 'Standalone switch failed'}
foreach($file in $keep.Keys){if((Get-FileHash $file).Hash -ne $keep[$file]){throw "Preserved file changed: $file"}}
if((Get-Content (Join-Path $game 'kiln/chain/foreign.dll') -Raw) -ne 'foreign chain sentinel'){throw 'Foreign loader changed'}
Put 'dinput8.dll' 'foreign proxy'
Run $false
if((Get-Content (Join-Path $game 'dinput8.dll') -Raw) -ne 'foreign proxy'){throw 'Foreign proxy overwritten'}
Write-Output 'PASS: automatic mode selection, identical DLL, legacy chain retirement, package/settings preservation, idempotence, interrupted recovery, failed update rollback, native uninstall and return to standalone.'
